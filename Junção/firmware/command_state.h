/**
 * @file command_state.h
 * @brief Estado compartilhado entre o servidor web (core 0) e a malha (core 1).
 *
 * Guarda os setpoints comandados, os flags de controle e um INSTANTANEO de
 * telemetria, todos protegidos por secao critica (portMUX), pois sao escritos e
 * lidos por tarefas em nucleos diferentes. Tambem mantem o relogio do WATCHDOG de
 * comando (base do failsafe de perda de link).
 */
#ifndef COMMAND_STATE_H
#define COMMAND_STATE_H

#include <stdint.h>
#include <stdbool.h>

/** @brief Setpoints comandados (angle-mode). */
typedef struct
{
    float throttle_us;     /**< Empuxo base (us). */
    float roll_angle_deg;  /**< Angulo de rolagem desejado. */
    float pitch_angle_deg; /**< Angulo de arfagem desejado. */
    float yaw_rate_dps;    /**< Taxa de guinada desejada. */
} command_setpoint_t;

/** @brief Instantâneo de telemetria publicado pela malha para o servidor web. */
typedef struct
{
    float roll_deg;       /**< Atitude estimada (rolagem). */
    float pitch_deg;      /**< Atitude estimada (arfagem). */
    float rate_roll_dps;  /**< Taxa de rolagem. */
    float rate_pitch_dps; /**< Taxa de arfagem. */
    float rate_yaw_dps;   /**< Taxa de guinada. */
    int32_t motor_us[4];  /**< Ultimo pulso por motor. */
    bool imu_valid;       /**< IMU valida e recente. */
    uint32_t imu_age_ms;  /**< Idade da ultima amostra de IMU. */
    bool enabled;         /**< Malha estabilizada ativa. */
    bool arming;          /**< ESCs ainda armando. */
    int failsafe_reason;  /**< failsafe_reason_t corrente. */
    bool latched;         /**< Latch de failsafe ativo. */
} command_telemetry_t;

/** @brief Inicializa o estado (zera setpoints, flags e telemetria). */
void command_state_init(void);

/* ----- Setpoints (escritos pelo web, lidos pela malha) ----- */

/** @brief Define os setpoints comandados (já saturados pelo chamador). */
void command_state_set_setpoint(const command_setpoint_t *sp);

/** @brief Le os setpoints correntes (ignorado se NULL). */
void command_state_get_setpoint(command_setpoint_t *sp);

/* ----- Watchdog de comando ----- */

/** @brief Marca a chegada de um comando valido (reinicia o watchdog). */
void command_state_mark_command(void);

/** @brief Tempo (ms) desde o ultimo comando valido. */
uint32_t command_state_command_age_ms(void);

/* ----- Flags de controle ----- */

/** @brief Pede (ou nao) a malha estabilizada (apply=1 & stabilize=1). */
void command_state_request_stabilize(bool request);

/** @brief Indica se a estabilizacao foi solicitada pelo ultimo comando. */
bool command_state_stabilize_requested(void);

/** @brief Define o flag "malha habilitada" (escrito pela malha). */
void command_state_set_enabled(bool enabled);

/** @brief Indica se a malha esta habilitada. */
bool command_state_is_enabled(void);

/* ----- Telemetria (escrita pela malha, lida pelo web) ----- */

/** @brief Publica um instantaneo de telemetria (chamado pela malha). */
void command_state_publish_telemetry(const command_telemetry_t *t);

/** @brief Le o ultimo instantaneo de telemetria (ignorado se NULL). */
void command_state_get_telemetry(command_telemetry_t *t);

#endif /* COMMAND_STATE_H */
