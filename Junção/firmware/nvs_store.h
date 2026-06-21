/**
 * @file nvs_store.h
 * @brief Persistência de calibração na NVS (via Preferences). [US3]
 *
 * A calibracao (start/max/trim por motor e offsets da IMU) e aferida na bancada e
 * precisa sobreviver a desligamentos. Este modulo faz a ponte entre a NVS e os
 * modulos donos dos valores (esc_out e imu_mpu9250). Substitui os offsets
 * hardcoded do sketch por valores persistidos e ajustaveis pela interface.
 */
#ifndef NVS_STORE_H
#define NVS_STORE_H

#include <stdbool.h>

/** @brief Inicializa o subsistema de persistencia. Chamar uma vez no boot. */
void nvs_store_init(void);

/**
 * @brief Carrega a calibracao da NVS e aplica em esc_out e imu_mpu9250.
 *
 * Chaves ausentes mantem os defaults atuais dos modulos. Chamar no boot, ANTES de
 * iniciar a malha de controle.
 */
void nvs_store_load(void);

/** @brief Le a calibracao atual (motores + IMU) e grava na NVS (torna permanente). */
void nvs_store_save(void);

#endif /* NVS_STORE_H */
