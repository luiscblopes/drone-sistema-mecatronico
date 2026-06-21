/**
 * @file motor_mix.cpp
 * @brief Implementação da mixagem física do Drone-main. CAMINHO CONGELADO.
 */
#include "motor_mix.h"
#include "config.h"

/** @brief Satura um valor float a [lo, hi] e converte para inteiro (us). */
static int32_t sat_us(float v, int32_t lo, int32_t hi)
{
    if (v < (float)lo) { return lo; }
    if (v > (float)hi) { return hi; }
    return (int32_t)v;
}

void motor_mix_compute(float throttle_us, const control_output_t *c, motor_output_t *out)
{
    if ((c == NULL) || (out == NULL)) { return; }

    const float t = throttle_us;
    const float r = c->roll;
    const float p = c->pitch;
    const float y = c->yaw;

    /* Convencao FISICA do Drone-main (research D6). NAO herdar a do sketch. */
    float m1 = t + p + r - y; /* M1 frente-esquerda */
    float m2 = t + p - r + y; /* M2 frente-direita  */
    float m3 = t - p - r - y; /* M3 tras-direita    */
    float m4 = t - p + r + y; /* M4 tras-esquerda   */

    /* Saturacao por motor: teto 1999, piso idle (motor nao para com malha ativa). */
    out->us[0] = sat_us(m1, THROTTLE_IDLE_US, MOTOR_OUTPUT_MAX_US);
    out->us[1] = sat_us(m2, THROTTLE_IDLE_US, MOTOR_OUTPUT_MAX_US);
    out->us[2] = sat_us(m3, THROTTLE_IDLE_US, MOTOR_OUTPUT_MAX_US);
    out->us[3] = sat_us(m4, THROTTLE_IDLE_US, MOTOR_OUTPUT_MAX_US);
}
