/**
 * @file flight_task.cpp
 * @brief Implementação da tarefa de controle a 250 Hz. CAMINHO CONGELADO.
 */
#include "flight_task.h"
#include "config.h"
#include "imu_mpu9250.h"
#include "attitude_filter.h"
#include "control_cascade.h"
#include "motor_mix.h"
#include "esc_out.h"
#include "failsafe.h"
#include "command_state.h"

#include <Arduino.h>

static TaskHandle_t s_task = NULL;
/** Instante (ms) da ultima amostra de IMU valida (para idade/validade). */
static uint32_t s_last_valid_imu_ms = 0U;
/** Ultima atitude valida estimada (mantida quando uma leitura falha). */
static attitude_t s_att = {0};

/**
 * @brief Publica o instantaneo de telemetria do ciclo corrente.
 */
static void publish_telemetry(const attitude_t *att, bool imu_valid, uint32_t imu_age)
{
    command_telemetry_t t;
    t.roll_deg = att->roll_deg;
    t.pitch_deg = att->pitch_deg;
    t.rate_roll_dps = att->rate_roll_dps;
    t.rate_pitch_dps = att->rate_pitch_dps;
    t.rate_yaw_dps = att->rate_yaw_dps;
    for (uint8_t i = 0U; i < NUM_MOTORS; i++) { t.motor_us[i] = esc_get_motor_us(i); }
    t.imu_valid = imu_valid;
    t.imu_age_ms = imu_age;
    t.enabled = command_state_is_enabled();
    t.arming = esc_is_arming();
    t.failsafe_reason = (int)failsafe_reason();
    t.latched = failsafe_is_latched();
    command_state_publish_telemetry(&t);
}

/**
 * @brief Corpo da tarefa: laço periódico de 4 ms (250 Hz) com vTaskDelayUntil.
 */
static void flight_task_run(void *arg)
{
    (void)arg;
    const TickType_t period = pdMS_TO_TICKS(1000U / CONTROL_LOOP_HZ);
    TickType_t last_wake = xTaskGetTickCount();
    s_last_valid_imu_ms = millis();

    for (;;)
    {
        /* 1) Sensor: le a IMU e atualiza a estimativa de atitude. */
        imu_sample_t sample;
        imu_read(&sample);
        if (sample.valid) { s_last_valid_imu_ms = sample.timestamp_ms; }
        const uint32_t imu_age = millis() - s_last_valid_imu_ms;
        const bool imu_valid = sample.valid && (imu_age <= MPU_MAX_AGE_MS);

        /* Atualiza a estimativa SO com amostra valida; numa falha, mantem a ultima
         * atitude (e o failsafe IMU_INVALID atua pela idade). */
        if (sample.valid) { attitude_filter_update(&sample, &s_att); }

        /* 2) Atuacao: somente com a malha habilitada e fora do arming. */
        if (command_state_is_enabled() && !esc_is_arming())
        {
            /* 2a) Avalia failsafes (perda de comando, IMU, tilt). */
            failsafe_input_t fin;
            fin.command_age_ms = command_state_command_age_ms();
            fin.imu_valid = imu_valid;
            fin.imu_calibrated = true; /* offsets carregados; calibracao assumida valida. */
            fin.roll_deg = s_att.roll_deg;
            fin.pitch_deg = s_att.pitch_deg;

            if (!failsafe_evaluate(&fin))
            {
                /* Condicao perigosa: corta, desabilita e (failsafe ja travou o latch). */
                esc_stop_all();
                command_state_set_enabled(false);
                control_cascade_reset();
                attitude_filter_reset();
            }
            else
            {
                /* 2b) Caminho normal: cascata -> mixagem -> ESC. */
                command_setpoint_t sp;
                command_state_get_setpoint(&sp);

                if (sp.throttle_us < (float)THROTTLE_ARM_MIN_US)
                {
                    /* Throttle muito baixo: nao corrige sem empuxo. Motores ao corte
                     * e reset dos termos (espelha o sketch). */
                    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
                    {
                        esc_set_motor_us(i, THROTTLE_CUTOFF_US);
                    }
                    control_cascade_reset();
                }
                else
                {
                    attitude_setpoint_t asp;
                    asp.roll_angle_deg = sp.roll_angle_deg;
                    asp.pitch_angle_deg = sp.pitch_angle_deg;
                    asp.yaw_rate_dps = sp.yaw_rate_dps;

                    control_output_t co;
                    control_cascade_step(&asp, &s_att, &co);

                    /* Teto do throttle comandado (espelha o sketch). */
                    float throttle = sp.throttle_us;
                    if (throttle > (float)THROTTLE_MAX_CMD_US) { throttle = (float)THROTTLE_MAX_CMD_US; }

                    motor_output_t mo;
                    motor_mix_compute(throttle, &co, &mo);
                    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
                    {
                        esc_set_motor_us(i, mo.us[i]);
                    }
                }
            }
        }
        /* Quando desabilitada, a tarefa NAO toca nos motores (deixa as rotas de
         * bancada /setMotor e /findDeadband atuarem). */

        /* 3) Telemetria do ciclo. */
        publish_telemetry(&s_att, imu_valid, imu_age);

        /* 4) Espera deterministica ate o proximo periodo (4 ms). */
        vTaskDelayUntil(&last_wake, period);
    }
}

void flight_task_init(void)
{
    attitude_filter_reset();
    control_cascade_init();
    failsafe_init();
}

bool flight_task_start(void)
{
    const BaseType_t ok = xTaskCreatePinnedToCore(
        flight_task_run, "flight", FLIGHT_TASK_STACK, NULL,
        FLIGHT_TASK_PRIO, &s_task, FLIGHT_TASK_CORE);
    return (ok == pdPASS);
}

bool flight_task_running(void)
{
    return (s_task != NULL);
}

void flight_task_engage(void)
{
    if (failsafe_is_latched()) { return; }
    control_cascade_reset();
    attitude_filter_reset();
    command_state_set_enabled(true);
}

void flight_task_disable(int reason, bool stop_motors)
{
    failsafe_trip((failsafe_reason_t)reason);
    command_state_set_enabled(false);
    if (stop_motors) { esc_stop_all(); }
}
