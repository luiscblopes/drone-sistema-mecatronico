# Análise dos três projetos — Base Arduino × Hardware Drone-main × Firmware Junção

> Documento de análise técnica, gerado a partir de uma leitura **linha a linha** dos três
> projetos. Objetivo: verificar se o firmware da pasta `Junção` **une corretamente** a lógica
> de voo comprovada do `ESP32-Flight-controller` (Arduino) ao hardware, à interface Wi-Fi e aos
> failsafes do `Drone-main`.

> ⚠️ **SEGURANÇA:** nada aqui substitui a validação **SEM HÉLICES** em bancada
> (`specs/001-firmware-juncao-voo/quickstart.md`). O código pode estar internamente consistente
> e ainda assim ter um eixo invertido — isso só a bancada revela.

---

## 1. Visão geral

| Projeto | Papel no resultado | Framework | Linhas (fonte) | Estado |
|---|---|---|---|---|
| **ESP32-Flight-controller** | Lógica de voo **comprovada** (angle-mode) | Arduino C++ (`.ino`) | ~855 | **voa** |
| **Drone-main** | Hardware + Wi-Fi + failsafes | ESP-IDF, **C puro** | ~5.000 | não voa |
| **Junção** (este) | Une os dois | Arduino-ESP32 **C++** | ~2.100 | escrito + revisado; **não compilado/voado** |

**Regra de junção:** lógica de **controle/sensor = Arduino** (a que voa); envelope de
**hardware/segurança/Wi-Fi = Drone-main**.

---

## 2. Fatos canônicos por projeto

### 2.1 Base Arduino — `ESP32-Flight-controller` (o que voa)

Arquivo principal: `src/Anglemode_flightcontroller_ver3.1.ino`.

- **IMU** MPU-6050 @ `0x68`, I2C 400 kHz; accel **±8 g (÷4096)**, gyro **±500 °/s (÷65.5)**,
  DLPF `0x05` (`.ino:293-324`).
- **Equações de ângulo** (accel): `roll = atan(AccY/√(AccX²+AccZ²))·57.29`,
  `pitch = −atan(AccX/√(AccY²+AccZ²))·57.29` (`.ino:335-336`).
- **Filtro complementar** `0.991/0.009`, saída clampada a **±20°** (`.ino:364-368`).
  (Kalman presente mas **comentado / código morto**.)
- **Controle em cascata** ângulo→taxa, PID **inline**, integral trapezoidal, derivada no erro,
  clamps de termo I e saída a **±400** (`.ino:378-444`). Ganhos exatos:
  - Ângulo R/P: **P=2, I=0.5, D=0.007**.
  - Taxa R/P: **P=0.625, I=2.1, D=0.0088**.
  - Taxa Yaw: **P=4, I=3, D=0**.
- **Laço 250 Hz** (`t=0.004 s`, busy-wait) (`.ino:31,615`).
- **Mixagem** (pinos 13/12/14/27):
  `M1=T−R−P−Y, M2=T−R+P+Y, M3=T+R+P−Y, M4=T+R−P+Y` (`.ino:453-456`).
- **ESC 500 Hz**, 1000–2000 µs; idle 1170, cutoff 1000, teto 1800 (`.ino:13,79-80,447`).
- **Failsafe único**: desarma se throttle < 1030 (`.ino:499`). **Sem detecção de perda de link
  RC** → risco de fly-away. Offsets de calibração **hardcoded** (`.ino:278-283`).

### 2.2 Drone-main — hardware/segurança (ESP-IDF, C)

- **Pinos** motores **25/26/27/14**; I2C **21/22**; ESC **250 Hz**; arming **5 s**
  (`config.h:32-35,57-58,27-28`).
- **IMU** MPU-9250: escalas **±2 g (÷16384)**, **±250 °/s (÷131)**, DLPF 20 Hz — **diferentes**
  da base Arduino.
- **Orientação da IMU** — *duas* compensações:
  1. **Remap +90°Y** a accel **e** gyro: `x'=z, y'=y, z'=−x` (`mpu9259.c:109-115`).
  2. **`CONTROL_SWAP_ROLL_PITCH=true`**: troca roll↔pitch (e gyro_x↔gyro_y) **só** no caminho de
     controle (`flight_control.c:206-212`). A telemetria `/sensors` **não** é trocada.
- **Mixagem** (`drone_pid.c:181-188`):
  `M1=T+P+R−Y, M2=T+P−R+Y, M3=T−P−R−Y, M4=T−P+R+Y` — desenhada para roll/pitch **já trocados**.
