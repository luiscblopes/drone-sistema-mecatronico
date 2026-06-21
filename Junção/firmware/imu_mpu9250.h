/**
 * @file imu_mpu9250.h
 * @brief Leitura da IMU MPU-9250 (I2C) com remap de orientação. CAMINHO CONGELADO.
 *
 * O MPU-9250 e register-compatible com o MPU-6050 no nucleo accel/gyro, entao o
 * codigo de leitura do sketch funciona sem mudar registradores. A diferenca
 * critica nesta placa e a ORIENTACAO: a IMU e montada girada, e este modulo
 * aplica o remap +90° em Y (a accel e gyro) ANTES de entregar os dados, de modo
 * que "nivelado" leia roll/pitch ~0 (config.h IMU_REMAP_ROTATE_Y_90).
 *
 * Tambem subtrai os offsets de calibracao (gyro/accel), que substituem os valores
 * hardcoded do sketch e podem ser persistidos na NVS.
 */
#ifndef IMU_MPU9250_H
#define IMU_MPU9250_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Amostra da IMU ja remapeada, escalada e sem offset.
 *
 * Eixos no referencial do FRAME (apos remap): roll/pitch/yaw conforme a montagem.
 */
typedef struct
{
    float accel_x_g;     /**< Aceleracao no eixo x do frame (g). */
    float accel_y_g;     /**< Aceleracao no eixo y do frame (g). */
    float accel_z_g;     /**< Aceleracao no eixo z do frame (g). */
    float rate_roll_dps; /**< Velocidade angular de rolagem (°/s). */
    float rate_pitch_dps;/**< Velocidade angular de arfagem (°/s). */
    float rate_yaw_dps;  /**< Velocidade angular de guinada (°/s). */
    uint32_t timestamp_ms; /**< Instante da leitura (millis). */
    bool valid;          /**< true se a leitura I2C foi bem-sucedida. */
} imu_sample_t;

/**
 * @brief Inicializa o I2C e acorda o MPU-9250 (PWR_MGMT, DLPF, escalas).
 * @return true se a IMU respondeu no endereco esperado (0x68).
 */
bool imu_init(void);

/**
 * @brief Le a IMU, aplica remap de orientacao, escala e remove offsets.
 * @param out Saida (preenchido sempre; campo valid indica sucesso).
 */
void imu_read(imu_sample_t *out);

/**
 * @brief Define os offsets de calibracao (gyro em °/s, accel em g).
 * @param gyro_dps Vetor [roll, pitch, yaw] no referencial do frame.
 * @param accel_g  Vetor [x, y, z] no referencial do frame.
 */
void imu_set_offsets(const float gyro_dps[3], const float accel_g[3]);

/** @brief Le os offsets atuais (vetores de saida ignorados se NULL). */
void imu_get_offsets(float gyro_dps[3], float accel_g[3]);

#endif /* IMU_MPU9250_H */
