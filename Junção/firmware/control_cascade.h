/**
 * @file control_cascade.h
 * @brief PID em cascata ângulo→taxa (angle-mode). CAMINHO CONGELADO.
 *
 * Reproduz a cascata comprovada do sketch a 250 Hz:
 *   1) Malha externa: erro de ANGULO (setpoint - estimado) -> PID -> setpoint de TAXA.
 *   2) Malha interna: erro de TAXA (setpoint - giroscopio) -> PID -> termo de saida
 *      (InputRoll/Pitch/Yaw) que alimenta a mixagem.
 * Yaw nao tem malha externa de angulo (comando e taxa pura). Todos os termos I e
 * saidas sao clampados a ±400. Os ganhos vivem em config.h (contrato congelado).
 */
#ifndef CONTROL_CASCADE_H
#define CONTROL_CASCADE_H

#include "attitude_filter.h"

/** @brief Ganhos PID de um eixo. */
typedef struct
{
    float kp;
    float ki;
    float kd;
} pid_gains_t;

/** @brief Setpoint de atitude/throttle comandado (angle-mode). */
typedef struct
{
    float roll_angle_deg;  /**< Angulo de rolagem desejado. */
    float pitch_angle_deg; /**< Angulo de arfagem desejado. */
    float yaw_rate_dps;    /**< Taxa de guinada desejada. */
} attitude_setpoint_t;

/** @brief Saidas da cascata (entram na mixagem). */
typedef struct
{
    float roll;   /**< InputRoll. */
    float pitch;  /**< InputPitch. */
    float yaw;    /**< InputYaw. */
} control_output_t;

/** @brief Inicializa os ganhos com os defaults comprovados (config.h). */
void control_cascade_init(void);

/** @brief Zera estados internos (termos I e erros anteriores). Usar em reset/cutoff. */
void control_cascade_reset(void);

/**
 * @brief Executa um passo da cascata (dt = CONTROL_DT_S).
 * @param sp Setpoint comandado (angulos roll/pitch, taxa yaw).
 * @param att Atitude estimada + taxas (do filtro).
 * @param out Saidas roll/pitch/yaw para a mixagem (ignorado se NULL).
 */
void control_cascade_step(const attitude_setpoint_t *sp, const attitude_t *att,
                          control_output_t *out);

/** @brief Define os ganhos de taxa (malha interna) de um eixo. */
void control_cascade_set_rate_gains(uint8_t axis, const pid_gains_t *g);

/** @brief Le os ganhos de taxa correntes (axis: 0=roll,1=pitch,2=yaw). */
void control_cascade_get_rate_gains(uint8_t axis, pid_gains_t *g);

#endif /* CONTROL_CASCADE_H */
