/**
 * @file web_routes.cpp
 * @brief Implementação do servidor HTTP e rotas (contrato Drone-main).
 */
#include "web_routes.h"
#include "config.h"
#include "index_html.h"
#include "esc_out.h"
#include "imu_mpu9250.h"
#include "command_state.h"
#include "flight_task.h"
#include "failsafe.h"
#include "control_cascade.h"
#include "nvs_store.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <stdarg.h>

static WebServer s_server(WEB_SERVER_PORT);
static TaskHandle_t s_task = NULL;

/* ===== Helpers ===== */

/** @brief Le um argumento float da query, ou retorna o default se ausente. */
static float arg_f(const char *name, float def)
{
    return s_server.hasArg(name) ? s_server.arg(name).toFloat() : def;
}

/** @brief Le um argumento int da query, ou retorna o default se ausente. */
static int32_t arg_i(const char *name, int32_t def)
{
    return s_server.hasArg(name) ? (int32_t)s_server.arg(name).toInt() : def;
}

/** @brief Satura um float a [lo, hi]. */
static float clamp_f(float v, float lo, float hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

/** @brief Responde texto curto com um codigo de status. */
static void send_text(int code, const char *msg)
{
    s_server.send(code, "text/plain; charset=utf-8", msg);
}

/** @brief Responde um JSON ja montado. */
static void send_json(const char *json)
{
    s_server.sendHeader("Cache-Control", "no-store");
    s_server.send(200, "application/json", json);
}

/**
 * @brief Acrescenta texto formatado ao buffer com seguranca.
 *
 * Evita o underflow de (cap - off) quando off ja atingiu cap: nesse caso nao
 * escreve nada. Mantem *off <= cap. Substitui o padrao "off += snprintf(buf+off,
 * cap-off, ...)", que teria comportamento indefinido se off ultrapassasse cap.
 */
static void json_append(char *buf, size_t cap, size_t *off, const char *fmt, ...)
{
    if (*off >= cap) { return; }
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (n > 0)
    {
        *off += (size_t)n;
        if (*off > cap) { *off = cap; } /* trunca: nao deixa o offset passar do fim */
    }
}

/** @brief Monta o JSON de calibracao (compartilhado pelas 3 rotas de calibracao). */
static void respond_calibration(void)
{
    char buf[384];
    size_t off = 0U;
    const size_t cap = sizeof(buf);
    json_append(buf, cap, &off, "{\"start\":[");
    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        int32_t s, m, t;
        esc_get_calibration(i, &s, &m, &t);
        json_append(buf, cap, &off, "%ld%s", (long)s, (i < NUM_MOTORS - 1) ? "," : "");
    }
    json_append(buf, cap, &off, "],\"max\":[");
    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        int32_t s, m, t;
        esc_get_calibration(i, &s, &m, &t);
        json_append(buf, cap, &off, "%ld%s", (long)m, (i < NUM_MOTORS - 1) ? "," : "");
    }
    json_append(buf, cap, &off, "],\"trim\":[");
    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        int32_t s, m, t;
        esc_get_calibration(i, &s, &m, &t);
        json_append(buf, cap, &off, "%ld%s", (long)t, (i < NUM_MOTORS - 1) ? "," : "");
    }
    json_append(buf, cap, &off, "],\"outputs\":[");
    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        json_append(buf, cap, &off, "%ld%s", (long)esc_get_motor_us(i),
                    (i < NUM_MOTORS - 1) ? "," : "");
    }
    json_append(buf, cap, &off, "],\"arming\":%s}", esc_is_arming() ? "true" : "false");
    send_json(buf);
}

/* ===== Rotas: página e telemetria ===== */

/** @brief GET / : serve o console (index.html embutido), sem cache. */
static void h_root(void)
{
    s_server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    s_server.send_P(200, "text/html; charset=utf-8", index_html);
}

/** @brief GET /health : diagnostico (heap, tarefas, motores no minimo). */
static void h_health(void)
{
    bool motors_min = true;
    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        if (esc_get_motor_us(i) != ESC_MIN_US) { motors_min = false; }
    }
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"ok\":true,\"heap\":%u,\"flightTask\":%s,\"webTask\":%s,\"motorsAtMinimum\":%s}",
             (unsigned)ESP.getFreeHeap(),
             flight_task_running() ? "true" : "false",
             web_routes_running() ? "true" : "false",
             motors_min ? "true" : "false");
    send_json(buf);
}

