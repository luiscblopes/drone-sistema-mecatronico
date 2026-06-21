# Implementation Plan: Firmware de Junção — Voo em Angle-Mode por Wi-Fi

**Branch**: `001-firmware-juncao-voo` | **Date**: 2026-06-21 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `specs/001-firmware-juncao-voo/spec.md`

## Summary

Gerar um firmware **Arduino-ESP32** novo na pasta `Junção` que roda na **placa do
Drone-main**, unindo: (a) a **lógica de voo comprovada** do `ESP32-Flight-controller`
(angle-mode, filtro complementar `0.991/0.009`, PID em cascata ângulo→taxa a **250 Hz**,
clamps ±20°/±400) e (b) a **infraestrutura de comando e segurança** do `Drone-main`
(AP Wi-Fi, console `index.html`, contrato das rotas HTTP, failsafes com **latch** e
calibração persistida). Decisão de framework (ver Clarifications da spec): **Estratégia A**
— manter o código Arduino e **adaptá-lo ao hardware**, **reimplementando** em Arduino-ESP32
as camadas que no Drone-main eram ESP-IDF/C (Wi-Fi/web/NVS), reaproveitando o `index.html`
e **replicando o contrato** das rotas (não se reusa o código-fonte C do ESP-IDF).

Abordagem técnica: a lógica de voo do sketch é preservada como **caminho congelado**, mas
adaptada em quatro pontos obrigatórios re-derivados e validados em bancada — **pinos**
(25/26/27/14), **frequência de ESC** (250 Hz), **remap de orientação da IMU** (+90° em Y a
accel e gyro) e **mixagem** (convenção física do Drone-main). O comando RC é **removido** e
substituído pelos **setpoints via HTTP**. Failsafes (timeout 500 ms, IMU inválida/não
calibrada, tilt 65°, e-stop) com latch destravável por `/stopAll`. A entrega só é aceita após
o **roteiro de bancada sem hélices** (quickstart.md).

## Technical Context

**Language/Version**: C++ (Arduino), core **arduino-esp32** (ESP32 Arduino) — alvo de
compilação `arduino-cli`/Arduino IDE.

**Primary Dependencies**:
- `Wire.h` (I2C, IMU MPU-9250 em 0x68).
- `ESP32Servo.h` (geração de PWM de ESC por `writeMicroseconds`, `setPeriodHertz(250)`).
- `WiFi.h` (modo SoftAP — substitui `wifi_ap.c` do Drone-main).
- `WebServer.h` (servidor HTTP/rotas — substitui `esp_http_server`/`web_server.c`).
- `Preferences.h` (persistência em NVS — substitui `nvs_flash`/`calibration_store.c`).
- FreeRTOS (já embutido no core arduino-esp32) para a tarefa de controle dedicada.

**Storage**: NVS via `Preferences` — namespace `motor-cal`, chaves `start%d/max%d/trim%d` e
offsets de calibração da IMU (mesma semântica do `calibration_store.c`).

**Testing**: Validação **em hardware**, na bancada, **sem hélices**, dirigida por
`quickstart.md` (boot/health, IMU nivelada, deadband por motor, sentido de rotação, resposta
da malha, failsafes). Não há framework de teste unitário embarcado; os critérios de aceite
são observáveis via console web e telemetria HTTP. Cálculos puros (mixagem, saturação) podem
ter verificação de mesa (planilha/host) como apoio.

**Target Platform**: ESP32 (placa do `Drone-main`). IMU MPU-9250 no I2C **SDA 21 / SCL 22**;
motores nos GPIO **25/26/27/14**; ESC **1000–2000 µs**.

**Project Type**: Firmware embarcado de controlador de voo (single project, sketch Arduino +
módulos de apoio).

**Performance Goals**: Laço de controle **250 Hz** determinístico (período 4 ms, jitter
mínimo); failsafe de perda de comando atuando em **≤ 500 ms**; servidor web responsivo sem
roubar o tempo do laço de controle.

**Constraints**:
- **Caminho congelado**: geração de PWM + laço de controle não mudam sem justificativa escrita
  e revalidação em bancada.
- **Sem hélices** em toda verificação.
- **Coexistência de rádio**: apenas Wi-Fi como canal (sem BT simultâneo).
- **Concorrência**: controle a 250 Hz e o `WebServer` não podem disputar o mesmo tempo de CPU
  de forma a violar o período do laço → tarefas FreeRTOS separadas + seção crítica nos dados
  compartilhados.

