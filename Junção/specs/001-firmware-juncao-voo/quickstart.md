# Quickstart — Roteiro de Bancada SEM HÉLICES (portão de aceite)

> ⚠️ **SEGURANÇA (NÃO-NEGOCIÁVEL):** todo este roteiro é executado **SEM HÉLICES**, com o
> quadricóptero **preso à bancada**. Motores e hélices têm energia destrutiva. Voo só **depois**
> de todos os passos aprovados (SC-009). Baseado no `TESTE_BANCADA.md` do Drone-main e na
> seção 8 do `PORTE_PARA_HARDWARE_Drone-main.md`.

Este guia **valida** que o firmware da Junção atende à spec. Cada passo aponta o critério de
sucesso (SC-xxx) e o(s) requisito(s) (FR-xxx) que comprova. Não contém implementação — apenas
como **rodar e observar**.

## Pré-requisitos

- Placa do Drone-main energizada por fonte de bancada, **sem hélices**, presa.
- IMU MPU-9250 (I2C 21/22), 4 ESCs nos GPIO 25/26/27/14.
- `arduino-cli` com core `esp32:esp32` e lib `ESP32Servo` instalados.
- Um navegador (PC/celular) para o console web.

## Build & Flash

```bash
# a partir da pasta do firmware (Junção/firmware)
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload  --fqbn esp32:esp32:esp32 -p <PORTA> .
# monitor serial opcional
arduino-cli monitor -p <PORTA> -c baudrate=115200
```

Conecte ao Wi-Fi **`EQUIPE4-AP`** (senha `12345678`) e abra **http://192.168.4.1**.

---

## Passo 1 — Boot e integridade (SC — pré-condição; FR-008/FR-009/FR-006)

- **Ação**: energizar; abrir `GET /health` e `GET /status`.
- **Esperado**: `ok:true`; `commandTask/telemetryTask/flightTask = true`;
  `motorsAtMinimum:true`; `arming:true` nos primeiros ~5 s e depois `false`; IMU detectada em
  0x68 (sem erro no serial). Motores **não** giram.

## Passo 2 — IMU nivelada e sinais (SC-001, SC-002; FR-007)

- **Ação**: placa **parada e nivelada** → ler atitude (`/sensors` ou `/flightStatus`). Depois
  **inclinar para a frente** e **para a direita**.
- **Esperado**:
  - Nivelada: `roll ≈ 0°` e `pitch ≈ 0°` dentro de **±2°** (valida o remap +90°Y — research D5).
  - Inclinar frente ⇒ `pitch` varia no **sinal esperado**; inclinar direita ⇒ `roll` no sinal
    esperado. **Nenhum eixo trocado/invertido.**
- **Se falhar**: revisar o remap (D5) — não prosseguir (a malha "corrigiria" para o lado errado).

## Passo 3 — Deadband por motor (FR-015; insumo da calibração)

- **Ação**: para cada motor `N` (0..3):
  ```
  GET /findDeadband?motor=N&from=1000&to=1250&step=2&dwell=400
  ```
- **Esperado**: registrar o pulso (µs) em que **aquele** motor começa a girar. Anotar para a
  calibração `start`.

## Passo 4 — Motor individual: pino e sentido (SC-003; FR-008, FR-015)

- **Ação**: com sistema desarmado, `GET /setMotor?motor=N&speed=20` para cada `N`.
- **Esperado**: gira **apenas** o motor físico correto: **M0→GPIO25, M1→GPIO26, M2→GPIO27,
  M3→GPIO14**. Confirmar **sentido de rotação** (CW/CCW) de cada um conforme o diagrama do
  airframe; se errado, inverter 2 das 3 fases motor↔ESC. **Nenhum motor cruzado.**

## Passo 5 — Resposta da malha / mixagem (SC-004, SC-010; FR-002, FR-005)

- **Ação**: armar e engatar estabilização com setpoints zero:
  ```
  GET /setFlight?throttle=1300&rollSp=0&pitchSp=0&yawSp=0&apply=1&stabilize=1
  ```
  (reenviar a cada < 500 ms — ver Passo 6). Com baixo throttle e **sem hélices**, **inclinar a
  placa na mão**.
- **Esperado**: os **motores corretos aceleram** para corrigir a inclinação imposta (ex.:
  inclinar para a direita ⇒ motores do lado baixo aceleram). Resposta **estável**, sem
  oscilação crescente. Isto valida **mixagem + sinais + sentido da malha** (D6).
- **Se falhar** (motores errados aceleram / corrige para o lado errado): revisar mixagem (D6)
  e/ou remap (D5). **Bloqueia voo.**

## Passo 6 — Failsafes com latch (SC-005, SC-007; FR-016..FR-020)

Disparar **cada** failsafe e confirmar **latch** (rearme só após `/stopAll`):

| Failsafe | Como disparar | Esperado |
|----------|---------------|----------|
| Perda de comando | parar de enviar `/setFlight` | motores ao mínimo em **≤ 500 ms**; `failsafe=COMMAND_TIMEOUT`; `latched=true` |
| IMU inválida | desconectar/segurar IMU sem amostra | recusa armar / corta; `failsafe=IMU_INVALID` |
| Inclinação excessiva | inclinar a placa **> 65°** | corta; `failsafe=EXCESSIVE_TILT`; `latched=true` |
| Parada de emergência | `GET /stopAll` | corta imediato; limpa latch (destrava) |

- **Rearme**: com latch ativo, `GET /setFlight?...&stabilize=1` deve responder **409**. Após
  `GET /stopAll`, rearmar deve ser aceito. Repetir cada caso para confirmar reprodutibilidade
  (100%, SC-005).

## Passo 7 — Calibração persistida (SC-008; FR-021, FR-022)

- **Ação**: gravar calibração e reiniciar:
  ```
  GET /setCalibration?start0=1120&max0=2000&trim0=0&start1=1110&max1=2000&trim1=0&start2=1120&max2=2000&trim2=0&start3=1115&max3=2000&trim3=0
  ```
  Reiniciar a placa; ler `GET /calibration`.
- **Esperado**: valores **idênticos** aos gravados após reboot (persistência NVS).

## Passo 8 — Comando exclusivamente por Wi-Fi (SC-006; FR-011, FR-012, FR-013)

- **Ação**: confirmar que **todo** o roteiro acima foi feito pelo console/HTTP, **sem rádio RC**.
- **Esperado**: setpoints, telemetria e calibração 100% por Wi-Fi.

---

## Critério de liberação (SC-009)

✅ Só após **todos** os passos 1–8 aprovados o sistema é considerado **seguro para voo**.
Qualquer reprovação **bloqueia** o voo e exige correção + nova rodada de bancada (Princípio V).
Voo: hélices, ao ar livre, com extrema cautela.

## Referências
- `contracts/http-api.md` — parâmetros e respostas das rotas.
- `contracts/hardware-config.md` — pinos, remap, mixagem, limiares.
- `research.md` — decisões D5 (remap), D6 (mixagem), D7 (ganhos), D10 (failsafes).
- `data-model.md` — entidades de estado e transições de failsafe.