/** @brief GET /status : velocidades/saidas e estado do controle. */
static void h_status(void)
{
    command_telemetry_t t;
    command_state_get_telemetry(&t);
    char buf[320];
    size_t off = 0U;
    const size_t cap = sizeof(buf);
    json_append(buf, cap, &off, "{\"speeds\":[");
    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        json_append(buf, cap, &off, "%ld%s", (long)esc_get_motor_speed_pct(i),
                    (i < NUM_MOTORS - 1) ? "," : "");
    }
    json_append(buf, cap, &off, "],\"outputs\":[");
    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        json_append(buf, cap, &off, "%ld%s", (long)t.motor_us[i],
                    (i < NUM_MOTORS - 1) ? "," : "");
    }
    json_append(buf, cap, &off,
                "],\"arming\":%s,\"flightControlEnabled\":%s,\"failsafe\":\"%s\"}",
                t.arming ? "true" : "false", t.enabled ? "true" : "false",
                failsafe_text((failsafe_reason_t)t.failsafe_reason));
    send_json(buf);
}

/** @brief GET /sensors : telemetria de atitude/IMU. */
static void h_sensors(void)
{
    command_telemetry_t t;
    command_state_get_telemetry(&t);
    char buf[320];
    snprintf(buf, sizeof(buf),
             "{\"roll\":%.2f,\"pitch\":%.2f,\"rateRoll\":%.2f,\"ratePitch\":%.2f,"
             "\"rateYaw\":%.2f,\"imuValid\":%s,\"imuAgeMs\":%u}",
             t.roll_deg, t.pitch_deg, t.rate_roll_dps, t.rate_pitch_dps, t.rate_yaw_dps,
             t.imu_valid ? "true" : "false", (unsigned)t.imu_age_ms);
    send_json(buf);
}

/** @brief GET /flightStatus : estado completo da malha (setpoint+estado+failsafe). */
static void h_flight_status(void)
{
    command_telemetry_t t;
    command_state_get_telemetry(&t);
    command_setpoint_t sp;
    command_state_get_setpoint(&sp);
    char buf[420];
    snprintf(buf, sizeof(buf),
             "{\"setpoint\":{\"throttle\":%.0f,\"roll\":%.2f,\"pitch\":%.2f,\"yawRate\":%.2f},"
             "\"state\":{\"roll\":%.2f,\"pitch\":%.2f,\"rateRoll\":%.2f,\"ratePitch\":%.2f,\"rateYaw\":%.2f},"
             "\"outputs\":[%ld,%ld,%ld,%ld],"
             "\"enabled\":%s,\"arming\":%s,\"failsafe\":\"%s\",\"latched\":%s,\"imuValid\":%s}",
             sp.throttle_us, sp.roll_angle_deg, sp.pitch_angle_deg, sp.yaw_rate_dps,
             t.roll_deg, t.pitch_deg, t.rate_roll_dps, t.rate_pitch_dps, t.rate_yaw_dps,
             (long)t.motor_us[0], (long)t.motor_us[1], (long)t.motor_us[2], (long)t.motor_us[3],
             t.enabled ? "true" : "false", t.arming ? "true" : "false",
             failsafe_text((failsafe_reason_t)t.failsafe_reason),
             t.latched ? "true" : "false", t.imu_valid ? "true" : "false");
    send_json(buf);
}

/* ===== Rotas: comando de voo ===== */

/**
 * @brief GET /setFlight : recebe setpoints/ganhos e comanda a malha.
 *
 * Satura os parametros, atualiza o setpoint, marca o watchdog e:
 *  - apply=1 & stabilize=1: engata a malha (503 se a tarefa nao roda; 409 se latch);
 *  - caso contrario: desabilita a malha estabilizada (override manual seguro).
 */
