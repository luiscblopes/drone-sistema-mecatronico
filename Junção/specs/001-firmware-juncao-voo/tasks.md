---
description: "Task list — Firmware de Junção (Voo em Angle-Mode por Wi-Fi)"
---

# Tasks: Firmware de Junção — Voo em Angle-Mode por Wi-Fi

**Input**: Design documents from `specs/001-firmware-juncao-voo/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Não há tarefas de teste unitário. A verificação desta feature é **em hardware,
na bancada, SEM HÉLICES**, dirigida por `quickstart.md` (cada passo mapeia SC-xxx/FR-xxx).
Esse roteiro é o **portão de aceite** (Princípio V).

**Organization**: Tarefas agrupadas por user story (P1→P3) para entrega incremental e
testável de forma independente na bancada.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: pode rodar em paralelo (arquivos diferentes, sem dependência pendente)
- **[Story]**: US1/US2/US3 (rastreabilidade à spec.md)
- Caminhos relativos à raiz do repo (`Junção/`)

## Path Conventions

Firmware embarcado (single project) em `firmware/` (sketch Arduino + módulos `.h/.cpp`),
conforme `plan.md > Project Structure`.

⚠️ **Princípio I (Caminho Congelado)**: o laço de controle e a geração de PWM
(`flight_task`, `control_cascade`, `motor_mix`, `esc_out`) são contrato congelado — mudanças
exigem justificativa escrita + revalidação em bancada.
⚠️ **Princípio II (pt-BR/Doxygen)**: todo arquivo/função/constante deve ser comentado em
pt-BR ao criar; comentário ausente bloqueia o aceite (FR-023). Não é uma tarefa só de polish.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Esqueleto do projeto e contrato de hardware.

- [X] T001 Criar a estrutura `firmware/` (sketch + pastas de módulos) e o `firmware/firmware.ino` vazio com cabeçalho `@file`/`@brief`, conforme `plan.md > Project Structure`
- [X] T002 [P] Copiar e adaptar o console `firmware/index.html` a partir de `Drone-main/main/index.html` (remover qualquer referência a RC; manter `fetch` das rotas do contrato)
- [X] T003 [P] Criar `firmware/config.h` com TODO o contrato de hardware/controle (pinos 25/26/27/14, I2C 21/22, ESC 1000–2000µs@250Hz, remap +90°Y, mixagem, ganhos PID, dt=0.004, timeout 500ms, tilt 65°, AP SSID/senha) — cada constante documentada (significado físico + origem) conforme `contracts/hardware-config.md`
- [X] T004 [P] Documentar o build em `firmware/README.md`: FQBN `esp32:esp32:esp32`, lib `ESP32Servo`, comandos `arduino-cli compile/upload` (espelhando `quickstart.md`)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Backbone que TODAS as stories usam — comunicação, ESC, IMU, estado compartilhado
e o esqueleto da tarefa de controle. Sem isto nada é observável/testável.

**⚠️ CRITICAL**: Nenhuma user story começa antes desta fase terminar.

- [X] T005 Implementar `firmware/esc_out.h/.cpp` (ESP32Servo): `attach` nos 4 GPIO, `setPeriodHertz(250)`, **arming** (escreve mínimo e segura `ESC_ARM_HOLD_MS`), `set/get` por motor, `stop_all` no mínimo, saturação 1000–2000µs
- [X] T006 [P] Implementar `firmware/imu_mpu9250.h/.cpp`: init I2C 0x68 (PWR_MGMT, DLPF 0x1A, gyro ±500°/s, accel ±8g), leitura bruta, escalas `/65.5` e `/4096`, e **REMAP +90°Y** (`x'=z, y'=y, z'=−x`) a accel e gyro; expõe idade da amostra/validade
- [X] T007 [P] Implementar `firmware/command_state.h/.cpp`: estado compartilhado protegido por `portMUX` (setpoints, ganhos, status, `failsafe_reason`, `latched`) + timestamp do **watchdog de comando** (`mark_command`/`command_age_ms`)
- [X] T008 [P] Implementar `firmware/wifi_ap.h/.cpp`: SoftAP (`WiFi.softAP`) com SSID `EQUIPE4-AP`/senha do `config.h`; host `192.168.4.1`
- [X] T009 Implementar `firmware/web_routes.h/.cpp` (esqueleto `WebServer` em **tarefa core 0**): servir `GET /` (index.html, no-cache), `GET /health`, `GET /status` (plumbing de telemetria JSON) conforme `contracts/http-api.md`
- [X] T010 Implementar `firmware/flight_task.h/.cpp`: **tarefa FreeRTOS no core 1**, período fixo **4 ms (250 Hz)**, ciclo = ler IMU → (corpo da malha a preencher em US1) → escrever ESC; integra o arming
- [X] T011 Montar `firmware/firmware.ino` (`setup()`/`loop()`): ordem de boot (I2C → ESC arm → IMU → SoftAP → web task → flight task); `loop()` mínimo (FreeRTOS faz o trabalho)

