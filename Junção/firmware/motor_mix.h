/**
 * @file motor_mix.h
 * @brief Mixagem dos motores (convenção física do Drone-main). CAMINHO CONGELADO.
 *
 * Converte throttle + saidas da cascata (roll/pitch/yaw) nos 4 pulsos de motor,
 * usando a convencao FISICA do Drone-main (re-derivada — NAO a do sketch, que
 * inverteria pitch e erraria a diagonal de yaw nesta placa):
 *   M1 (frente-esq) = T + P + R - Y
 *   M2 (frente-dir) = T + P - R + Y
 *   M3 (tras-dir)   = T - P - R - Y
 *   M4 (tras-esq)   = T - P + R + Y
 * ⚠️ Sinais confirmados motor a motor em bancada (quickstart §4/§5).
 */
#ifndef MOTOR_MIX_H
#define MOTOR_MIX_H

#include "control_cascade.h"

/** @brief Pulsos resultantes por motor (us), na ordem M1..M4. */
typedef struct
{
    int32_t us[4];
} motor_output_t;

/**
 * @brief Calcula os 4 pulsos a partir do throttle e das saidas da cascata.
 *
 * Aplica a mixagem e a saturacao por motor: teto MOTOR_OUTPUT_MAX_US e piso
 * THROTTLE_IDLE_US (mantem os motores girando o minimo util com a malha ativa).
 *
 * @param throttle_us Empuxo base (us).
 * @param c Saidas roll/pitch/yaw da cascata.
 * @param out Pulsos por motor (ignorado se NULL).
 */
void motor_mix_compute(float throttle_us, const control_output_t *c, motor_output_t *out);

#endif /* MOTOR_MIX_H */