static void h_set_flight(void)
{
    if (esc_is_arming())
    {
        send_text(423, "Motores ainda armando");
        return;
    }

    command_setpoint_t sp;
    command_state_get_setpoint(&sp);

    /* Saturacao dos setpoints (envelope comprovado ±20°; yaw em taxa). */
    sp.throttle_us     = clamp_f(arg_f("throttle", sp.throttle_us), (float)ESC_MIN_US, (float)ESC_MAX_US);
    sp.roll_angle_deg  = clamp_f(arg_f("rollSp", sp.roll_angle_deg), -SETPOINT_ANGLE_MAX_DEG, SETPOINT_ANGLE_MAX_DEG);
    sp.pitch_angle_deg = clamp_f(arg_f("pitchSp", sp.pitch_angle_deg), -SETPOINT_ANGLE_MAX_DEG, SETPOINT_ANGLE_MAX_DEG);
    sp.yaw_rate_dps    = clamp_f(arg_f("yawSp", sp.yaw_rate_dps), -200.0f, 200.0f);
    command_state_set_setpoint(&sp);

    /* Sintonia de ganhos de taxa em runtime (opcional). [US2] */
    for (uint8_t axis = 0U; axis < 3U; axis++)
    {
        const char *pk = (axis == 0) ? "rollKp" : (axis == 1) ? "pitchKp" : "yawKp";
        const char *pi = (axis == 0) ? "rollKi" : (axis == 1) ? "pitchKi" : "yawKi";
        const char *pd = (axis == 0) ? "rollKd" : (axis == 1) ? "pitchKd" : "yawKd";
        if (s_server.hasArg(pk) || s_server.hasArg(pi) || s_server.hasArg(pd))
        {
            pid_gains_t g;
            control_cascade_get_rate_gains(axis, &g);
            g.kp = arg_f(pk, g.kp);
            g.ki = arg_f(pi, g.ki);
            g.kd = arg_f(pd, g.kd);
            control_cascade_set_rate_gains(axis, &g);
        }
    }

    command_state_mark_command(); /* reinicia o watchdog de perda de comando */

    const bool apply = (arg_i("apply", 0) == 1);
    const bool stabilize = (arg_i("stabilize", 0) == 1);

    if (apply && stabilize)
    {
        if (!flight_task_running())
        {
            send_text(503, "Tarefa de controle indisponivel");
            return;
        }
        if (failsafe_is_latched())
        {
            send_text(409, "Failsafe ativo; execute PARAR TUDO antes de reabilitar");
            return;
        }
        command_state_request_stabilize(true);
        flight_task_engage();
    }
    else
    {
        /* Sem estabilizar: garante a malha desabilitada (override manual seguro). */
        command_state_request_stabilize(false);
        if (command_state_is_enabled())
        {
            flight_task_disable((int)FS_MANUAL_OVERRIDE, false);
        }
    }
    send_text(200, "OK");
}

/** @brief GET /resetPid : zera os controladores (cascata). */
static void h_reset_pid(void)
{
    control_cascade_reset();
    send_text(200, "PID reiniciado");
}

/** @brief GET /stopAll : parada de emergencia + limpa o latch (destrava). */
static void h_stop_all(void)
{
    if (esc_is_arming())
    {
        send_text(423, "Motores ainda armando");
        return;
    }
    flight_task_disable((int)FS_EMERGENCY_STOP, true);
    esc_stop_all();
    failsafe_clear_latch();
    send_text(200, "Todos parados");
}

/* ===== Rotas: bancada (motor / deadband) ===== */

/** @brief GET /setMotor?motor=&speed= : aciona um motor (override manual). */
static void h_set_motor(void)
{
    if (esc_is_arming())
    {
        send_text(423, "Motores ainda armando");
        return;
    }
    if (!s_server.hasArg("motor") || !s_server.hasArg("speed"))
    {
        send_text(400, "Parametros faltando");
        return;
    }
    const int32_t motor = arg_i("motor", 0);
    const int32_t speed = arg_i("speed", 0);
    /* Desabilita a malha (override manual; nao trava o latch). */
    flight_task_disable((int)FS_MANUAL_OVERRIDE, false);
    if ((motor >= 0) && (motor < NUM_MOTORS))
    {
        esc_set_motor_speed_pct((uint8_t)motor, speed);
    }
    send_text(200, "OK");
}

/** @brief GET /findDeadband?motor=&from=&to=&step=&dwell= : varredura de deadband. */
static void h_find_deadband(void)
{
    if (esc_is_arming())
    {
        send_text(423, "Motores ainda armando");
        return;
    }
    const int32_t motor = arg_i("motor", 0);
    const int32_t from = arg_i("from", ESC_MIN_US);
    const int32_t to = arg_i("to", 1250);
    const int32_t step = (arg_i("step", 2) <= 0) ? 2 : arg_i("step", 2);
    const int32_t dwell = arg_i("dwell", 300);
    if ((motor < 0) || (motor >= NUM_MOTORS))
    {
        send_text(400, "Motor invalido");
        return;
    }
    flight_task_disable((int)FS_MANUAL_OVERRIDE, false);
    for (int32_t us = from; us <= to; us += step)
    {
        esc_set_motor_us((uint8_t)motor, us);
        delay((uint32_t)((dwell > 0) ? dwell : 0));
    }
    esc_set_motor_us((uint8_t)motor, ESC_MIN_US);
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"motor\":%ld,\"from\":%ld,\"to\":%ld,\"done\":true}",
             (long)motor, (long)from, (long)to);
    send_json(buf);
}

