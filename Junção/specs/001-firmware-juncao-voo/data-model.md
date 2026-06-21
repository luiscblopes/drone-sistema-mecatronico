# Phase 1 — Data Model: Firmware de Junção

Entidades de **estado em memória** do firmware (não há banco de dados). Cada entidade lista
campos, faixas/unidades, regras de validação e relações. Persistência: apenas a **Calibração**
vai à NVS (ver `research.md` D11). Rastreável às Key Entities da `spec.md`.

---

## 1. Estado de Atitude (`AttitudeState`)

Atitude estimada e taxas, produzidas pela IMU + filtro complementar. Insumo da malha e da
telemetria.

| Campo | Tipo | Unidade/Faixa | Origem / Regra |
|-------|------|---------------|----------------|
| `roll_deg` | float | grau, clamp **±20°** | Filtro complementar (0.991/0.009) após remap +90°Y |
| `pitch_deg` | float | grau, clamp **±20°** | idem |
| `rate_roll_dps` | float | °/s | Gyro `/65.5`, pós-remap, menos offset de calibração |
| `rate_pitch_dps` | float | °/s | idem |
| `rate_yaw_dps` | float | °/s | idem |
| `accel_xyz` | float[3] | g (±8g, `/4096`) | Accel pós-remap, menos offset; usado nas eqs. de ângulo |
| `sample_age_ms` | uint32 | ms | Idade da última amostra; **> ~150 ms ⇒ IMU inválida** (failsafe) |
| `valid` | bool | — | IMU detectada (0x68) e amostra recente |

**Regras**: nivelada e parada ⇒ `roll_deg ≈ 0`, `pitch_deg ≈ 0` (±2°, SC-001). `valid=false`
ou `sample_age_ms` excedido ⇒ failsafe de IMU (bloqueia armar). Remap aplicado **antes** do
cálculo de ângulo (D5).

## 2. Setpoint de Voo (`FlightSetpoint`)

Comando recebido por Wi-Fi; entrada da malha em angle-mode.

| Campo | Tipo | Unidade/Faixa | Regra (saturação no recebimento) |
|-------|------|---------------|----------------------------------|
| `throttle_us` | float | µs, **1000..2000** | `clamp(ESC_MIN_US, ESC_MAX_US)` |
| `roll_setpoint_deg` | float | grau, **±20°** | `clamp(±20)` (envelope proven, D8) |
| `pitch_setpoint_deg` | float | grau, **±20°** | `clamp(±20)` |
| `yaw_setpoint_dps` | float | °/s | Taxa de guinada (escala 0.15 do sketch) |
| `stabilize` | bool | — | `apply=1 & stabilize=1` engata a malha |
| `apply` | bool | — | Aplica saída aos motores |

**Relações**: alimentado por `/setFlight` (contrato em `contracts/http-api.md`). Cada recepção
válida chama o watchdog (`mark_command`) → reinicia o timeout de 500 ms (entidade Failsafe).

## 3. Saída/Comando de Motor (`MotorOutput`)

Resultado da mixagem aplicado a cada ESC.

| Campo | Tipo | Unidade/Faixa | Regra |
|-------|------|---------------|-------|
| `output_us[4]` | int[4] | µs, **1000..2000** | Mixagem (D6) + saturação: teto 1999; piso `ThrottleIdle`(~1170) |
| `cutoff` | bool | — | `throttle` abaixo do mínimo seguro ⇒ todos em `ESC_CUTOFF`(1000) e reset dos termos PID |
| `arming` | bool | — | true durante `ESC_ARM_HOLD_MS` (~5 s) no boot; recusa comandos (423) |

**Mixagem (índices físicos)**: `M1=frente-esq, M2=frente-dir, M3=trás-dir, M4=trás-esq`,
mapeados a GPIO 25/26/27/14 (D3/D6). Saturação por motor espelha o sketch (teto/idle/cutoff).

## 4. Estado de Armar/Failsafe (`SafetyState`)

