/**
 * @file esc_out.h
 * @brief Geração de PWM dos ESCs (via ESP32Servo) e arming. CAMINHO CONGELADO.
 *
 * Encapsula a escrita de pulso (1000..2000 us) nos quatro motores a 250 Hz,
 * o arming (segura o minimo no boot) e a calibracao por motor (start/max/trim).
 * E a unica porta de saida para os ESCs — todo o resto do firmware aciona motor
 * por aqui, nunca escrevendo no hardware diretamente (Princípio I/IV).
 *
 * @warning Alterar faixa/frequencia/arming muda a seguranca de partida dos ESCs;
 * so com revalidacao em bancada.
 */
#ifndef ESC_OUT_H
#define ESC_OUT_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Inicializa os 4 ESCs: aloca timers, atribui pinos, define 250 Hz e
 *        escreve o minimo. Marca o inicio do arming.
 *
 * Chamar uma vez no boot, depois de carregar a calibracao da NVS (para que os
 * defaults ja reflitam o calibrado).
 */
void esc_init(void);

/** @brief true enquanto o arming nao terminou (ESC_ARM_HOLD_MS desde o boot). */
bool esc_is_arming(void);

/**
 * @brief Escreve um pulso (us) em um motor, saturando a faixa valida e somando o
 *        trim de calibracao.
 * @param motor Indice 0..3.
 * @param pulse_us Pulso desejado (sera saturado a [ESC_MIN_US, MOTOR_OUTPUT_MAX_US]).
 */
void esc_set_motor_us(uint8_t motor, int32_t pulse_us);

/** @brief Ultimo pulso (us) efetivamente escrito no motor. */
int32_t esc_get_motor_us(uint8_t motor);

/**
 * @brief Aciona um motor por percentual (0..100), mapeado por start/max da
 *        calibracao. Usado no teste de motor individual (/setMotor).
 * @param motor Indice 0..3.
 * @param percent 0..100.
 */
void esc_set_motor_speed_pct(uint8_t motor, int32_t percent);

/** @brief Percentual aproximado (0..100) correspondente ao pulso atual do motor. */
int32_t esc_get_motor_speed_pct(uint8_t motor);

/** @brief Leva todos os motores ao minimo (parada segura). */
void esc_stop_all(void);

/**
 * @brief Define a calibracao de um motor (sanitiza trim a [-100,+100]).
 * @param motor Indice 0..3.
 * @param start_us Pulso minimo util.
 * @param max_us Pulso maximo util.
 * @param trim_us Ajuste fino somado a saida.
 */
void esc_set_calibration(uint8_t motor, int32_t start_us, int32_t max_us, int32_t trim_us);

/** @brief Le a calibracao de um motor (parametros de saida ignorados se NULL). */
void esc_get_calibration(uint8_t motor, int32_t *start_us, int32_t *max_us, int32_t *trim_us);

/** @brief Restaura a calibracao de todos os motores aos defaults de fabrica. */
void esc_reset_calibration(void);

#endif /* ESC_OUT_H */
