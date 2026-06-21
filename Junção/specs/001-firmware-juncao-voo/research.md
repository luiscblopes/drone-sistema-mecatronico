# Phase 0 — Research & Decisões: Firmware de Junção

Consolidação das decisões técnicas que resolvem os pontos em aberto do `Technical Context`.
Cada decisão segue o formato **Decisão / Justificativa / Alternativas consideradas** e é
rastreável às fontes (`PORTE_PARA_HARDWARE_Drone-main.md`, sketch Arduino, `config.h` do
Drone-main). Valores marcados ⚠️ **só são confirmados em bancada** (quickstart.md).

---

## D1 — Framework e estratégia de junção

- **Decisão**: **Arduino-ESP32 (Estratégia A)**. Manter o código de voo comprovado do
  `ESP32-Flight-controller` e adaptá-lo ao hardware do Drone-main; reimplementar a camada
  Wi-Fi/web/NVS com bibliotecas **padrão do core arduino-esp32** (`WiFi`, `WebServer`,
  `Preferences`), reaproveitando o `index.html` e **replicando o contrato** das rotas.
- **Justificativa**: decisão registrada nas Clarifications da spec; alinhada à constitution
  ("framework Arduino"). As libs do core fornecem equivalentes diretos do que o Drone-main fez
  em ESP-IDF (WebServer ≈ esp_http_server, Preferences ≈ nvs_flash, WiFi SoftAP ≈ wifi_ap),
  evitando recriar infraestrutura do zero.
- **Alternativas**: (B) transplante para ESP-IDF — rejeitada por contrariar a constitution;
  (Híbrido Arduino-no-IDF) — rejeitada por complexidade de build/coexistência.

## D2 — Build e toolchain

- **Decisão**: compilar com **arduino-cli** (e Arduino IDE como alternativa manual), placa
  `esp32:esp32:esp32`, dependências `ESP32Servo`. `WiFi`, `WebServer`, `Wire`, `Preferences`
  são parte do core.
- **Justificativa**: arduino-cli é reprodutível e documentável (comando no quickstart);
  mantém o projeto fiel ao framework Arduino.
- **Alternativas**: PlatformIO — viável, porém adiciona uma camada de configuração não exigida;
  fica como opção futura.

## D3 — Pinos de motor (re-pinagem)

- **Decisão**: `M1=GPIO25, M2=GPIO26, M3=GPIO27, M4=GPIO14` (config em X, ordem do Drone-main).
- **Justificativa**: pinos físicos dos ESCs na placa do Drone-main (`config.h`
  `ESC_GPIO_M1..M4`). O sketch original usa 13/12/14/27 (incompatível: 25/26 lá são canais de
  rádio; 13 não tem ESC nesta placa) — `PORTE` §3.
- **Alternativas**: manter pinos do Arduino — impossível (não há ESC nesses pinos).

## D4 — Frequência e arming dos ESCs

- **Decisão**: **250 Hz**, faixa **1000–2000 µs**; arming escrevendo o mínimo e segurando por
  ~5 s antes de aceitar comando (`ESC_ARM_HOLD_MS`).
- **Justificativa**: 250 Hz é o valor validado nos ESCs desta placa (`PORTE` §4.4; `config.h`
  `ESC_FREQUENCY_HZ=250`). O sketch usava 500 Hz. A faixa de pulso é comum aos dois.
- **Alternativas**: 500 Hz do Arduino — rejeitada (pode fazer o ESC apitar/não armar nesta
  placa).

## D5 — Remap de orientação da IMU

- **Decisão**: aplicar rotação **+90° em torno de Y** a **accel e gyro**, **antes** das
  equações de ângulo e do PID: `novo_x = antigo_z ; novo_y = antigo_y ; novo_z = −antigo_x`.
  Manter o registrador/escala do Arduino (accel `/4096` em ±8g, gyro `/65.5` em ±500°/s,
  DLPF 0x1A, endereço 0x68).
- **Justificativa**: a IMU é montada girada na placa do Drone-main; em repouso a gravidade cai
  em **−X** (`config.h` `MPU_REMAP_ROTATE_Y_90=1`; `PORTE` §4.2). Sem o remap, roll/pitch do
  Arduino (que assume Z para cima) saem trocados/invertidos. O MPU-9250 é register-compatible
  com o MPU-6050 no núcleo accel/gyro (`PORTE` §4.1) → o código de leitura do Arduino funciona
  sem mudar registradores; só o remap é necessário.
