/**
 * @file command_state.cpp
 * @brief Implementação do estado compartilhado (seção crítica/portMUX).
 */
#include "command_state.h"

#include <Arduino.h>

/** Secao critica que protege TODO o estado abaixo (acesso de 2 nucleos). */
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

static command_setpoint_t s_sp;
static command_telemetry_t s_tlm;
static uint32_t s_last_cmd_ms = 0U;
static bool s_stabilize_req = false;
static bool s_enabled = false;

void command_state_init(void)
{
    portENTER_CRITICAL(&s_mux);
    s_sp.throttle_us = (float)1000;
    s_sp.roll_angle_deg = 0.0f;
    s_sp.pitch_angle_deg = 0.0f;
    s_sp.yaw_rate_dps = 0.0f;
    s_tlm = command_telemetry_t{};
    s_last_cmd_ms = millis();
    s_stabilize_req = false;
    s_enabled = false;
    portEXIT_CRITICAL(&s_mux);
}

void command_state_set_setpoint(const command_setpoint_t *sp)
{
    if (sp == NULL) { return; }
    portENTER_CRITICAL(&s_mux);
    s_sp = *sp;
    portEXIT_CRITICAL(&s_mux);
}

void command_state_get_setpoint(command_setpoint_t *sp)
{
    if (sp == NULL) { return; }
    portENTER_CRITICAL(&s_mux);
    *sp = s_sp;
    portEXIT_CRITICAL(&s_mux);
}

void command_state_mark_command(void)
{
    const uint32_t now = millis();
    portENTER_CRITICAL(&s_mux);
    s_last_cmd_ms = now;
    portEXIT_CRITICAL(&s_mux);
}

uint32_t command_state_command_age_ms(void)
{
    const uint32_t now = millis();
    portENTER_CRITICAL(&s_mux);
    const uint32_t last = s_last_cmd_ms;
    portEXIT_CRITICAL(&s_mux);
    return now - last;
}

void command_state_request_stabilize(bool request)
{
    portENTER_CRITICAL(&s_mux);
    s_stabilize_req = request;
    portEXIT_CRITICAL(&s_mux);
}

bool command_state_stabilize_requested(void)
{
    portENTER_CRITICAL(&s_mux);
    const bool v = s_stabilize_req;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

void command_state_set_enabled(bool enabled)
{
    portENTER_CRITICAL(&s_mux);
    s_enabled = enabled;
    portEXIT_CRITICAL(&s_mux);
}

bool command_state_is_enabled(void)
{
    portENTER_CRITICAL(&s_mux);
    const bool v = s_enabled;
    portEXIT_CRITICAL(&s_mux);
    return v;
}

void command_state_publish_telemetry(const command_telemetry_t *t)
{
    if (t == NULL) { return; }
    portENTER_CRITICAL(&s_mux);
    s_tlm = *t;
    portEXIT_CRITICAL(&s_mux);
}

void command_state_get_telemetry(command_telemetry_t *t)
{
    if (t == NULL) { return; }
    portENTER_CRITICAL(&s_mux);
    *t = s_tlm;
    portEXIT_CRITICAL(&s_mux);
}
