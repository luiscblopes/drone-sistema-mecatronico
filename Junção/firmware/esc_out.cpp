/**
 * @file esc_out.cpp
 * @brief Implementação da saída de PWM dos ESCs (ESP32Servo). CAMINHO CONGELADO.
 */
#include "esc_out.h"
#include "config.h"

#include <Arduino.h>
#include <ESP32Servo.h>

/** Objetos de PWM (um por motor). */
static Servo s_motor[NUM_MOTORS];
/** Pinos dos motores, na ordem fisica M1..M4 (contrato config.h). */
static const int s_pin[NUM_MOTORS] = { ESC_GPIO_M1, ESC_GPIO_M2, ESC_GPIO_M3, ESC_GPIO_M4 };
/** Ultimo pulso escrito por motor (us), para telemetria. */
static int32_t s_last_us[NUM_MOTORS];
/** Calibracao por motor. */
static int32_t s_start_us[NUM_MOTORS] = DEFAULT_MOTOR_START_US;
static int32_t s_max_us[NUM_MOTORS]   = DEFAULT_MOTOR_MAX_US;
static int32_t s_trim_us[NUM_MOTORS]  = DEFAULT_MOTOR_TRIM_US;
/** Instante (ms) em que o arming comecou. */
static uint32_t s_arm_start_ms = 0U;

/** Spinlock que serializa o acesso ao ESC entre os dois nucleos (tarefa de
 * controle no core 1 e servidor web no core 0). NAO e recursivo: nunca chamar
 * uma funcao que o adquire enquanto ja se esta dentro de uma secao critica. */
static portMUX_TYPE s_esc_mux = portMUX_INITIALIZER_UNLOCKED;

/** @brief Satura um inteiro ao intervalo [lo, hi]. */
static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

/**
 * @brief Escreve um pulso no motor SEM travar (assume secao critica ja tomada).
 *
 * Helper interno para evitar travamento recursivo do spinlock: as funcoes
 * publicas adquirem o mux uma vez e chamam este helper.
 */
static void write_motor_locked(uint8_t motor, int32_t pulse_us)
{
    const int32_t out = clamp_i32(pulse_us + s_trim_us[motor], ESC_MIN_US, MOTOR_OUTPUT_MAX_US);
    s_motor[motor].writeMicroseconds(out);
    s_last_us[motor] = out;
}

void esc_init(void)
{
    /* O ESP32Servo precisa de timers de LEDC alocados antes do attach. */
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);

    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        s_motor[i].setPeriodHertz(ESC_FREQUENCY_HZ);          /* 250 Hz validado nesta placa. */
        s_motor[i].attach(s_pin[i], ESC_MIN_US, ESC_MAX_US);  /* faixa 1000..2000 us. */
        s_motor[i].writeMicroseconds(ESC_MIN_US);             /* arming: comeca no minimo. */
        s_last_us[i] = ESC_MIN_US;
    }
    s_arm_start_ms = millis();
}

bool esc_is_arming(void)
{
    return (millis() - s_arm_start_ms) < ESC_ARM_HOLD_MS;
}

void esc_set_motor_us(uint8_t motor, int32_t pulse_us)
{
    if (motor >= NUM_MOTORS) { return; }
    /* Soma o trim e satura a faixa segura (teto 1999 evita "full" acidental). */
    portENTER_CRITICAL(&s_esc_mux);
    write_motor_locked(motor, pulse_us);
    portEXIT_CRITICAL(&s_esc_mux);
}

int32_t esc_get_motor_us(uint8_t motor)
{
    if (motor >= NUM_MOTORS) { return ESC_MIN_US; }
    portENTER_CRITICAL(&s_esc_mux);
    const int32_t v = s_last_us[motor];
    portEXIT_CRITICAL(&s_esc_mux);
    return v;
}

void esc_set_motor_speed_pct(uint8_t motor, int32_t percent)
{
    if (motor >= NUM_MOTORS) { return; }
    percent = clamp_i32(percent, 0, 100);
    /* Mapeia 0..100% para [start, max] da calibracao do motor (sob o lock, para
     * ler start/max e escrever de forma atomica). */
    portENTER_CRITICAL(&s_esc_mux);
    const int32_t span = s_max_us[motor] - s_start_us[motor];
    const int32_t us = (percent == 0) ? ESC_MIN_US : (s_start_us[motor] + (percent * span) / 100);
    write_motor_locked(motor, us);
    portEXIT_CRITICAL(&s_esc_mux);
}

int32_t esc_get_motor_speed_pct(uint8_t motor)
{
    if (motor >= NUM_MOTORS) { return 0; }
    portENTER_CRITICAL(&s_esc_mux);
    const int32_t span = s_max_us[motor] - s_start_us[motor];
    const int32_t last = s_last_us[motor];
    const int32_t start = s_start_us[motor];
    portEXIT_CRITICAL(&s_esc_mux);
    if (span <= 0) { return 0; }
    const int32_t pct = ((last - start) * 100) / span;
    return clamp_i32(pct, 0, 100);
}

void esc_stop_all(void)
{
    portENTER_CRITICAL(&s_esc_mux);
    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        s_motor[i].writeMicroseconds(ESC_MIN_US);
        s_last_us[i] = ESC_MIN_US;
    }
    portEXIT_CRITICAL(&s_esc_mux);
}

void esc_set_calibration(uint8_t motor, int32_t start_us, int32_t max_us, int32_t trim_us)
{
    if (motor >= NUM_MOTORS) { return; }
    portENTER_CRITICAL(&s_esc_mux);
    s_start_us[motor] = clamp_i32(start_us, ESC_MIN_US, ESC_MAX_US);
    s_max_us[motor]   = clamp_i32(max_us, ESC_MIN_US, ESC_MAX_US);
    s_trim_us[motor]  = clamp_i32(trim_us, MOTOR_TRIM_MIN_US, MOTOR_TRIM_MAX_US);
    portEXIT_CRITICAL(&s_esc_mux);
}

void esc_get_calibration(uint8_t motor, int32_t *start_us, int32_t *max_us, int32_t *trim_us)
{
    if (motor >= NUM_MOTORS) { return; }
    portENTER_CRITICAL(&s_esc_mux);
    const int32_t s = s_start_us[motor];
    const int32_t m = s_max_us[motor];
    const int32_t t = s_trim_us[motor];
    portEXIT_CRITICAL(&s_esc_mux);
    if (start_us != NULL) { *start_us = s; }
    if (max_us != NULL)   { *max_us = m; }
    if (trim_us != NULL)  { *trim_us = t; }
}

void esc_reset_calibration(void)
{
    const int32_t def_start[NUM_MOTORS] = DEFAULT_MOTOR_START_US;
    const int32_t def_max[NUM_MOTORS]   = DEFAULT_MOTOR_MAX_US;
    const int32_t def_trim[NUM_MOTORS]  = DEFAULT_MOTOR_TRIM_US;
    portENTER_CRITICAL(&s_esc_mux);
    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        s_start_us[i] = def_start[i];
        s_max_us[i]   = def_max[i];
        s_trim_us[i]  = def_trim[i];
    }
    portEXIT_CRITICAL(&s_esc_mux);
}
