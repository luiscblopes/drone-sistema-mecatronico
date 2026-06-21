/**
 * @file imu_mpu9250.cpp
 * @brief Implementação da leitura da IMU MPU-9250 com remap. CAMINHO CONGELADO.
 */
#include "imu_mpu9250.h"
#include "config.h"

#include <Arduino.h>
#include <Wire.h>

/* Registradores do MPU (compatíveis MPU-6050/9250). */
#define REG_PWR_MGMT_1   (0x6B)
#define REG_CONFIG_DLPF  (0x1A)
#define REG_GYRO_CONFIG  (0x1B)
#define REG_ACCEL_CONFIG (0x1C)
#define REG_ACCEL_XOUT_H (0x3B)
#define REG_GYRO_XOUT_H  (0x43)

/** Offsets de calibracao (subtraidos da leitura). Defaults do sketch. */
static float s_gyro_off[3]  = IMU_DEFAULT_GYRO_OFFSET_DPS;
static float s_accel_off[3] = IMU_DEFAULT_ACCEL_OFFSET_G;

/** Protege os offsets, escritos pela rota /setImuOffset (core 0) e lidos pela
 * tarefa de controle em imu_read (core 1). Evita ler um vetor meio-atualizado. */
static portMUX_TYPE s_imu_mux = portMUX_INITIALIZER_UNLOCKED;

