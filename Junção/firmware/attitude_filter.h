/**
 * @file attitude_filter.h
 * @brief Filtro complementar (estimativa de atitude). CAMINHO CONGELADO.
 *
 * Reproduz fielmente o estimador comprovado do sketch: calcula roll/pitch pelo
 * acelerometro (geometria da gravidade) e funde com a integracao do giroscopio
 * num filtro complementar de pesos 0.991/0.009, com a saida clampada a ±20°.
 * Opera sobre dados JA remapeados pela IMU (referencial do frame).
 */
#ifndef ATTITUDE_FILTER_H
#define ATTITUDE_FILTER_H

#include "imu_mpu9250.h"

/** @brief Atitude estimada e taxas medidas (referencial do frame). */
typedef struct
{
    float roll_deg;       /**< Rolagem estimada (clamp ±20°). */
    float pitch_deg;      /**< Arfagem estimada (clamp ±20°). */
    float rate_roll_dps;  /**< Taxa de rolagem (giroscopio). */
    float rate_pitch_dps; /**< Taxa de arfagem (giroscopio). */
    float rate_yaw_dps;   /**< Taxa de guinada (giroscopio). */
} attitude_t;

/** @brief Zera o estado interno do filtro (usar no boot e em reset). */
void attitude_filter_reset(void);

/**
 * @brief Atualiza a estimativa com uma amostra da IMU (passo de CONTROL_DT_S).
 * @param s Amostra remapeada da IMU.
 * @param out Atitude estimada resultante (ignorado se NULL).
 */
void attitude_filter_update(const imu_sample_t *s, attitude_t *out);

#endif /* ATTITUDE_FILTER_H */