- **⚠️ Validação**: confirmar em bancada (quickstart §2) — nivelada ⇒ roll/pitch ≈ 0; inclinar
  frente/direita ⇒ sinais esperados. O eixo de yaw e os sinais de gyro pós-remap precisam ser
  conferidos pela resposta da malha (quickstart §5).
- **Alternativas**: remontar a IMU com Z para cima (`MPU_REMAP=0`) — viável, mas muda o
  hardware; preferimos compensar por software para não alterar a montagem validada do
  Drone-main. Magnetômetro AK8963 fica **sem uso** (não necessário em angle-mode).

## D6 — Mixagem dos motores (re-derivada)

- **Decisão**: usar a **convenção física do Drone-main** (não a do Arduino):
  ```
  M1 (frente-esq) = T + P + R − Y
  M2 (frente-dir) = T + P − R + Y
  M3 (trás-dir)   = T − P − R − Y
  M4 (trás-esq)   = T − P + R + Y
  ```
- **Justificativa**: a mixagem do Arduino (pinos 13/12/14/27) **inverte o pitch** e atribui o
  yaw à **diagonal errada** nesta placa (`PORTE` §4.6: pitch com sinal oposto, par CW em
  diagonal oposta). A mixagem depende da posição física e do sentido de rotação dos motores
  **desta** placa.
- **⚠️ Validação**: confirmar motor a motor em bancada (quickstart §5) — inclinar à mão e ver
  os motores **certos** acelerarem para corrigir. Os sinais acima são **ponto de partida** e
  podem ser ajustados conforme o sentido de rotação real (quickstart §4).
- **Alternativas**: herdar a mixagem do Arduino — rejeitada (capota ao decolar).

## D7 — Taxa do laço e ganhos de PID

- **Decisão**: **250 Hz** (`dt = 0.004 s`) com os **ganhos comprovados do Arduino**, sem
  re-sintonia:
  - Ângulo (externo): `P=2, I=0.5, D=0.007` (roll=pitch).
  - Taxa (interno): roll/pitch `P=0.625, I=2.1, D=0.0088`; yaw `P=4, I=3, D=0`.
  - Clamps: termo I e saída de cada PID em **±400**; filtro complementar **0.991/0.009** com
    saída clampada a **±20°**.
- **Justificativa**: decisão registrada nas Clarifications (250 Hz fixos). Os termos I/D
  dependem de `dt`; manter 250 Hz preserva a validade dos ganhos comprovados em voo (`PORTE`
  §4.5). Setpoint de ângulo é o erro da malha externa; a saída vira setpoint de taxa do interno
  (cascata), exatamente como no sketch.
- **⚠️ Validação**: resposta estável sem oscilação crescente (quickstart §5; SC-010). Ajuste
  fino de ganho, se necessário, exige justificativa escrita (Princípio III).
- **Alternativas**: 50 Hz do Drone-main — rejeitada (exigiria re-sintonia, perde a base
  comprovada).

## D8 — Substituição do comando RC por Wi-Fi

- **Decisão**: **remover** a leitura por interrupção dos canais de rádio (GPIO 34/35/32/33 e o
  conflito em 25/26) e alimentar os setpoints a partir das rotas HTTP. Mapeamento:
  - `throttle` → empuxo base (`InputThrottle`).
  - `rollSp`/`pitchSp` → `DesiredAngleRoll/Pitch` (ângulo comandado).
  - `yawSp` → `DesiredRateYaw` (taxa de guinada comandada — angle-mode mantém yaw em taxa).
- **Justificativa**: a placa não tem receptor RC e os pinos do RC colidem com motores (`PORTE`
  §3, §4.3). Os 4 setpoints substituem `ReceiverValue[0..3]` no sketch (`PORTE` §4.3).
- **Saturação de setpoint**: ângulos de roll/pitch saturados ao **envelope proven de ±20°**
  (coerente com o clamp do filtro complementar do Arduino), e não aos ±35° do `setFlight`
  original — escolha conservadora para a junção; ajustável com justificativa. Yaw em taxa
  conforme escala `0.15` do sketch.
- **Alternativas**: Bluetooth/BLE — fora de escopo (Clarifications/`PORTE` §4.3.1).

## D9 — Concorrência (tarefa de controle × servidor web)