/** @brief Escreve um valor em um registrador do MPU. */
static void mpu_write(uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(IMU_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

/**
 * @brief Le um inteiro de 16 bits (big-endian) do barramento, na ordem certa.
 *
 * IMPORTANTE: nao usar "Wire.read() << 8 | Wire.read()" — a ordem de avaliacao
 * dos dois Wire.read() e indefinida em C++, o que pode trocar os bytes (high/low)
 * e corromper TODA a leitura da IMU. Aqui as leituras sao sequenciadas
 * explicitamente em variaveis nomeadas.
 */
static int16_t mpu_read_i16(void)
{
    const uint8_t hi = (uint8_t)Wire.read();
    const uint8_t lo = (uint8_t)Wire.read();
    return (int16_t)(((uint16_t)hi << 8) | (uint16_t)lo);
}

bool imu_init(void)
{
    Wire.begin(I2C_SDA_GPIO, I2C_SCL_GPIO, I2C_CLOCK_HZ);
    delay(250);

    /* Acorda o sensor (sai do sleep). */
    mpu_write(REG_PWR_MGMT_1, 0x00);
    /* DLPF: filtro digital (mesmo valor de leitura do sketch, reg 0x1A=0x05). */
    mpu_write(REG_CONFIG_DLPF, 0x05);
    /* Giroscopio ±500 °/s (0x08) e acelerometro ±8 g (0x10) — escalas do sketch. */
    mpu_write(REG_GYRO_CONFIG, 0x08);
    mpu_write(REG_ACCEL_CONFIG, 0x10);

    /* Confirma presenca: tenta um endTransmission no endereco. */
    Wire.beginTransmission(IMU_I2C_ADDR);
    return (Wire.endTransmission() == 0);
}

void imu_read(imu_sample_t *out)
{
    if (out == NULL) { return; }
    /* Zera os campos: numa falha de I2C, os dados nao ficam com lixo (que poderia
     * corromper o filtro). timestamp e preenchido sempre. */
    *out = imu_sample_t{};
    out->timestamp_ms = millis();

    /* --- Acelerometro (6 bytes a partir de 0x3B) --- */
    Wire.beginTransmission(IMU_I2C_ADDR);
    Wire.write(REG_ACCEL_XOUT_H);
    if (Wire.endTransmission() != 0)
    {
        out->valid = false;
        return;
    }
    Wire.requestFrom((uint8_t)IMU_I2C_ADDR, (uint8_t)6);
    if (Wire.available() < 6)
    {
        out->valid = false;
        return;
    }
    const int16_t ax_lsb = mpu_read_i16();
    const int16_t ay_lsb = mpu_read_i16();
    const int16_t az_lsb = mpu_read_i16();

    /* --- Giroscopio (6 bytes a partir de 0x43) --- */
    Wire.beginTransmission(IMU_I2C_ADDR);
    Wire.write(REG_GYRO_XOUT_H);
    if (Wire.endTransmission() != 0)
    {
        out->valid = false;
        return;
    }
    Wire.requestFrom((uint8_t)IMU_I2C_ADDR, (uint8_t)6);
    if (Wire.available() < 6)
    {
        out->valid = false;
        return;
    }
    const int16_t gx_lsb = mpu_read_i16();
    const int16_t gy_lsb = mpu_read_i16();
    const int16_t gz_lsb = mpu_read_i16();

    /* Escala bruta (referencial fisico do chip). */
    const float acc_x = (float)ax_lsb / ACCEL_LSB_PER_G;
    const float acc_y = (float)ay_lsb / ACCEL_LSB_PER_G;
    const float acc_z = (float)az_lsb / ACCEL_LSB_PER_G;
    const float gyr_x = (float)gx_lsb / GYRO_LSB_PER_DPS;
    const float gyr_y = (float)gy_lsb / GYRO_LSB_PER_DPS;
    const float gyr_z = (float)gz_lsb / GYRO_LSB_PER_DPS;

#if IMU_REMAP_ROTATE_Y_90
    /* Remap +90° em Y (a accel E gyro): x' = z ; y' = y ; z' = -x.
     * Compensa a IMU montada girada nesta placa (config.h). Sem isto, roll/pitch
     * sairiam trocados/invertidos. ⚠️ Sinais confirmados em bancada (quickstart §2/§5). */
    out->accel_x_g     = acc_z;
    out->accel_y_g     = acc_y;
    out->accel_z_g     = -acc_x;
    out->rate_roll_dps = gyr_z;
    out->rate_pitch_dps= gyr_y;
    out->rate_yaw_dps  = -gyr_x;
#else
    /* IMU remontada com Z para cima (sem remap). */
    out->accel_x_g     = acc_x;
    out->accel_y_g     = acc_y;
    out->accel_z_g     = acc_z;
    out->rate_roll_dps = gyr_x;
    out->rate_pitch_dps= gyr_y;
    out->rate_yaw_dps  = gyr_z;
#endif

    /* Remove offsets de calibracao (referencial do frame). Tira um retrato
     * consistente dos offsets sob secao critica (podem mudar via /setImuOffset). */
    float go[3];
    float ao[3];
    portENTER_CRITICAL(&s_imu_mux);
    go[0] = s_gyro_off[0];  go[1] = s_gyro_off[1];  go[2] = s_gyro_off[2];
    ao[0] = s_accel_off[0]; ao[1] = s_accel_off[1]; ao[2] = s_accel_off[2];
    portEXIT_CRITICAL(&s_imu_mux);

    out->accel_x_g     -= ao[0];
    out->accel_y_g     -= ao[1];
    out->accel_z_g     -= ao[2];
    out->rate_roll_dps -= go[0];
    out->rate_pitch_dps-= go[1];
    out->rate_yaw_dps  -= go[2];

    out->valid = true;
}

void imu_set_offsets(const float gyro_dps[3], const float accel_g[3])
{
    portENTER_CRITICAL(&s_imu_mux);
    if (gyro_dps != NULL)
    {
        for (uint8_t i = 0U; i < 3U; i++) { s_gyro_off[i] = gyro_dps[i]; }
    }
    if (accel_g != NULL)
    {
        for (uint8_t i = 0U; i < 3U; i++) { s_accel_off[i] = accel_g[i]; }
    }
    portEXIT_CRITICAL(&s_imu_mux);
}

void imu_get_offsets(float gyro_dps[3], float accel_g[3])
{
    portENTER_CRITICAL(&s_imu_mux);
    if (gyro_dps != NULL)
    {
        for (uint8_t i = 0U; i < 3U; i++) { gyro_dps[i] = s_gyro_off[i]; }
    }
    if (accel_g != NULL)
    {
        for (uint8_t i = 0U; i < 3U; i++) { accel_g[i] = s_accel_off[i]; }
    }
    portEXIT_CRITICAL(&s_imu_mux);
}