- **Laço 50 Hz**.
- **Failsafe com latch** — motivos: `COMMAND_TIMEOUT` (**600 ms**), `MPU_INVALID` (age **150 ms**),
  `MPU_NOT_CALIBRATED`, `EXCESSIVE_TILT` (**65°**), `MANUAL_OVERRIDE`, `EMERGENCY_STOP`. O latch
  trava o rearme; **só `/stopAll` limpa** (`flight_control.c:144-162`, `web_server.c:199-209`).
- **NVS** namespace `motor-cal`, chaves `start/max/trim0..3` (`calibration_store.c:18,59-97`).
- **Rotas HTTP** (todas GET): `/`, `/status`, `/health`, `/sensors`, `/setMotor`, `/stopAll`,
  `/setFlight`, `/flightStatus`, `/resetPid`, `/calibration`, `/setCalibration`,
  `/resetCalibration`, `/findDeadband` (+ verticais, fora de escopo). Códigos especiais: **423**
  (arming), **409** (latch), **503** (sem tarefa de controle).

### 2.3 Junção — o firmware novo (Arduino-ESP32, C++)

`firmware/`: `firmware.ino` + `config.h` + 12 módulos `.cpp/.h` + `index.html`. Duas tarefas
FreeRTOS: **controle 250 Hz no core 1**, **web no core 0**; estado compartilhado via
`command_state` (portMUX). Caminho congelado: IMU+remap → filtro complementar → cascata PID →
mixagem → ESC.

---

## 3. Matriz de fidelidade — a Junção uniu certo?

| Dimensão | Origem | Junção | Confere? |
|---|---|---|---|
| Escala accel/gyro, DLPF | **Arduino** (÷4096, ÷65.5, 0x05) | idêntico | ✅ casa com os ganhos mantidos |
| Filtro complementar 0.991/0.009, ±20° | **Arduino** | idêntico | ✅ |
| Ganhos PID (ângulo + taxa) | **Arduino** (exatos) | idênticos | ✅ |
| Laço 250 Hz, clamps ±400 | **Arduino** | idêntico | ✅ (decisão de clarify) |
| Pinos 25/26/27/14 | **Drone-main** | idêntico | ✅ |
| ESC 250 Hz, 1000–2000 µs, arming 5 s | **Drone-main** | idêntico | ✅ (Arduino era 500 Hz) |
| Remap IMU +90°Y (accel+gyro) | **Drone-main / porte** | idêntico | ✅ |
| Mixagem `M1=T+P+R−Y…` | **Drone-main** | idêntico | ✅ |
| Failsafe latch + e-stop | **Drone-main** | portado | ✅ |
| Timeout de comando | **divergência deliberada** (500 ms; Drone usa 600) | 500 ms | ✅ documentado |
| Tilt 65°, IMU age 150 ms | **Drone-main** | idêntico | ✅ |
| Rotas HTTP + NVS | **Drone-main** (contrato) | replicado | ✅ |

**Conclusão:** a separação está **fiel** ao plano — pipeline sensor+controle do Arduino +
envelope hardware+segurança do Drone-main, exatamente como o `PORTE_PARA_HARDWARE_Drone-main.md`
recomendou (Estratégia A/B combinadas).

---

## 4. ⚠️ Ponto de risco nº 1 — orientação da IMU × mixagem

Este é o achado mais importante da análise cruzada, e **só a bancada resolve**.

O Drone-main usa **duas** compensações de orientação: o **remap +90°Y** *e* o
**swap roll↔pitch** (`CONTROL_SWAP_ROLL_PITCH=true`). A mixagem dele (`M1=T+P+R−Y…`) foi
desenhada para roll/pitch **já trocados**.

A Junção aplica **apenas o remap +90°Y** (seguindo a abordagem do Arduino), **sem o swap**, mas
usa a **mixagem do Drone-main**. Ou seja: a mixagem que espera roll/pitch trocados está sendo
alimentada com roll/pitch na convenção do Arduino.

```
Drone-main:  IMU → remap +90°Y → SWAP roll↔pitch → mixagem(M1=T+P+R−Y…)
Junção:      IMU → remap +90°Y → (sem swap)       → mixagem(M1=T+P+R−Y…)   ← verificar!
```

**Isso pode estar certo** (se o remap do Arduino já posiciona os eixos como a mixagem espera)
**ou pode trocar/inverter um eixo** (se faltar o swap). É **impossível decidir estaticamente** —
depende dos eixos físicos reais do airframe.