/* ===== Rotas: calibração (NVS) ===== */

/** @brief GET /calibration : le a calibracao atual. */
static void h_calibration(void)
{
    respond_calibration();
}

/** @brief GET /setCalibration?startN=&maxN=&trimN= : grava e persiste. */
static void h_set_calibration(void)
{
    if (esc_is_arming())
    {
        send_text(423, "Motores ainda armando");
        return;
    }
    flight_task_disable((int)FS_MANUAL_OVERRIDE, false);
    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        int32_t s, m, t;
        esc_get_calibration(i, &s, &m, &t);
        char key[12];
        snprintf(key, sizeof(key), "start%u", (unsigned)i);
        s = arg_i(key, s);
        snprintf(key, sizeof(key), "max%u", (unsigned)i);
        m = arg_i(key, m);
        snprintf(key, sizeof(key), "trim%u", (unsigned)i);
        t = arg_i(key, t);
        esc_set_calibration(i, s, m, t);
    }
    nvs_store_save();
    respond_calibration();
}

/**
 * @brief GET /setImuOffset?gx=&gy=&gz=&ax=&ay=&az= : grava offsets da IMU.
 *
 * Substitui os offsets hardcoded do sketch por valores ajustaveis e persistidos.
 * Feito por valores explicitos (e nao por auto-calibracao concorrente) para nao
 * disputar o barramento I2C com a tarefa de controle (que le a IMU a 250 Hz).
 */
static void h_set_imu_offset(void)
{
    float gyro[3];
    float accel[3];
    imu_get_offsets(gyro, accel);
    gyro[0]  = arg_f("gx", gyro[0]);
    gyro[1]  = arg_f("gy", gyro[1]);
    gyro[2]  = arg_f("gz", gyro[2]);
    accel[0] = arg_f("ax", accel[0]);
    accel[1] = arg_f("ay", accel[1]);
    accel[2] = arg_f("az", accel[2]);
    imu_set_offsets(gyro, accel);
    nvs_store_save();
    char buf[160];
    snprintf(buf, sizeof(buf),
             "{\"gyro\":[%.4f,%.4f,%.4f],\"accel\":[%.4f,%.4f,%.4f]}",
             gyro[0], gyro[1], gyro[2], accel[0], accel[1], accel[2]);
    send_json(buf);
}

/** @brief GET /resetCalibration : restaura defaults, persiste e responde. */
static void h_reset_calibration(void)
{
    if (esc_is_arming())
    {
        send_text(423, "Motores ainda armando");
        return;
    }
    flight_task_disable((int)FS_MANUAL_OVERRIDE, false);
    esc_reset_calibration();
    nvs_store_save();
    respond_calibration();
}

/* ===== Setup / tarefa ===== */

void web_routes_init(void)
{
    s_server.on("/", h_root);
    s_server.on("/health", h_health);
    s_server.on("/status", h_status);
    s_server.on("/sensors", h_sensors);
    s_server.on("/flightStatus", h_flight_status);
    s_server.on("/setFlight", h_set_flight);
    s_server.on("/resetPid", h_reset_pid);
    s_server.on("/stopAll", h_stop_all);
    s_server.on("/setMotor", h_set_motor);
    s_server.on("/findDeadband", h_find_deadband);
    s_server.on("/calibration", h_calibration);
    s_server.on("/setCalibration", h_set_calibration);
    s_server.on("/resetCalibration", h_reset_calibration);
    s_server.on("/setImuOffset", h_set_imu_offset);
    s_server.begin();
}

/** @brief Corpo da tarefa do servidor web (core 0). */
static void web_task_run(void *arg)
{
    (void)arg;
    for (;;)
    {
        s_server.handleClient();
        vTaskDelay(pdMS_TO_TICKS(2)); /* cede CPU; nao compete com o laco de 250 Hz */
    }
}

bool web_routes_start(void)
{
    const BaseType_t ok = xTaskCreatePinnedToCore(
        web_task_run, "web", WEB_TASK_STACK, NULL, WEB_TASK_PRIO, &s_task, WEB_TASK_CORE);
    return (ok == pdPASS);
}

bool web_routes_running(void)
{
    return (s_task != NULL);
}
