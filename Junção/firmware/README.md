# Firmware de Junção — Voo em Angle-Mode por Wi-Fi

Firmware **Arduino-ESP32** que roda na placa do `Drone-main`, unindo a lógica de voo
comprovada do `ESP32-Flight-controller` (angle-mode, filtro complementar, PID em cascata a
**250 Hz**) à infraestrutura de comando por **Wi-Fi** e aos **failsafes com latch**.

> ⚠️ **SEGURANÇA:** toda verificação é feita **SEM HÉLICES**. Siga o roteiro de bancada em
> `../specs/001-firmware-juncao-voo/quickstart.md` antes de qualquer voo.

## Dependências

- Core **arduino-esp32** (placa `esp32:esp32:esp32`).
- Biblioteca **ESP32Servo** (geração de PWM dos ESCs).
- `Wire`, `WiFi`, `WebServer`, `Preferences` — já fazem parte do core.

Instalação (uma vez):

```bash
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "ESP32Servo"
```

## Compilar e gravar

```bash
# a partir desta pasta (Junção/firmware)
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload  --fqbn esp32:esp32:esp32 -p <PORTA> .
arduino-cli monitor -p <PORTA> -c baudrate=115200   # opcional
```

> O sketch principal é `firmware.ino`. O Arduino IDE também compila esta pasta (abra
> `firmware.ino`); todos os `.cpp/.h` são compilados juntos.

## Operação

1. Conecte ao Wi-Fi **`EQUIPE4-AP`** (senha `12345678`).
2. Abra **http://192.168.4.1** no navegador (celular ou PC) — console web embutido.
3. Comande setpoints, leia telemetria e calibre pelas rotas HTTP.

## Estrutura

| Arquivo | Papel |
|---------|-------|
| `firmware.ino` | Boot, inicialização e criação das tarefas |
| `config.h` | **Contrato congelado** de hardware/controle (pinos, PWM, remap, ganhos, limiares) |
| `imu_mpu9250.*` | Leitura da IMU + remap de orientação |
| `attitude_filter.*` | Filtro complementar (estimativa de atitude) |
| `control_cascade.*` | PID em cascata ângulo→taxa |
| `motor_mix.*` | Mixagem física + saturação por motor |
| `esc_out.*` | PWM dos ESCs (ESP32Servo) + arming |
| `failsafe.*` | Máquina de estados de segurança (latch) |
| `flight_task.*` | Tarefa de controle a 250 Hz (core 1) |
| `command_state.*` | Estado compartilhado (portMUX) + watchdog de comando |
| `web_routes.*` | Servidor HTTP (contrato das rotas do Drone-main) |
| `wifi_ap.*` | Ponto de acesso Wi-Fi (SoftAP) |
| `nvs_store.*` | Persistência de calibração (Preferences/NVS) |
| `index.html` | Console web (reaproveitado do Drone-main) |

Rastreabilidade: ver `../specs/001-firmware-juncao-voo/` (spec, plan, research, data-model,
contracts, tasks, quickstart).
