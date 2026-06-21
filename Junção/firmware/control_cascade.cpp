/**
 * @file control_cascade.cpp
 * @brief Implementação da cascata ângulo→taxa. CAMINHO CONGELADO (fiel ao sketch).
 */
#include "control_cascade.h"
#include "config.h"

/* Ganhos de TAXA (malha interna), por eixo: 0=roll, 1=pitch, 2=yaw. */
static pid_gains_t s_rate[3];
/* Ganhos de ANGULO (malha externa) roll/pitch — iguais nos dois eixos (sketch). */
static pid_gains_t s_angle = { ANGLE_KP, ANGLE_KI, ANGLE_KD };

/* Estados anteriores da malha externa (angulo). */
static float s_prev_err_angle[2] = { 0.0f, 0.0f }; /* roll, pitch */
static float s_prev_iterm_angle[2] = { 0.0f, 0.0f };
/* Estados anteriores da malha interna (taxa). */
static float s_prev_err_rate[3] = { 0.0f, 0.0f, 0.0f };
static float s_prev_iterm_rate[3] = { 0.0f, 0.0f, 0.0f };

/** @brief Satura ao intervalo simetrico [-limit, +limit]. */
static float clamp_sym(float v, float limit)
{
    if (v > limit)  { return limit; }
    if (v < -limit) { return -limit; }
    return v;
}

/**
 * @brief Passo de um PID generico (forma do sketch: trapezoidal no I, derivada no
 *        erro). Atualiza prev_err/prev_iterm por referencia e satura I e saida.
 * @return Saida do PID (clampada a ±PID_OUTPUT_CLAMP).
 */
static float pid_step(float error, const pid_gains_t *g, float *prev_err, float *prev_iterm)
{
    const float p = g->kp * error;
    float i = *prev_iterm + (g->ki * (error + *prev_err) * (CONTROL_DT_S / 2.0f));
    i = clamp_sym(i, PID_ITERM_CLAMP);
    const float d = g->kd * ((error - *prev_err) / CONTROL_DT_S);
    float out = p + i + d;
    out = clamp_sym(out, PID_OUTPUT_CLAMP);

    *prev_err = error;
    *prev_iterm = i;
    return out;
}

void control_cascade_init(void)
{
    s_rate[0].kp = RATE_RP_KP; s_rate[0].ki = RATE_RP_KI; s_rate[0].kd = RATE_RP_KD; /* roll */
    s_rate[1].kp = RATE_RP_KP; s_rate[1].ki = RATE_RP_KI; s_rate[1].kd = RATE_RP_KD; /* pitch */
    s_rate[2].kp = RATE_YAW_KP; s_rate[2].ki = RATE_YAW_KI; s_rate[2].kd = RATE_YAW_KD; /* yaw */
    s_angle.kp = ANGLE_KP; s_angle.ki = ANGLE_KI; s_angle.kd = ANGLE_KD;
    control_cascade_reset();
}

void control_cascade_reset(void)
{
    for (uint8_t i = 0U; i < 2U; i++) { s_prev_err_angle[i] = 0.0f; s_prev_iterm_angle[i] = 0.0f; }
    for (uint8_t i = 0U; i < 3U; i++) { s_prev_err_rate[i] = 0.0f; s_prev_iterm_rate[i] = 0.0f; }
}

void control_cascade_step(const attitude_setpoint_t *sp, const attitude_t *att,
                          control_output_t *out)
{
    if ((sp == NULL) || (att == NULL) || (out == NULL)) { return; }

    /* --- Malha externa (angulo -> setpoint de taxa) para roll e pitch --- */
    const float err_angle_roll  = sp->roll_angle_deg  - att->roll_deg;
    const float err_angle_pitch = sp->pitch_angle_deg - att->pitch_deg;
    const float desired_rate_roll  = pid_step(err_angle_roll,  &s_angle,
                                              &s_prev_err_angle[0], &s_prev_iterm_angle[0]);
    const float desired_rate_pitch = pid_step(err_angle_pitch, &s_angle,
                                              &s_prev_err_angle[1], &s_prev_iterm_angle[1]);
    /* Yaw e comando de taxa puro (sem malha externa de angulo). */
    const float desired_rate_yaw = sp->yaw_rate_dps;

    /* --- Malha interna (taxa -> saida para a mixagem) --- */
    const float err_rate_roll  = desired_rate_roll  - att->rate_roll_dps;
    const float err_rate_pitch = desired_rate_pitch - att->rate_pitch_dps;
    const float err_rate_yaw   = desired_rate_yaw   - att->rate_yaw_dps;

    out->roll  = pid_step(err_rate_roll,  &s_rate[0], &s_prev_err_rate[0], &s_prev_iterm_rate[0]);
    out->pitch = pid_step(err_rate_pitch, &s_rate[1], &s_prev_err_rate[1], &s_prev_iterm_rate[1]);
    out->yaw   = pid_step(err_rate_yaw,   &s_rate[2], &s_prev_err_rate[2], &s_prev_iterm_rate[2]);
}

void control_cascade_set_rate_gains(uint8_t axis, const pid_gains_t *g)
{
    if ((axis < 3U) && (g != NULL)) { s_rate[axis] = *g; }
}

void control_cascade_get_rate_gains(uint8_t axis, pid_gains_t *g)
{
    if ((axis < 3U) && (g != NULL)) { *g = s_rate[axis]; }
}
