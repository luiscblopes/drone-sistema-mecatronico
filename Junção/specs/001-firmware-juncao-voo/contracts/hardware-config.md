# Contrato de Hardware/Controle (`config.h` da Junção)

Contrato **congelado** e **centralizado** (Princípio IV). Toda constante de pino, PWM, ganho,
remap e limiar mora aqui, **documentada individualmente** (significado físico + origem do
valor). Alterar qualquer item exige justificativa escrita e **revalidação em bancada**
(Princípio I/III). Valores ⚠️ são **confirmados em bancada** (quickstart.md) — os listados são
ponto de partida derivado das fontes.

## Motores / ESCs

| Constante | Valor | Significado / Origem |
|-----------|-------|----------------------|
| `NUM_MOTORS` | 4 | Quadricóptero em X |
| `ESC_GPIO_M1..M4` | 25, 26, 27, 14 | GPIOs dos ESCs na placa Drone-main (`PORTE` §3) |
| `ESC_MIN_US` / `ESC_MAX_US` | 1000 / 2000 | Faixa padrão de ESC (1 ms parado, 2 ms máx) |
| `ESC_FREQUENCY_HZ` | 250 | Validado nos ESCs desta placa (`PORTE` §4.4) |
| `ESC_ARM_HOLD_MS` | 5000 | Segura mínimo no boot p/ ESC reconhecer "throttle zero" |
| `THROTTLE_IDLE_US` | ⚠️ ~1170 | Piso por motor com malha ativa (sketch Arduino) |
| `THROTTLE_CUTOFF_US` | 1000 | Corte total (desarmado) |
| `MOTOR_OUTPUT_MAX_US` | 1999 | Teto de saturação por motor (sketch) |

## Mixagem (convenção física do Drone-main) — ⚠️ validar motor a motor

```
M1 (frente-esq) = T + P + R − Y
M2 (frente-dir) = T + P − R + Y
M3 (trás-dir)   = T − P − R − Y
M4 (trás-esq)   = T − P + R + Y
```
Origem: `drone_pid.c` / `PORTE` §4.6. **Não** herdar a mixagem do Arduino (inverte pitch e
erra a diagonal de yaw). Confirmar em quickstart §4/§5.

## IMU (MPU-9250 @ I2C)

| Constante | Valor | Significado / Origem |
|-----------|-------|----------------------|
| `I2C_SDA_GPIO` / `I2C_SCL_GPIO` | 21 / 22 | Igual nos dois projetos (`PORTE` §3) |
| `I2C_CLOCK_HZ` | 400000 | Fast mode |
| `IMU_ADDR` | 0x68 | MPU-9250 ≈ MPU-6050 no núcleo accel/gyro (`PORTE` §4.1) |
| Accel FS | ±8g (`/4096`, reg 0x1C=0x10) | Escala do sketch |
| Gyro FS | ±500°/s (`/65.5`, reg 0x1B=0x08) | Escala do sketch |
| DLPF | reg 0x1A | Filtro do sketch |
| `MPU_REMAP_ROTATE_Y_90` | 1 | Remap +90°Y: `x'=z, y'=y, z'=−x` (accel+gyro) — ⚠️ validar (quickstart §2) |
| `MPU_MAX_AGE_MS` | ~150 | Amostra mais velha ⇒ IMU inválida (failsafe) |

## Controle (laço a 250 Hz)

| Constante | Valor | Significado / Origem |
|-----------|-------|----------------------|
| `DT_S` | 0.004 | Período do laço (250 Hz) — base comprovada |
| `COMP_FILTER_GYRO_W` / `_ACC_W` | 0.991 / 0.009 | Pesos do filtro complementar (sketch) |
| `ANGLE_CLAMP_DEG` | ±20 | Clamp do ângulo estimado/comandado (sketch) |
| `PID_OUT_CLAMP` / `I_TERM_CLAMP` | ±400 | Saturação de saída e termo I (sketch) |
| Ângulo (ext.) `P/I/D` | 2 / 0.5 / 0.007 | Ganhos comprovados (roll=pitch) |
| Taxa roll/pitch `P/I/D` | 0.625 / 2.1 / 0.0088 | Ganhos comprovados |
| Taxa yaw `P/I/D` | 4 / 3 / 0 | Ganhos comprovados |
| `YAW_RATE_SCALE` | 0.15 | Escala do setpoint de taxa de yaw (sketch) |

## Segurança (failsafe — ver data-model §4)

| Constante | Valor | Origem |
|-----------|-------|--------|
| `FLIGHT_COMMAND_TIMEOUT_MS` | **500** | Clarifications (spec) |
| `MAX_SAFE_TILT_DEG` | **65** | Clarifications (spec) / `config.h` Drone-main |
| `CONTROL_MIN_THROTTLE_US` | ~1050 | Abaixo disso, motores em mínimo |

## Rede (AP)

| Constante | Valor |
|-----------|-------|
| `WIFI_AP_SSID` | `EQUIPE4-AP` |
| `WIFI_AP_PASSWORD` | `12345678` |
| Host | `192.168.4.1` |

## Persistência (NVS via `Preferences`)

| Item | Namespace / chaves |
|------|--------------------|
| Calibração de motor | `motor-cal` / `start%d`, `max%d`, `trim%d` |
| Offsets IMU | gyro/accel (substituem os hardcoded do `setup()`) |