### Como validar na bancada (SEM HÉLICES)
1. **`quickstart` §2 (IMU nivelada/sinais):** nivelada ⇒ roll≈0, pitch≈0 (±2°). Inclinar frente
   ⇒ pitch no sinal esperado; inclinar direita ⇒ roll no sinal esperado. **Se roll e pitch
   reagirem trocados, falta o swap.**
2. **`quickstart` §5 (resposta da malha):** inclinar à mão ⇒ os **motores certos** aceleram para
   corrigir. Se os errados acelerarem ou corrigir para o lado errado, é remap/mixagem.

### Mitigação recomendada
Tornar o swap **opcional e configurável** em `config.h` (desligado por padrão), para testar os
dois casos na bancada sem reescrever código — ver §6.

---

## 5. Bugs e riscos encontrados

### 5.1 Na Junção (corrigidos)
| # | Severidade | Item | Status |
|---|---|---|---|
| 1 | BLOCKER | Ordem indefinida de `Wire.read()<<8 \| Wire.read()` (byte-swap da IMU) | ✅ corrigido (`mpu_read_i16`) |
| 2 | BUG | Acesso ao ESC pelos 2 cores sem lock | ✅ corrigido (`portMUX` em `esc_out`) |
| 3 | BUG | `failsafe_trip` mascarava o motivo do latch | ✅ corrigido |
| 4 | RISK | NaN "grudento" no filtro (0/0) | ✅ corrigido (`fmaxf`) |
| 5 | RISK | `snprintf` acumulado com underflow de `size_t` | ✅ corrigido (`json_append`) |
| 6 | RISK | Offsets da IMU compartilhados entre cores sem lock | ✅ corrigido (`portMUX` em `imu_mpu9250`) |

Remanescente aceitável: `/findDeadband` usa `delay()` longo no core 0 (bloqueia só o servidor
web; é ferramenta de bancada). **Sem bloqueadores de compilação** apontados pela auditoria.

### 5.2 Nos projetos-fonte (contexto — não alterados)
- **Arduino:** sem failsafe de perda de link RC (fly-away); reescreve registradores I2C todo
  ciclo; `while(...);` com `;` solto (`.ino:615`).
- **Drone-main:** senha Wi-Fi fraca hardcoded (`12345678`); `/sensors` mostra roll/pitch
  **não-trocados** enquanto o controle usa **trocados** — pegadinha de sintonia.

---

## 6. Sugestão: swap roll↔pitch opcional (cobre o risco nº 1)

Para não ficar refém de uma única hipótese de orientação, recomenda-se adicionar ao `config.h`:

```c
/* Se 1, troca roll<->pitch (e suas taxas) apos o remap, como o Drone-main faz
 * (CONTROL_SWAP_ROLL_PITCH). Manter 0 por padrao (abordagem do Arduino) e so
 * ligar se a bancada (quickstart §2/§5) mostrar roll/pitch trocados. */
#define CONTROL_SWAP_ROLL_PITCH (0)
```

…aplicado no `imu_mpu9250` (ou no `attitude_filter`) logo após o remap. Assim o teste de bancada
vira "ligar/desligar uma flag" em vez de reescrever a mixagem — muito mais seguro.

---

## 7. Veredito

- **União conceitual: correta e fiel** — a matriz de fidelidade (§3) fecha em todas as
  dimensões.
- **Código: consistente e revisado** por três análises independentes; 6 bugs reais corrigidos;
  sem bloqueador de compilação aparente.
- **Não se pode afirmar que voa.** Dois portões continuam abertos e **exigem hardware**:
  1. **Compilar:** `arduino-cli compile --fqbn esp32:esp32:esp32 firmware`.
  2. **Bancada SEM HÉLICES** (`quickstart.md`), com atenção máxima ao **risco nº 1** (§4) nos
     passos 2 e 5.

---

## Referências
- `docs/PORTE_PARA_HARDWARE_Drone-main.md` — análise de migração original.
- `specs/001-firmware-juncao-voo/` — spec, plan, research, data-model, contracts, tasks,
  quickstart.
- Base: `ESP32-Flight-controller--main/src/Anglemode_flightcontroller_ver3.1.ino`.
- Hardware: `Drone-main/main/{config.h,mpu9259.c,drone_pid.c,flight_control.c,web_server.c}`.
- Firmware: `Junção/firmware/`.
