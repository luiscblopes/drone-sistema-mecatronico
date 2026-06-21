/**
 * @file failsafe.h
 * @brief Máquina de estados de segurança com LATCH. NÃO-NEGOCIÁVEL (Princípio I).
 *
 * Avalia, a cada ciclo da malha, as condicoes perigosas e decide se o acionamento
 * e permitido. Qualquer condicao perigosa (perda de comando, IMU invalida/nao
 * calibrada, inclinacao excessiva, parada de emergencia) leva os motores ao
 * minimo, desabilita a malha e TRAVA (latch) o rearme ate o operador comandar
 * PARAR TUDO (/stopAll). Isso evita religamento automatico apos uma falha.
 *
 * O override manual (/setMotor, /setCalibration) tambem desabilita a malha, mas
 * NAO trava o rearme (nao e condicao perigosa, e acao deliberada do operador).
 */
#ifndef FAILSAFE_H
#define FAILSAFE_H

#include <stdint.h>
#include <stdbool.h>

/** @brief Motivo corrente de failsafe (e texto estavel para telemetria). */
typedef enum
{
    FS_NONE = 0,          /**< Sem failsafe. */
    FS_COMMAND_TIMEOUT,   /**< Sem comando valido por > FLIGHT_COMMAND_TIMEOUT_MS. */
    FS_IMU_INVALID,       /**< IMU ausente, amostra velha ou nao calibrada. */
    FS_EXCESSIVE_TILT,    /**< |roll| ou |pitch| > MAX_SAFE_TILT_DEG. */
    FS_EMERGENCY_STOP,    /**< Parada de emergencia do operador (/stopAll). */
    FS_MANUAL_OVERRIDE    /**< Override manual deliberado (nao trava). */
} failsafe_reason_t;

/** @brief Entrada de avaliacao do failsafe (lida do estado da malha). */
typedef struct
{
    uint32_t command_age_ms; /**< Tempo desde o ultimo comando valido. */
    bool imu_valid;          /**< Amostra de IMU valida e recente. */
    bool imu_calibrated;     /**< IMU considerada calibrada. */
    float roll_deg;          /**< Atitude estimada (rolagem). */
    float pitch_deg;         /**< Atitude estimada (arfagem). */
} failsafe_input_t;

/** @brief Inicializa a maquina (sem failsafe, sem latch). */
void failsafe_init(void);

/**
 * @brief Avalia as condicoes de seguranca e atualiza o estado/latch.
 *
 * Se detectar condicao perigosa, registra o motivo e ativa o latch. Uma vez
 * travado, permanece travado (mantendo o primeiro motivo perigoso) ate
 * ::failsafe_clear_latch.
 *
 * @param in Entradas correntes.
 * @return true se o acionamento estabilizado e PERMITIDO (sem failsafe ativo).
 */
bool failsafe_evaluate(const failsafe_input_t *in);

/**
 * @brief Forca um failsafe por acao do operador/sistema (ex.: e-stop, override).
 * @param reason Motivo. Motivos perigosos travam; FS_MANUAL_OVERRIDE nao trava.
 */
void failsafe_trip(failsafe_reason_t reason);

/** @brief Limpa o latch e o motivo (apos PARAR TUDO). Permite rearmar. */
void failsafe_clear_latch(void);

/** @brief Motivo corrente. */
failsafe_reason_t failsafe_reason(void);

/** @brief true enquanto o rearme estiver travado aguardando PARAR TUDO. */
bool failsafe_is_latched(void);

/** @brief Texto curto e estavel do motivo (nunca NULL). */
const char *failsafe_text(failsafe_reason_t reason);

#endif /* FAILSAFE_H */