- **Decisão**: **duas tarefas FreeRTOS** (o core arduino-esp32 já provê FreeRTOS):
  - **Tarefa de controle** no **core 1**, prioridade alta, período fixo **4 ms** (250 Hz) —
    contém o caminho congelado (IMU → filtro → cascata → mixagem → ESC).
  - **Servidor web + SoftAP** no **core 0** (`WebServer.handleClient()`), prioridade menor.
  - Estado compartilhado (setpoints, ganhos, status, latch) protegido por **seção crítica /
    `portMUX`** e, para os controladores, mutex — espelhando o Drone-main.
- **Justificativa**: o sketch original roda tudo em um `loop()` único de 250 Hz **sem** servir
  HTTP; introduzir o `WebServer` no mesmo laço quebraria o período de 4 ms. Separar tarefas
  preserva o caminho congelado e o determinismo (Princípio I/III). O Drone-main já usa 2
  tarefas + mutex/portMUX (`PORTE` §4.7).
- **Alternativas**: loop único cooperativo — rejeitada (jitter inaceitável a 250 Hz).

## D10 — Failsafes com latch (portados do Drone-main)

- **Decisão**: máquina de estados com os motivos do Drone-main e **latch** que só é limpo por
  `/stopAll`:
  - **COMMAND_TIMEOUT**: sem comando válido por **> 500 ms** (watchdog `mark_command`).
  - **IMU inválida/não calibrada**: dado de IMU mais velho que `MPU_MAX_AGE_MS` (~150 ms),
    ausência no boot, ou flag "não calibrada" ⇒ recusa armar/corta.
  - **EXCESSIVE_TILT**: `|roll|` ou `|pitch|` > **65°**.
  - **EMERGENCY_STOP / MANUAL_OVERRIDE**: comando do operador (`/stopAll`, `/setMotor`,
    `/setCalibration`).
  - Qualquer motivo perigoso leva motores ao mínimo, desabilita a malha e **trava o rearme**;
    `/setFlight?...&stabilize=1` responde **409** enquanto travado; `/stopAll` limpa o latch.
- **Justificativa**: o Arduino praticamente não tem failsafes (só corta se throttle < 1030);
  os do Drone-main são o diferencial de segurança (`PORTE` §4.7, §9). Valores 500 ms e 65°
  vêm das Clarifications.
- **⚠️ Validação**: disparar cada failsafe reproduzivelmente (quickstart §6; SC-005/SC-007).

## D11 — Persistência de calibração (NVS via Preferences)

- **Decisão**: persistir em NVS com **`Preferences`**: calibração por motor
  (`start/max/trim`, namespace `motor-cal`, chaves `start%d/max%d/trim%d`) e offsets de
  calibração da IMU (gyro/accel). Carregar no boot antes de iniciar o laço; gravar ao alterar
  via `/setCalibration`.
- **Justificativa**: mesma semântica do `calibration_store.c`; substitui os offsets
  **hardcoded** do `setup()` do Arduino por valores persistidos e ajustáveis pela interface
  (`PORTE` §4.7). `Preferences` é o wrapper Arduino sobre a mesma NVS.
- **Alternativas**: manter offsets hardcoded — rejeitada (não atende US3/FR-022).

## D12 — Contrato de rotas HTTP (replicado)

- **Decisão**: replicar o **contrato** das rotas do Drone-main (mesmos caminhos, parâmetros e
  formato JSON), detalhado em `contracts/http-api.md`: `/`, `/status`, `/health`, `/sensors`,
  `/setMotor`, `/stopAll`, `/setFlight`, `/flightStatus`, `/resetPid`, `/calibration`,
  `/setCalibration`, `/resetCalibration`, `/findDeadband`.
- **Justificativa**: reaproveita o `index.html` sem alterá-lo (faz `fetch` desses caminhos) e
  mantém a familiaridade do operador. Rotas de altitude vertical (`/setVerticalHold`,
  `/setVerticalGains`) ficam **fora de escopo** (sem barômetro nesta feature).
- **Alternativas**: API nova — rejeitada (quebraria o reaproveitamento do console).

---

## Itens que permanecem para validação física (não bloqueiam o design)

Todos marcados ⚠️ acima convergem para o **roteiro de bancada** (quickstart.md): orientação da
IMU (D5), mixagem e sinais (D6/D8), estabilidade dos ganhos a 250 Hz (D7) e atuação dos
failsafes (D10). Nenhum `NEEDS CLARIFICATION` permanece no `Technical Context`.