**Checkpoint**: Sobe, IMU detectada em 0x68, motores armam no mínimo, web responde → **quickstart Passo 1** (boot/health) passa.

---

## Phase 3: User Story 1 - Validação de bancada sem hélices (Priority: P1) 🎯 MVP

**Goal**: Caminho de controle completo + failsafes + rotas de bancada, de modo que o roteiro
SEM HÉLICES produza o veredito "seguro para voar" (orientação da IMU, mixagem motor-a-motor,
resposta da malha no sentido certo, failsafes com latch).

**Independent Test**: Executar `quickstart.md` Passos 2–6 e confirmar SC-001..SC-005, SC-007,
SC-010 (IMU nivelada ±2°, sinais corretos, motores certos, resposta da malha correta,
failsafes travam).

### Implementation for User Story 1

- [X] T012 [P] [US1] Implementar `firmware/attitude_filter.h/.cpp`: filtro complementar `0.991/0.009` sobre dados **já remapeados**, eqs. de ângulo do sketch, clamp **±20°** (roll/pitch)
- [X] T013 [P] [US1] Implementar `firmware/control_cascade.h/.cpp`: cascata ângulo→taxa com ganhos do `config.h` (`dt=0.004`), clamps **±400** (termo I e saída), reset dos termos no cutoff de throttle
- [X] T014 [P] [US1] Implementar `firmware/motor_mix.h/.cpp`: mixagem física do Drone-main (M1=T+P+R−Y, M2=T+P−R+Y, M3=T−P−R−Y, M4=T−P+R+Y) + saturação por motor (teto 1999, piso idle, cutoff 1000)
- [X] T015 [US1] Implementar `firmware/failsafe.h/.cpp`: máquina de estados com **latch** — `COMMAND_TIMEOUT` (>500ms), `IMU_INVALID` (idade > `MPU_MAX_AGE_MS`/ausência/não calibrada), `EXCESSIVE_TILT` (|roll|/|pitch|>65°), `EMERGENCY_STOP`, `MANUAL_OVERRIDE`; latch só limpo por `clear_failsafe_latch` (data-model §4)
- [X] T016 [US1] Preencher o corpo da malha em `firmware/flight_task.cpp`: IMU→`attitude_filter`→`control_cascade`→`motor_mix`→`esc_out`, com o **failsafe avaliado e aplicado a cada ciclo** (corta+latcha antes de escrever ESC) — depende de T012–T015
- [X] T017 [US1] Implementar as rotas de bancada em `firmware/web_routes.cpp`: `GET /setMotor` (override manual), `GET /findDeadband` (varredura), `GET /stopAll` (e-stop + **limpa latch**), `GET /sensors` (atitude/validade), `GET /flightStatus` (status completo) conforme `contracts/http-api.md`
- [X] T018 [US1] Implementar o caminho estabilizado de `GET /setFlight` em `firmware/web_routes.cpp`: engatar malha, `mark_command`, **409** se `latched`, **503** se a tarefa não roda
- [X] T019 [US1] Aplicar trava de arming (**423**) às rotas de motor/calibração e expor texto de `failsafe` em `GET /status` em `firmware/web_routes.cpp`

