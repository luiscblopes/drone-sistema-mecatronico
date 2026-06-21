# Controladora de Voo ESP32 — Mestrado

Repositório do trabalho de mestrado sobre uma **controladora de voo (flight controller) para
quadricóptero** baseada em **ESP32**, comandada por **Wi-Fi**. Reúne três projetos: dois de
origem (um que **voa** e um com o **hardware/Wi-Fi**) e o resultado **oficial** que une os dois.

> ⚠️ **SEGURANÇA (NÃO-NEGOCIÁVEL):** toda verificação é feita **SEM HÉLICES**, na bancada.
> Motores e hélices têm energia destrutiva. Nenhum voo antes de passar no roteiro de bancada.

---

## Os três projetos

| Pasta | Papel | Framework | Estado |
|---|---|---|---|
| [`ESP32-Flight-controller--main`](./ESP32-Flight-controller--main) | Lógica de voo **comprovada** (angle-mode, PID em cascata) | Arduino C++ (`.ino`) | **voa** |
| [`Drone-main`](./Drone-main) | **Hardware** + **Wi-Fi** + failsafes | ESP-IDF (C puro) | não voa |
| [**`Junção`**](./Junção) | **★ O OFICIAL** — une os dois | Arduino-ESP32 (C++) | escrito + revisado |

A ideia central: **pegar a lógica que já voa** (do projeto Arduino) e **rodá-la no hardware**
do `Drone-main` (placa ESP32 com IMU MPU-9250 e comando por Wi-Fi, sem rádio RC), incorporando
os **failsafes** do `Drone-main`. O resultado é a pasta **`Junção`**.

---

## ★ O firmware oficial: `Junção`

O firmware de produção fica em [`Junção/firmware/`](./Junção/firmware). Ele estabiliza o
quadricóptero em **angle-mode** e é comandado **exclusivamente por Wi-Fi** (console web no
navegador, sem app, sem rádio RC).

### Como funciona (resumo)

```
[Celular/PC] --Wi-Fi--> [ESP32 cria AP "EQUIPE4-AP"] --> navegador em 192.168.4.1
                              │
   IMU (MPU-9250) ─ remap ─> filtro complementar ─> PID em cascata (250 Hz) ─> mixagem ─> 4 ESCs
                              │                                                     ▲
   rotas HTTP (setpoints) ───┘                          failsafes com latch ───────┘
```

- **Duas tarefas FreeRTOS**: controle a **250 Hz no núcleo 1** (caminho de tempo real) e
  servidor web no **núcleo 0** — assim o web não atrapalha o laço de controle.
- **Estimador**: filtro complementar (mesma lógica comprovada do Arduino).
- **Controle**: PID em cascata ângulo→taxa, a 250 Hz, com os **ganhos comprovados em voo**.
- **Comando**: setpoints (roll, pitch, yaw, throttle) chegam pelas rotas HTTP.
- **Segurança**: failsafes que **travam (latch)** o rearme até o operador mandar "PARAR TUDO".

---

## Hardware e pinagem

Placa-alvo: **ESP32** (a do `Drone-main`). Sensor: **IMU MPU-9250** (9 eixos; usa-se só
accel+gyro). Atuadores: **4 ESCs** (1000–2000 µs).

### Pinos (GPIO) — contrato oficial da `Junção`

| Função | GPIO | Observação |
|---|---|---|
| **Motor M1** (frente-esquerda) | **25** | ESC, PWM 250 Hz |
| **Motor M2** (frente-direita) | **26** | ESC, PWM 250 Hz |
| **Motor M3** (traseira-direita) | **27** | ESC, PWM 250 Hz |
| **Motor M4** (traseira-esquerda) | **14** | ESC, PWM 250 Hz |
| **I2C SDA** (IMU) | **21** | barramento I2C @ 400 kHz |
| **I2C SCL** (IMU) | **22** | barramento I2C @ 400 kHz |

> A **re-pinagem** dos motores foi necessária: o projeto Arduino usava 13/12/14/27 (com conflito
> nos pinos 25/26, que nesta placa são motores). O contrato acima é o **da placa do `Drone-main`**.

### Parâmetros de hardware/controle (em `Junção/firmware/config.h`)

| Parâmetro | Valor | Origem |
|---|---|---|
| Faixa de pulso do ESC | **1000–2000 µs** | comum aos dois |
| Frequência do ESC | **250 Hz** | Drone-main (Arduino usava 500 Hz) |
| Arming (segura mínimo no boot) | **5 s** | Drone-main |
| Taxa do laço de controle | **250 Hz** (dt = 0,004 s) | Arduino (base comprovada) |
| Filtro complementar | pesos **0,991 / 0,009**, clamp ±20° | Arduino |
| Remap da IMU | **+90° em Y** (`x'=z, y'=y, z'=−x`) | Drone-main |
| Endereço da IMU | **0x68** | — |
| Escala accel / gyro | **±8 g (÷4096)** / **±500 °/s (÷65,5)** | Arduino |
| Wi-Fi (AP) | SSID **`EQUIPE4-AP`**, senha **`12345678`**, host **192.168.4.1** | Drone-main |

