/**
 * @file firmware.ino
 * @brief Ponto de entrada do firmware de Junção (Arduino-ESP32).
 *
 * Une a logica de voo comprovada do ESP32-Flight-controller (angle-mode, filtro
 * complementar, PID em cascata a 250 Hz) ao hardware e a interface Wi-Fi do
 * Drone-main. O boot inicializa, na ordem segura, os subsistemas e cria as duas
 * tarefas FreeRTOS: a malha de controle (core 1, 250 Hz, caminho congelado) e o
 * servidor web (core 0). O comando vem exclusivamente por Wi-Fi (sem radio RC).
 *
 * ⚠️ SEGURANCA: toda verificacao e SEM HELICES (ver ../specs/.../quickstart.md).
 *
 * Rastreabilidade: spec/plan/tasks em specs/001-firmware-juncao-voo/.
 */
#include "config.h"
#include "esc_out.h"
#include "imu_mpu9250.h"
#include "nvs_store.h"
#include "wifi_ap.h"
#include "web_routes.h"
#include "flight_task.h"

#include <Arduino.h>

/**
 * @brief Inicializacao unica: subsistemas + tarefas, na ordem segura.
 *
 * Ordem: serial -> NVS (carrega calibracao) -> ESC (arming no minimo) -> IMU ->
 * estimador/cascata/failsafe -> Wi-Fi AP -> servidor web -> tarefa de controle.
 * A calibracao e carregada ANTES do ESC para os defaults ja refletirem o aferido.
 */
void setup(void)
{
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[boot] Firmware de Juncao iniciando...");

    /* Persistencia: carrega calibracao de motor e offsets da IMU da NVS. */
    nvs_store_init();
    nvs_store_load();

    /* ESCs: atribui pinos, define 250 Hz e ARMA no minimo (segura ESC_ARM_HOLD_MS). */
    esc_init();
    Serial.println("[boot] ESCs armando no minimo...");

    /* IMU: I2C + remap de orientacao. */
    if (imu_init())
    {
        Serial.println("[boot] IMU MPU-9250 detectada (0x68).");
    }
    else
    {
        Serial.println("[boot] AVISO: IMU nao respondeu — failsafe IMU_INVALID bloqueara armar.");
    }

    /* Estimador, cascata e failsafe. */
    flight_task_init();

    /* Rede: sobe o AP Wi-Fi (unico canal de comando). */
    (void)wifi_ap_start();

    /* Servidor web (core 0) e tarefa de controle (core 1). */
    web_routes_init();
    if (!web_routes_start()) { Serial.println("[boot] FALHA ao criar a tarefa web."); }
    if (!flight_task_start()) { Serial.println("[boot] FALHA ao criar a tarefa de controle."); }

    Serial.println("[boot] Pronto. Conecte ao AP e abra http://192.168.4.1");
}

/**
 * @brief Laço ocioso: o trabalho real roda nas tarefas FreeRTOS.
 *
 * Mantido leve de proposito — o controle (250 Hz) e o web tem tarefas dedicadas
 * em nucleos separados para preservar o determinismo do caminho congelado.
 */
void loop(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