**Checkpoint**: `quickstart.md` Passos 2–6 passam → MVP entregue (veredito de bancada). US2/US3 podem começar.

---

## Phase 4: User Story 2 - Estabilização angle-mode comandada por Wi-Fi (Priority: P2)

**Goal**: Comandar atitude (roll/pitch/yaw) e throttle exclusivamente por Wi-Fi, mantendo a
atitude comandada; sintonia de ganhos em runtime. Constrói sobre o caminho de US1.

**Independent Test**: `quickstart.md` Passo 5/8 — conectar o celular ao AP, abrir o console,
enviar setpoints e ver a malha conduzir/manter a atitude (SC-006, SC-002 em comando);
failsafe de perda de comando ao cessar o envio (SC-007).

### Implementation for User Story 2

- [X] T020 [US2] Implementar o mapeamento completo de setpoints em `GET /setFlight` (`throttle` 1000–2000, `rollSp`/`pitchSp` saturados **±20°**, `yawSp` como taxa de guinada) alimentando `command_state` em `firmware/web_routes.cpp`
- [X] T021 [P] [US2] Implementar sintonia de ganhos em runtime via `/setFlight` (`rollKp..yawKd`) e `GET /resetPid` em `firmware/web_routes.cpp` + `firmware/control_cascade.cpp`
- [X] T022 [US2] Implementar engate/desengate por comando sustentado e o **handoff para failsafe** quando o comando cessa (override manual quando `stabilize=0`) em `firmware/flight_task.cpp` + `firmware/command_state.cpp`
- [X] T023 [US2] Validar/ajustar o `firmware/index.html` para dirigir setpoints e telemetria contra o contrato (sem RC), e o reenvio periódico (<500ms) do `/setFlight`

**Checkpoint**: Celular comanda atitude por Wi-Fi e a malha mantém; perda de comando dispara failsafe. US1 continua válido.

---

## Phase 5: User Story 3 - Calibração e telemetria persistidas (Priority: P3)

**Goal**: Calibrar motores (start/max/trim) e IMU pelo console, **persistir em NVS** e observar
telemetria ao vivo; valores sobrevivem ao reboot.

**Independent Test**: `quickstart.md` Passo 7 — gravar calibração, reiniciar, ler de volta
idêntico (SC-008); ler telemetria viva (SC — US3 cenário 3).

### Implementation for User Story 3

- [X] T024 [P] [US3] Implementar `firmware/nvs_store.h/.cpp` com `Preferences`: calibração de motor (namespace `motor-cal`, chaves `start%d/max%d/trim%d`) e offsets de IMU (gyro/accel)
- [X] T025 [US3] Carregar a calibração no boot (antes da flight task) e aplicá-la a `esc_out` e aos offsets da IMU em `firmware/firmware.ino` + `firmware/nvs_store.cpp` — depende de T024
- [X] T026 [US3] Implementar as rotas de calibração em `firmware/web_routes.cpp`: `GET /calibration` (lê), `GET /setCalibration` (grava+persiste+reaplica, sanitiza `trim` ±100), `GET /resetCalibration` (defaults) conforme `contracts/http-api.md`
- [X] T027 [US3] Substituir os offsets de IMU **hardcoded** por valores persistidos e adicionar o gancho de rotina de calibração da IMU em `firmware/imu_mpu9250.cpp`
- [X] T028 [P] [US3] Enriquecer a telemetria (`/sensors`, `/flightStatus`) com validade/idade da IMU, taxas e saídas de motor em `firmware/web_routes.cpp`

**Checkpoint**: Calibração persiste após reboot; telemetria completa. As três stories funcionam.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Qualidade transversal e o portão de validação final.