---

## Failsafes (com latch)

Qualquer condição perigosa leva os motores ao mínimo, desabilita a malha e **trava o rearme**
até o operador comandar `/stopAll`:

| Failsafe | Disparo |
|---|---|
| Perda de comando | sem comando Wi-Fi válido por **> 500 ms** |
| IMU inválida | IMU ausente / amostra velha / não calibrada |
| Inclinação excessiva | `roll` ou `pitch` acima de **65°** |
| Parada de emergência | comando `/stopAll` do operador |

---

## Rotas HTTP (console web)

Conecte ao Wi-Fi `EQUIPE4-AP` e abra `http://192.168.4.1`. Rotas principais:

| Rota | Função |
|---|---|
| `GET /` | Console web (`index.html`) |
| `GET /setFlight?throttle&rollSp&pitchSp&yawSp&apply&stabilize` | Setpoints + engatar a malha |
| `GET /stopAll` | Parada de emergência (destrava o latch) |
| `GET /sensors`, `/status`, `/flightStatus`, `/health` | Telemetria |
| `GET /setMotor?motor&speed` | Aciona um motor (bancada) |
| `GET /findDeadband?motor&from&to&step&dwell` | Varredura de deadband (bancada) |
| `GET /calibration`, `/setCalibration`, `/resetCalibration` | Calibração de motor (persiste em NVS) |
| `GET /setImuOffset?gx&gy&gz&ax&ay&az` | Offsets de calibração da IMU |

---

## Como compilar e gravar (`Junção`)

Requer **arduino-cli** com o core `esp32:esp32` e a lib **ESP32Servo**:

```bash
arduino-cli core install esp32:esp32
arduino-cli lib install "ESP32Servo"

cd Junção/firmware
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload  --fqbn esp32:esp32:esp32 -p <PORTA> .
```

Detalhes em [`Junção/firmware/README.md`](./Junção/firmware/README.md).

---

## Roteiro de segurança (antes de qualquer voo)

Siga **SEM HÉLICES** o roteiro em
[`Junção/specs/001-firmware-juncao-voo/quickstart.md`](./Junção/specs/001-firmware-juncao-voo/quickstart.md):
boot/saúde → IMU nivelada (±2°) → deadband por motor → motor individual (pino e sentido) →
**resposta da malha** (mixagem motor-a-motor) → failsafes → calibração persistida. Só depois de
**tudo aprovado**: hélices, ao ar livre, com extrema cautela.

> **Risco nº 1 a vigiar na bancada:** a orientação da IMU (remap) combinada com a mixagem. Ver
> a análise detalhada em
> [`Junção/docs/ANALISE_TRES_PROJETOS.md`](./Junção/docs/ANALISE_TRES_PROJETOS.md).

---

## Estrutura do repositório

```
mestrado/
├── README.md                       # este arquivo
├── ESP32-Flight-controller--main/  # base Arduino (o que voa) — referência
├── Drone-main/                     # hardware/Wi-Fi/failsafes (ESP-IDF) — referência
└── Junção/                         # ★ FIRMWARE OFICIAL
    ├── firmware/                   # código Arduino-ESP32 (.ino + módulos)
    ├── specs/001-firmware-juncao-voo/   # spec, plano, tarefas, contratos, quickstart
    └── docs/                       # análises técnicas (porte + comparação dos 3)
```

## Documentação de referência

- [`Junção/docs/ANALISE_TRES_PROJETOS.md`](./Junção/docs/ANALISE_TRES_PROJETOS.md) — comparação linha a linha dos três projetos + matriz de fidelidade.
- [`Junção/docs/PORTE_PARA_HARDWARE_Drone-main.md`](./Junção/docs) — análise de migração original.
- [`Junção/specs/001-firmware-juncao-voo/`](./Junção/specs/001-firmware-juncao-voo) — especificação completa (spec-kit).

---

## Status

- **Junção**: firmware escrito e revisado (6 bugs corrigidos em revisão independente); **ainda
  não compilado em `arduino-cli` nem validado em hardware**. Os dois portões finais
  (compilação + bancada sem hélices) dependem do equipamento físico.

> Trabalho de mestrado. O firmware **oficial** é o da pasta `Junção`; os outros dois são as
> bases de origem mantidas como referência.
