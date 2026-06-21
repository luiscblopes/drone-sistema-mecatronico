# Junção — Controladora de Voo ESP32 (Angle-Mode por Wi-Fi)

Firmware que **une** a lógica de voo **comprovada em voo** do `ESP32-Flight-controller`
(Arduino: angle-mode, filtro complementar, PID em cascata a **250 Hz**) ao **hardware e à
interface Wi-Fi** do `Drone-main` (placa ESP32 com MPU-9250, console web e failsafes com
latch). Resultado: um quadricóptero que estabiliza atitude em angle-mode, comandado
**exclusivamente por Wi-Fi** (sem rádio RC).

> ⚠️ **SEGURANÇA (NÃO-NEGOCIÁVEL):** toda verificação é feita **SEM HÉLICES**. Nenhum voo
> antes de passar no roteiro de bancada (`specs/001-firmware-juncao-voo/quickstart.md`).

## Estrutura do repositório

| Caminho | Conteúdo |
|---------|----------|
| `firmware/` | Código-fonte do firmware (Arduino-ESP32) — ver `firmware/README.md` para compilar/gravar |
| `specs/001-firmware-juncao-voo/` | Especificação e artefatos de planejamento (spec-kit) |
| `docs/` | `PORTE_PARA_HARDWARE_Drone-main.md` (análise técnica de migração) |
| `.specify/memory/constitution.md` | Princípios inegociáveis do projeto |

## Artefatos da feature

- **Spec**: `specs/001-firmware-juncao-voo/spec.md`
- **Plano**: `specs/001-firmware-juncao-voo/plan.md`
- **Decisões**: `specs/001-firmware-juncao-voo/research.md`
- **Modelo de dados**: `specs/001-firmware-juncao-voo/data-model.md`
- **Contratos**: `specs/001-firmware-juncao-voo/contracts/` (HTTP + hardware)
- **Tarefas**: `specs/001-firmware-juncao-voo/tasks.md`
- **Roteiro de bancada (portão de aceite)**: `specs/001-firmware-juncao-voo/quickstart.md`

## Hardware-alvo

- ESP32 (placa do `Drone-main`); IMU **MPU-9250** no I2C **SDA 21 / SCL 22** (remap +90°Y).
- Motores nos GPIO **25 / 26 / 27 / 14**; ESC **1000–2000 µs @ 250 Hz**.
- Comando por **Wi-Fi** (AP `EQUIPE4-AP`, console em `http://192.168.4.1`).

## Como começar

1. Compilar e gravar — ver **`firmware/README.md`** (arduino-cli, lib `ESP32Servo`).
2. Validar na bancada **SEM HÉLICES** — seguir **`specs/001-firmware-juncao-voo/quickstart.md`**.
3. Só após o roteiro aprovado: voo, ao ar livre, com extrema cautela.

## Princípios (constitution)

Segurança de voo acima de tudo · código 100% comentado em **pt-BR** (Doxygen) · base
comprovada em voo preservada · contrato de hardware centralizado (`firmware/config.h`) ·
validação em bancada antes do voo. Ver `.specify/memory/constitution.md`.
