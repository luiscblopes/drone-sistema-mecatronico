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

## O console web (HTML) — onde está e como funciona

Tudo roda num **único ESP32**. O console web fica na pasta do firmware:

- [`Junção/firmware/index.html`](./Junção/firmware/index.html) — a página de controle (fonte legível).
- [`Junção/firmware/index_html.h`](./Junção/firmware/index_html.h) — **o mesmo HTML embutido em PROGMEM**; é este arquivo que entra no binário e é servido em `GET /`.

> **Importante:** o HTML **não é enviado separado** para o ESP32 — ele é **compilado dentro do
> firmware**. Você grava **um arquivo só** (o firmware) e o console já está dentro. Não precisa de
> SPIFFS/LittleFS nem de segundo upload.
>
> **Se você editar `index.html`, precisa regerar `index_html.h`** (PowerShell, na pasta `firmware`):
> ```powershell
> $h = Get-Content index.html -Raw -Encoding UTF8
> $out = "#ifndef INDEX_HTML_H`n#define INDEX_HTML_H`n#include <pgmspace.h>`n`nconst char index_html[] PROGMEM = R`"JUNCAO_HTML(`n" + $h + "`n)JUNCAO_HTML`";`n`n#endif`n"
> Set-Content index_html.h -Value $out -Encoding UTF8
> ```

---

## Como gravar em um único ESP32 — passo a passo completo

Todo o sistema (controle de voo + Wi-Fi + console web) roda em **um único ESP32**. Não há
"mestre/escravo" nem segundo microcontrolador.

### Passo 0 — Hardware (montagem, SEM HÉLICES)

| Ligação | Pino ESP32 | Para |
|---|---|---|
| Motor M1 | **GPIO 25** | sinal do ESC 1 (frente-esquerda) |
| Motor M2 | **GPIO 26** | sinal do ESC 2 (frente-direita) |
| Motor M3 | **GPIO 27** | sinal do ESC 3 (traseira-direita) |
| Motor M4 | **GPIO 14** | sinal do ESC 4 (traseira-esquerda) |
| IMU SDA | **GPIO 21** | SDA do MPU-9250 |
| IMU SCL | **GPIO 22** | SCL do MPU-9250 |
| IMU VCC/GND | 3V3 / GND | alimentação da IMU |
| GND comum | GND | **GND do ESP32, dos ESCs e da IMU juntos** |

> ⚠️ **Hélices removidas.** Alimente os ESCs pela bateria/fonte de bancada; o ESP32 pela USB.

### Passo 1 — Instalar a ferramenta (uma vez)

Opção A — **arduino-cli** (linha de comando, recomendado):
```bash
# instalar o core ESP32 e a unica biblioteca externa
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "ESP32Servo"
```

Opção B — **Arduino IDE**: em *Preferências* adicione a URL de placas ESP32
`https://espressif.github.io/arduino-esp32/package_esp32_index.json`, instale "esp32" em
*Gerenciador de Placas*, e a lib **ESP32Servo** em *Gerenciador de Bibliotecas*. Abra
`Junção/firmware/firmware.ino` (os demais `.cpp/.h` viram abas automaticamente).

### Passo 2 — Descobrir a porta do ESP32

Conecte o ESP32 por USB e:
```bash
arduino-cli board list
```
Anote a porta (ex.: Windows `COM5`, Linux `/dev/ttyUSB0`, macOS `/dev/cu.SLAB_USBtoUART`).

### Passo 3 — Compilar e gravar (um único comando cada)

```bash
cd Junção/firmware
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload  --fqbn esp32:esp32:esp32 -p <PORTA> .
```
> Se a gravação não iniciar, segure o botão **BOOT** do ESP32 ao começar o upload.

Para ver os logs de boot (opcional):
```bash
arduino-cli monitor -p <PORTA> -c baudrate=115200
```
Deve aparecer: `IMU MPU-9250 detectada (0x68)` e `Conecte ao AP e abra http://192.168.4.1`.

### Passo 4 — Conectar e abrir o console

1. No celular/PC, conecte na rede Wi-Fi **`EQUIPE4-AP`** (senha **`12345678`**).
2. Abra o navegador em **`http://192.168.4.1`**.
3. O console (o `index.html` embutido) carrega — telemetria, setpoints e calibração.

> 📱 No celular, ao conectar numa rede **sem internet**, escolha **"manter conexão"** (e/ou
> desligue os dados móveis), senão o Android/iOS pode não abrir a página.

### Passo 5 — Validar na bancada (OBRIGATÓRIO, SEM HÉLICES)

Antes de qualquer voo, siga **todo** o roteiro de
[`Junção/specs/001-firmware-juncao-voo/quickstart.md`](./Junção/specs/001-firmware-juncao-voo/quickstart.md):
boot/saúde → IMU nivelada (±2°) → deadband por motor → motor individual (pino e sentido) →
resposta da malha (mixagem motor-a-motor) → failsafes → calibração. Só depois disso: hélices,
ao ar livre, com extrema cautela.

### Resolução de problemas

| Sintoma | Provável causa / o que fazer |
|---|---|
| `IMU nao respondeu` no serial | Conferir fiação I2C (21/22), 3V3/GND da IMU, endereço 0x68 |
| ESC apita e não arma | Conferir alimentação dos ESCs; a frequência já é 250 Hz (correta p/ esta placa) |
| Página não abre no celular | "Manter conexão" na rede sem internet; tentar `http://192.168.4.1` (não https) |
| Upload falha | Segurar **BOOT** ao iniciar; conferir a `<PORTA>`; fechar o monitor serial antes |
| Motor errado gira | É esperado descobrir na bancada (passo 4/5 do quickstart) — ajustar mixagem/fiação |

Detalhes adicionais em [`Junção/firmware/README.md`](./Junção/firmware/README.md).

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

- [`Junção/docs/diagrama_interligacao.html`](./Junção/docs/diagrama_interligacao.html) — **diagrama de ligação completa** (ESP32 ↔ IMU, ESCs/motores, energia, Wi-Fi). Abra no navegador.
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