- [X] T029 [P] Auditoria Doxygen pt-BR (Princípio II) em `firmware/*`: `@file`/`@brief` por arquivo, doc por função, porquê dos blocos não-óbvios, origem física das constantes
- [X] T030 [P] Escrever `README.md` na raiz `Junção/` apontando para `specs/001-firmware-juncao-voo/` e o `quickstart.md`
- [ ] T031 Executar o `quickstart.md` completo **SEM HÉLICES** e registrar resultados (portão SC-009 — libera ou bloqueia voo) — **requer hardware físico (pendente)**
- [X] T032 [P] Verificar rastreabilidade à constitution (caminho congelado, cobertura de comentários, spec→tasks) e preparar a verificação de normas (skill `verificar-normas`)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: sem dependências — começa já.
- **Foundational (Phase 2)**: depende do Setup — **BLOQUEIA** todas as stories.
- **US1 (Phase 3)**: depende da Foundational. É o **MVP**.
- **US2 (Phase 4)**: depende da Foundational; **constrói sobre o caminho de controle de US1**
  (estabilização). Testável independentemente pelo fluxo de comando Wi-Fi.
- **US3 (Phase 5)**: depende da Foundational; ortogonal a US1/US2 (persistência/telemetria).
  Pode ser desenvolvida em paralelo a US2.
- **Polish (Phase 6)**: depois das stories desejadas; T031 só após o caminho de US1 existir.

### Within Each User Story

- US1: modelos puros (filtro/cascata/mixagem T012–T014 em paralelo) → failsafe (T015) →
  integração na flight task (T016) → rotas (T017–T019).
- Modules antes das rotas; rotas antes da integração de fluxo.

### Parallel Opportunities

- Setup: T002, T003, T004 em paralelo.
- Foundational: T006, T007, T008 em paralelo (após T005 idealmente; T009/T010/T011 dependem
  dos módulos base).
- US1: T012, T013, T014 em paralelo (arquivos distintos), depois T015→T016.
- US2 e US3 podem correr em paralelo após US1 (devs diferentes); dentro de US3, T024 e T028
  são [P].

---

## Parallel Example: User Story 1

```text
# Lançar os módulos puros do caminho de controle juntos (arquivos distintos):
Task T012: attitude_filter.cpp (filtro complementar + clamp ±20°)
Task T013: control_cascade.cpp (PID cascata, dt=0.004, ±400)
Task T014: motor_mix.cpp (mixagem física Drone-main + saturação)
# Em seguida (sequencial): T015 failsafe → T016 integração na flight_task → T017–T019 rotas
```

---

## Implementation Strategy

### MVP First (User Story 1)

1. Setup (Phase 1) → 2. Foundational (Phase 2) → 3. US1 (Phase 3).
4. **PARAR e VALIDAR**: rodar `quickstart.md` Passos 1–6 SEM HÉLICES.
5. Veredito de bancada (SC-009 parcial) é o primeiro entregável de valor real.

### Incremental Delivery

1. Setup + Foundational → backbone (boot/IMU/ESC/web).
2. + US1 → caminho de controle + failsafes validados na bancada (**MVP**).
3. + US2 → comando angle-mode pleno por Wi-Fi (celular).
4. + US3 → calibração persistida + telemetria rica.
5. Polish → auditoria de comentários + `quickstart` completo + prep de normas.

### Parallel Team Strategy

Após a Foundational: Dev A em US1 (crítico, primeiro); ao concluir US1, Dev B em US2 e Dev C
em US3 simultaneamente. Integração por arquivos distintos minimiza conflito.

---

## Notes

- **Nenhuma tarefa de teste unitário**: a verificação é a bancada (`quickstart.md`), portão
  de aceite (Princípio V). Toda mudança no caminho congelado revalida na bancada.
- `[P]` = arquivos diferentes, sem dependência pendente.
- Comentar em pt-BR ao escrever (Princípio II) — não deixar para o polish.
- Commit após cada tarefa ou grupo lógico; parar nos checkpoints para validar.
- **SEM HÉLICES** em toda verificação até SC-009 aprovado.