Máquina de estados de segurança; governa se o acionamento é permitido. **Latch**.

| Campo | Tipo | Valores | Regra |
|-------|------|---------|-------|
| `enabled` | bool | — | Malha estabilizada ativa |
| `arming` | bool | — | ESCs ainda armando (boot) |
| `failsafe_reason` | enum | `NONE, COMMAND_TIMEOUT, IMU_INVALID, EXCESSIVE_TILT, EMERGENCY_STOP, MANUAL_OVERRIDE` | Motivo corrente |
| `latched` | bool | — | true após motivo perigoso; **só `/stopAll` limpa** |
| `command_age_ms` | uint32 | ms | Tempo desde o último comando; **> 500 ⇒ COMMAND_TIMEOUT** |

**Transições**:
- `NONE → COMMAND_TIMEOUT`: `command_age_ms > 500`.
- `NONE → IMU_INVALID`: IMU ausente/idade excedida/não calibrada.
- `NONE → EXCESSIVE_TILT`: `|roll|>65°` ou `|pitch|>65°`.
- `NONE → EMERGENCY_STOP`: `/stopAll`.
- `* → (motores mínimo, enabled=false, latched=true)` para motivos perigosos.
- `latched → NONE`: somente `/stopAll` (`clear_failsafe_latch`).
- Enquanto `latched`, `/setFlight?...&stabilize=1` ⇒ **409**.

## 5. Calibração (`Calibration`) — **persistida (NVS)**

| Campo | Tipo | Faixa | Persistência |
|-------|------|-------|--------------|
| `motor_start_us[4]` | int[4] | µs (default 1100) | NVS `motor-cal/start%d` |
| `motor_max_us[4]` | int[4] | µs (default 2000) | NVS `motor-cal/max%d` |
| `motor_trim_us[4]` | int[4] | µs, **−100..+100** | NVS `motor-cal/trim%d` |
| `gyro_offset[3]` | float | °/s | NVS (offsets de gyro) |
| `accel_offset[3]` | float | g | NVS (offsets de accel) |

**Regras**: carregada no boot antes do laço; gravada por `/setCalibration` e
`/resetCalibration`; valores sanitizados (trim clamp ±100). Substitui os offsets hardcoded do
`setup()` do Arduino (D11). Lida de volta deve igualar o gravado (SC-008).

## 6. Contrato de Hardware (`config.h`) — **constantes**

Não é estado de runtime; é o **contrato congelado** centralizado. Detalhado em
`contracts/hardware-config.md`. Resumo: pinos (I2C 21/22; motores 25/26/27/14), `ESC_MIN/MAX_US`
1000/2000, `ESC_FREQUENCY_HZ` 250, `MPU_REMAP_ROTATE_Y_90`, mixagem, ganhos PID, `dt=0.004`,
`COMMAND_TIMEOUT_MS=500`, `MAX_SAFE_TILT_DEG=65`, `MPU_MAX_AGE_MS`, AP SSID/senha.

## 7. Telemetria (`Telemetry`) — derivada (somente leitura)

Projeção das entidades acima exposta via HTTP/console: atitude (1), saídas de motor (3),
estado de armar/failsafe (4) e calibração (5). Formato JSON conforme `contracts/http-api.md`
(`/status`, `/sensors`, `/flightStatus`, `/calibration`). Sem estado próprio.

---

### Diagrama de fluxo (entidades no laço de 250 Hz)

```text
IMU(0x68) ──remap D5──> AttitudeState(1) ──┐
                                           ├─> ControlCascade ──> MotorOutput(3) ──> ESC(250Hz)
FlightSetpoint(2) ◄── /setFlight ──────────┘            ▲
        │ mark_command                                  │ gate
        ▼                                               │
   SafetyState(4) ──latch──────────────────────────────┘
Calibration(5) ──boot load / NVS──> (offsets IMU + por-motor)
Telemetry(7) ◄── projeção de 1,3,4,5 ──> /status,/sensors,/flightStatus,/calibration
```