**Scale/Scope**: 1 ESP32, 4 ESCs, 1 IMU, 1 operador via navegador. ~10 rotas HTTP, 1 console
HTML reaproveitado. Sem GPS/barômetro/altitude nesta feature.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Princípio | Gate | Status |
|-----------|------|--------|
| I. Segurança de Voo (NÃO-NEGOCIÁVEL) | Caminho de PWM/controle congelado; failsafes obrigatórios com latch; bring-up sem hélices | **PASS** — caminho congelado declarado; 4 failsafes com latch (FR-016..020); quickstart sem hélices é portão de aceite |
| II. Código 100% Comentado pt-BR (NÃO-NEGOCIÁVEL) | `@file`/`@brief`, doc por função, porquê de blocos, origem física de constantes | **PASS (compromisso)** — exigido em todas as tasks; revisão bloqueia se faltar (FR-023) |
| III. Base Comprovada em Voo | Ganhos/filtro/taxa do Arduino como linha de base; desvio justificado e revalidado | **PASS** — 250 Hz e ganhos preservados; filtro complementar `0.991/0.009` mantido |
| IV. Contrato de Hardware Centralizado | Pinos/PWM/mixagem/remap/limiares centralizados e documentados; mixagem e IMU re-derivadas | **PASS** — `config.h` central da Junção; mixagem e remap re-derivados (research.md) e validados (quickstart §5/§2) |
| V. Validação em Bancada Antes do Voo | Roteiro sem hélices obrigatório antes de voo | **PASS** — quickstart.md é o roteiro; SC-009 é portão |

**Resultado**: nenhuma violação. **Complexity Tracking** vazio.

Nota de governança: a escolha de framework **Arduino** está alinhada à constitution
(Restrições Técnicas) — **nenhuma emenda necessária**. A reimplementação da camada
Wi-Fi/web/NVS em Arduino-ESP32 é o custo decorrente da Estratégia A, e usa bibliotecas
padrão do core (não recria infraestrutura do zero).

## Project Structure

### Documentation (this feature)

```text
specs/001-firmware-juncao-voo/
├── plan.md              # Este arquivo (/speckit-plan)
├── research.md          # Fase 0: decisões re-derivadas (pinos, remap, mixagem, taxa, build)
├── data-model.md        # Fase 1: entidades de estado (atitude, setpoint, motor, failsafe, calibração)
├── quickstart.md        # Fase 1: roteiro de bancada SEM HÉLICES (portão de aceite)
├── contracts/
│   ├── http-api.md      # Contrato das rotas HTTP (replicado do Drone-main)
│   └── hardware-config.md  # Contrato de hardware (config.h da Junção): pinos, PWM, remap, mixagem, limiares
├── checklists/
│   └── requirements.md  # Checklist de qualidade da spec (já gerado)
└── tasks.md             # Fase 2: /speckit-tasks (NÃO criado por /speckit-plan)
```

### Source Code (repository root)

Estrutura do firmware na pasta `Junção` (sketch Arduino + módulos de apoio `.h/.cpp`,
organizados por responsabilidade, espelhando os módulos do Drone-main mas em Arduino-ESP32):

```text
Junção/
├── firmware/
│   ├── firmware.ino            # setup()/loop(): boot, init de subsistemas, cria tarefas
│   ├── config.h                # CONTRATO de hardware/controle centralizado (pinos, PWM, remap, mixagem, ganhos, limiares)
│   ├── imu_mpu9250.h/.cpp      # Leitura IMU (0x68), escalas, REMAP +90°Y, offsets de calibração
│   ├── attitude_filter.h/.cpp  # Filtro complementar 0.991/0.009 + clamps ±20° (base Arduino)
│   ├── control_cascade.h/.cpp  # PID cascata ângulo→taxa (ganhos do Arduino, dt=0.004), clamps ±400
│   ├── motor_mix.h/.cpp        # Mixagem física do Drone-main + saturação/idle/cutoff por motor
│   ├── esc_out.h/.cpp          # ESP32Servo: attach/250 Hz/arming; escrita por motor
│   ├── failsafe.h/.cpp         # Máquina de estados de failsafe com LATCH (timeout/IMU/tilt/e-stop)
│   ├── flight_task.h/.cpp      # Tarefa FreeRTOS de controle a 250 Hz (caminho congelado)
│   ├── command_state.h/.cpp    # Setpoints/estado compartilhado (seção crítica/portMUX) + watchdog de comando
│   ├── web_routes.h/.cpp       # WebServer: rotas HTTP replicando o contrato do Drone-main
│   ├── wifi_ap.h/.cpp          # SoftAP (SSID/senha do contrato)
│   ├── nvs_store.h/.cpp        # Preferences: persistência de calibração (motores + IMU)
│   └── index.html              # Console web reaproveitado do Drone-main (servido em "/")
└── README.md                   # Visão geral + ponteiro para specs/ e quickstart
```

**Structure Decision**: **Single project** (firmware embarcado). O sketch `firmware.ino` é o
ponto de entrada; a lógica é dividida em módulos de responsabilidade única para honrar o
Princípio II (comentário/documentação) e o Princípio IV (contrato centralizado em `config.h`).
A separação **tarefa de controle (core 1, 250 Hz) × servidor web (core 0)** preserva o caminho
congelado. Os nomes de módulo espelham os do Drone-main (`esc_pwm`, `flight_control`,
`web_server`, `calibration_store`) para rastreabilidade, mas implementados em Arduino-ESP32.

## Complexity Tracking

> Sem violações de constitution — seção vazia (nada a justificar).

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|--------------------------------------|
| — | — | — |
