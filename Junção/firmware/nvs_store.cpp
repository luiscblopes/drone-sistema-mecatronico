/**
 * @file nvs_store.cpp
 * @brief Implementação da persistência de calibração (Preferences/NVS). [US3]
 */
#include "nvs_store.h"
#include "config.h"
#include "esc_out.h"
#include "imu_mpu9250.h"

#include <Arduino.h>
#include <Preferences.h>

static Preferences s_prefs;

void nvs_store_init(void)
{
    /* Preferences usa a NVS internamente; nada a alocar aqui. A inicializacao da
     * particao NVS e feita pelo proprio Preferences no primeiro begin(). */
}

void nvs_store_load(void)
{
    /* --- Calibracao de motor (namespace motor-cal, somente leitura) --- */
    s_prefs.begin(NVS_NS_MOTOR_CAL, true);
    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        int32_t start_us;
        int32_t max_us;
        int32_t trim_us;
        esc_get_calibration(i, &start_us, &max_us, &trim_us); /* parte do default atual */

        char key[12];
        (void)snprintf(key, sizeof(key), "start%u", (unsigned)i);
        start_us = s_prefs.getInt(key, start_us);
        (void)snprintf(key, sizeof(key), "max%u", (unsigned)i);
        max_us = s_prefs.getInt(key, max_us);
        (void)snprintf(key, sizeof(key), "trim%u", (unsigned)i);
        trim_us = s_prefs.getInt(key, trim_us);

        esc_set_calibration(i, start_us, max_us, trim_us); /* sanitiza ao aplicar */
    }
    s_prefs.end();

    /* --- Offsets da IMU (namespace imu-cal, somente leitura) --- */
    float gyro[3];
    float accel[3];
    imu_get_offsets(gyro, accel); /* parte do default atual */
    s_prefs.begin(NVS_NS_IMU_CAL, true);
    gyro[0]  = s_prefs.getFloat("gx", gyro[0]);
    gyro[1]  = s_prefs.getFloat("gy", gyro[1]);
    gyro[2]  = s_prefs.getFloat("gz", gyro[2]);
    accel[0] = s_prefs.getFloat("ax", accel[0]);
    accel[1] = s_prefs.getFloat("ay", accel[1]);
    accel[2] = s_prefs.getFloat("az", accel[2]);
    s_prefs.end();
    imu_set_offsets(gyro, accel);
}

void nvs_store_save(void)
{
    /* --- Calibracao de motor (escrita) --- */
    s_prefs.begin(NVS_NS_MOTOR_CAL, false);
    for (uint8_t i = 0U; i < NUM_MOTORS; i++)
    {
        int32_t start_us;
        int32_t max_us;
        int32_t trim_us;
        esc_get_calibration(i, &start_us, &max_us, &trim_us);

        char key[12];
        (void)snprintf(key, sizeof(key), "start%u", (unsigned)i);
        (void)s_prefs.putInt(key, start_us);
        (void)snprintf(key, sizeof(key), "max%u", (unsigned)i);
        (void)s_prefs.putInt(key, max_us);
        (void)snprintf(key, sizeof(key), "trim%u", (unsigned)i);
        (void)s_prefs.putInt(key, trim_us);
    }
    s_prefs.end();

    /* --- Offsets da IMU (escrita) --- */
    float gyro[3];
    float accel[3];
    imu_get_offsets(gyro, accel);
    s_prefs.begin(NVS_NS_IMU_CAL, false);
    (void)s_prefs.putFloat("gx", gyro[0]);
    (void)s_prefs.putFloat("gy", gyro[1]);
    (void)s_prefs.putFloat("gz", gyro[2]);
    (void)s_prefs.putFloat("ax", accel[0]);
    (void)s_prefs.putFloat("ay", accel[1]);
    (void)s_prefs.putFloat("az", accel[2]);
    s_prefs.end();
}
