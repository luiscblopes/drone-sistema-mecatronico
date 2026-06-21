/**
 * @file attitude_filter.cpp
 * @brief Implementação do filtro complementar. CAMINHO CONGELADO (fiel ao sketch).
 */
#include "attitude_filter.h"
#include "config.h"

#include <math.h>

/** Estado integrado do filtro (graus). Persiste entre ciclos. */
static float s_comp_roll = 0.0f;
static float s_comp_pitch = 0.0f;

/** @brief Satura um valor ao intervalo simetrico [-limit, +limit]. */
static float clamp_sym(float v, float limit)
{
    if (v > limit)  { return limit; }
    if (v < -limit) { return -limit; }
    return v;
}

void attitude_filter_reset(void)
{
    s_comp_roll = 0.0f;
    s_comp_pitch = 0.0f;
}

void attitude_filter_update(const imu_sample_t *s, attitude_t *out)
{
    if ((s == NULL) || (out == NULL)) { return; }

    const float ax = s->accel_x_g;
    const float ay = s->accel_y_g;
    const float az = s->accel_z_g;

    /* Angulo pelo acelerometro (geometria da gravidade) — equacoes do sketch.
     * 57.29 ≈ 180/pi (rad -> graus). O denominador e protegido contra zero: se
     * accel viesse [0,0,0] (glitch de I2C que ainda marca valido), o 0/0 geraria
     * NaN, que "gruda" no estado do filtro (clamp nao pega NaN) e nunca se
     * recupera. fmaxf garante um piso minimo no denominador. */
    const float denom_roll  = fmaxf(sqrtf(ax * ax + az * az), 1e-6f);
    const float denom_pitch = fmaxf(sqrtf(ay * ay + az * az), 1e-6f);
    const float angle_roll_acc  =  atanf(ay / denom_roll)  * 57.29f;
    const float angle_pitch_acc = -atanf(ax / denom_pitch) * 57.29f;

    /* Filtro complementar: integra o giroscopio (peso 0.991) e corrige lentamente
     * com o angulo do acelerometro (peso 0.009). [sketch] */
    s_comp_roll  = COMP_FILTER_GYRO_W * (s_comp_roll  + s->rate_roll_dps  * CONTROL_DT_S)
                 + COMP_FILTER_ACC_W  * angle_roll_acc;
    s_comp_pitch = COMP_FILTER_GYRO_W * (s_comp_pitch + s->rate_pitch_dps * CONTROL_DT_S)
                 + COMP_FILTER_ACC_W  * angle_pitch_acc;

    /* Clamp de seguranca a ±20° (envelope comprovado do sketch). */
    s_comp_roll  = clamp_sym(s_comp_roll, ANGLE_CLAMP_DEG);
    s_comp_pitch = clamp_sym(s_comp_pitch, ANGLE_CLAMP_DEG);

    out->roll_deg       = s_comp_roll;
    out->pitch_deg      = s_comp_pitch;
    out->rate_roll_dps  = s->rate_roll_dps;
    out->rate_pitch_dps = s->rate_pitch_dps;
    out->rate_yaw_dps   = s->rate_yaw_dps;
}
