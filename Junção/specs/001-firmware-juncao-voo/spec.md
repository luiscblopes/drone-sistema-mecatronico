# Feature Specification: Firmware de Junção — Voo em Angle-Mode por Wi-Fi

**Feature Branch**: `001-firmware-juncao-voo`

**Created**: 2026-06-21

**Status**: Draft

**Input**: User description: "Unir a lógica de voo comprovada em voo do firmware Arduino (ESP32-Flight-controller: angle-mode, filtro complementar, PID em cascata a 250 Hz) ao hardware e à interface Wi-Fi do projeto Drone-main, gerando um firmware novo na pasta Junção. Base = Arduino (porque já voa); o Drone-main ainda não voa. O resultado deve: estabilizar o quadricóptero em angle-mode na placa do Drone-main (ESP32, IMU MPU-9250 no I2C 21/22, motores nos GPIO 25/26/27/14, ESC 1000–2000 µs); ser comandado por Wi-Fi (sem rádio RC), reaproveitando o console web (index.html) e as rotas HTTP do Drone-main para setpoints, telemetria e calibração; e incorporar os failsafes do Drone-main (perda de comando, IMU inválida, inclinação excessiva, parada de emergência com latch). Critério de sucesso: passar no roteiro de bancada sem hélices (orientação da IMU correta, mixagem correta motor a motor, resposta da malha no sentido certo) antes de qualquer voo."

## Visão Geral

O firmware da pasta `Junção` une a **lógica de voo comprovada** do `ESP32-Flight-controller`
(angle-mode, filtro complementar, PID em cascata) ao **hardware e à interface Wi-Fi** do
`Drone-main` (placa ESP32 com MPU-9250, comando por console web e failsafes com latch). O
objetivo é um quadricóptero que **estabiliza atitude em angle-mode** na placa do `Drone-main`,
comandado **somente por Wi-Fi**, e que só voa **depois** de passar por um roteiro de validação
de bancada **sem hélices**.

Esta especificação descreve **o que** o sistema deve fazer e **como verificar** que está
correto, em linguagem independente de implementação. As decisões de **como** (estrutura de
código, organização de tarefas) pertencem ao `/speckit-plan`.

## Clarifications

### Session 2026-06-21

- Q: Tensão de framework — a constitution fixa "framework Arduino", mas toda a
  infraestrutura do Drone-main a reaproveitar (Wi-Fi/web/failsafes/NVS) é ESP-IDF em C.
  Qual estratégia adotar? → A: **Arduino (Estratégia A)** — manter o código de voo
  comprovado do `ESP32-Flight-controller` como base e **adaptá-lo ao hardware do
  Drone-main**. A camada de Wi-Fi, comando, failsafes e persistência é **portada/
  reimplementada no firmware Arduino**, reaproveitando o `index.html` como console e
  **replicando o contrato** das rotas HTTP do Drone-main (não se reusa o código C do
  ESP-IDF). A constitution permanece válida sem emenda (framework Arduino).
- Q: Decorrência da escolha acima sobre a taxa do laço de controle. → A: **250 Hz fixos**
  — mantém-se a taxa nativa e os ganhos de PID comprovados em voo do Arduino; **sem
  re-sintonia** (a alternativa de 50 Hz fica descartada).
- Q: Tempo limite do failsafe de perda de comando Wi-Fi antes de cortar e travar? → A:
  **500 ms** (equilíbrio entre reação rápida à perda de Wi-Fi e tolerância a jitter de rede).
- Q: Ângulo de tilt do failsafe de inclinação excessiva? → A: **65°** (limiar já validado na
  placa do Drone-main; margem confortável antes do tombamento).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Validação de bancada sem hélices (Priority: P1)

O operador, com o quadricóptero **sem hélices** preso à bancada, energiza a placa e usa o
console web para confirmar que cada elo da malha de controle está fisicamente correto antes de
qualquer voo: a IMU lê atitude no sentido certo, cada motor responde no GPIO certo, a mixagem
acelera os motores certos para corrigir uma inclinação imposta à mão, e os failsafes cortam o
acionamento quando devem. Este é o **portão de segurança não-negociável**: nenhum voo acontece
sem este roteiro aprovado.

**Why this priority**: É o critério de sucesso explícito do projeto e o princípio
não-negociável da constitution (Segurança de Voo Acima de Tudo; Validação em Bancada Antes do
Voo). Um eixo invertido na mixagem ou um remap de IMU errado faz o drone capotar na decolagem;
a única forma segura de descobrir isso é em bancada, sem hélices. Sem esta história aprovada,
nenhuma outra entrega tem valor utilizável.

**Independent Test**: Pode ser totalmente testada energizando a placa na bancada (sem hélices),
percorrendo o roteiro pelo console web/HTTP e confirmando cada observação esperada (IMU
nivelada, deadband por motor, sentido de rotação, resposta da malha no sentido certo, atuação
dos failsafes). Entrega valor independente: um veredito objetivo de "seguro para voar" ou "não".

**Acceptance Scenarios**:

1. **Given** a placa nivelada e parada na bancada, **When** o operador lê a telemetria de
   atitude, **Then** roll ≈ 0° e pitch ≈ 0° (dentro de uma tolerância de bancada), confirmando
   que o remap de orientação da IMU está correto.
2. **Given** a placa parada, **When** o operador inclina a placa para a frente e para a direita,
   **Then** o pitch e o roll reportados variam no **sinal esperado** (frente → pitch no sentido
   convencionado; direita → roll no sentido convencionado).
3. **Given** o sistema desarmado, **When** o operador aciona individualmente cada motor por um
   comando de percentual/pulso, **Then** **apenas** o motor físico correspondente ao GPIO
   esperado (M1→25, M2→26, M3→27, M4→14) gira, e o pulso onde ele começa a girar (deadband) é
   registrado.
4. **Given** o sistema armado e estabilização ligada com setpoints zero, **When** o operador
   inclina a placa na mão, **Then** os **motores corretos** aceleram para corrigir a inclinação
   (validando mixagem e sinais da malha no sentido certo).
5. **Given** o sistema acionando motores, **When** uma condição de failsafe ocorre (timeout de
   comando, IMU inválida/não calibrada, inclinação excessiva, ou parada de emergência),
   **Then** o acionamento é cortado para o estado seguro e **trava (latch)**, exigindo comando
   explícito do operador para rearmar.

---

### User Story 2 - Estabilização em angle-mode comandada por Wi-Fi (Priority: P2)

Com a bancada aprovada, o operador comanda o quadricóptero **exclusivamente por Wi-Fi**: conecta
ao ponto de acesso da placa, abre o console web e envia setpoints de atitude (roll, pitch, yaw)
e throttle. A malha de controle em angle-mode mantém o quadricóptero na atitude comandada,
usando a lógica de voo comprovada (filtro complementar + PID em cascata ângulo→taxa). Não há
rádio RC — todo o comando trafega pelas rotas HTTP.

**Why this priority**: É a função-fim do produto (voar estável sob comando), mas só tem valor —
e só pode ser exercitada com hélices — **após** a User Story 1. Por isso é P2: depende do portão
de segurança.

**Independent Test**: Pode ser testada na bancada sem hélices observando a **resposta** da malha
(direção e proporcionalidade da correção de motores a setpoints e a perturbações impostas à mão)
e, só após aprovação total, em voo controlado. Entrega valor: o operador consegue manter e
alterar a atitude do quadricóptero pela rede.

**Acceptance Scenarios**:

1. **Given** o operador conectado ao ponto de acesso Wi-Fi da placa, **When** abre o endereço do
   console no navegador, **Then** o console web carrega e exibe telemetria ao vivo (atitude,
   estado de armar/failsafe, motores).
2. **Given** estabilização ligada, **When** o operador envia um setpoint de atitude (ex.: roll =
   +10°) via rota HTTP, **Then** a malha conduz a atitude estimada em direção ao setpoint e a
   mantém enquanto o comando for renovado.
3. **Given** estabilização ligada e setpoints zero, **When** uma perturbação inclina o
   quadricóptero, **Then** a malha gera correções de motor que reduzem o erro de atitude
   (comportamento estável, sem divergir/oscilar de forma crescente).
4. **Given** comando ativo por Wi-Fi, **When** o operador para de enviar comandos por mais que o
   tempo limite, **Then** o failsafe de perda de comando atua (ver User Story 1, cenário 5).

---

### User Story 3 - Calibração e telemetria pelo console (Priority: P3)

O operador usa o console web para **calibrar** (faixas/deadband por motor, calibração da IMU) e
para **observar telemetria** (atitude, sensores, estado da malha e dos failsafes). Os parâmetros
de calibração persistem entre reinicializações, de modo que a placa volta a operar com os
valores aferidos sem recalibração manual a cada boot.

**Why this priority**: Aumenta a usabilidade e a repetibilidade dos ensaios, e é insumo da
própria User Story 1 (deadband por motor, calibração da IMU). É P3 porque o núcleo de segurança
e de voo (US1/US2) pode ser exercitado com calibração mínima; a persistência e a ergonomia
agregam valor incremental.

**Independent Test**: Pode ser testada gravando valores de calibração pelo console, reiniciando
a placa e confirmando que os valores persistiram; e lendo a telemetria pelo console enquanto se
manipula a placa. Entrega valor: ensaios reproduzíveis e visibilidade do estado do sistema.

**Acceptance Scenarios**:

1. **Given** o operador no console web, **When** grava uma calibração de motor (faixa/início) ou
   de IMU, **Then** o valor é aceito, aplicado e **persiste após reinicialização**.
2. **Given** uma calibração persistida, **When** a placa reinicia, **Then** ela opera com os
   valores persistidos sem exigir nova calibração manual.
3. **Given** o operador no console, **When** observa a telemetria, **Then** vê atitude estimada,
   leituras de sensores, estado de armar/failsafe e saídas de motor atualizando ao vivo.

---

### Edge Cases

- **IMU ausente/inválida no boot**: se a IMU não é detectada no I2C, ou entrega dados inválidos,
  o sistema deve recusar armar e sinalizar a condição (não há voo sem atitude confiável).
- **IMU não calibrada**: o sistema deve tratar "não calibrada" como impeditivo de armar/voar,
  coerente com o failsafe de IMU inválida.
- **Inclinação excessiva durante o voo**: se a atitude ultrapassa o limiar de segurança
  (tombamento iminente), o failsafe corta o acionamento e trava.
- **Perda de comando Wi-Fi** (cliente sai do alcance, desconecta, ou para de comandar): o
  failsafe de timeout corta o acionamento dentro do tempo limite.
- **Parada de emergência durante acionamento**: o comando de e-stop corta imediatamente e
  **trava o rearme** até ação explícita do operador.
- **Tentativa de rearmar com failsafe latcheado**: o sistema deve recusar rearmar enquanto a
  causa do latch não for reconhecida/limpa pelo operador.
- **Setpoints fora de faixa** (ângulo/throttle além dos limites): devem ser saturados aos
  limites de segurança, nunca aplicados crus.
- **Comando recebido com o sistema desarmado**: setpoints não devem acionar motores enquanto o
  sistema não estiver explicitamente armado.
- **Orientação da IMU não-trivial**: a placa monta a IMU girada; o sistema deve aplicar o remap
  correto para que roll/pitch saiam no sentido físico certo (risco conhecido de eixos
  trocados/invertidos).
- **Mixagem herdada cegamente**: usar a mixagem do projeto de origem inverteria pitch e
  atribuiria o yaw à diagonal errada; a mixagem deve refletir as convenções físicas validadas
  desta placa.

## Requirements *(mandatory)*

### Functional Requirements

#### Estabilização e malha de controle

- **FR-001**: O sistema MUST estimar a atitude (roll, pitch, yaw) do quadricóptero a partir da
  IMU, usando a lógica de fusão comprovada da base (filtro complementar).
- **FR-002**: O sistema MUST estabilizar o quadricóptero em **angle-mode** (o operador comanda
  ângulos de roll/pitch e taxa de yaw; a malha mantém a atitude comandada) por meio de um
  controle em **cascata** (ângulo → taxa).
- **FR-003**: O sistema MUST executar a malha de controle a **250 Hz fixos** (taxa nativa da base
  comprovada), preservando os ganhos de PID validados em voo sem re-sintonia.
- **FR-004**: O sistema MUST saturar setpoints e termos internos do controlador aos limites de
  segurança (ex.: limites de ângulo comandado e anti-windup do termo integral) antes de
  aplicá-los.
- **FR-005**: O sistema MUST converter as saídas da malha em comandos de motor por uma **mixagem
  re-derivada e validada para o airframe físico desta placa** (sinais de roll/pitch/yaw
  conferidos motor a motor), e NÃO MUST herdar cegamente a mixagem de nenhum dos projetos de
  origem.

#### Hardware e atuação

- **FR-006**: O sistema MUST ler a IMU (MPU-9250) pelo barramento I2C nos pinos SDA 21 / SCL 22.
- **FR-007**: O sistema MUST aplicar o **remap de orientação** da IMU correto para esta placa, de
  modo que, em repouso e nivelada, roll ≈ 0° e pitch ≈ 0°, e que inclinações produzam variação
  no sinal físico esperado.
- **FR-008**: O sistema MUST acionar os quatro motores nos GPIO **25 (M1), 26 (M2), 27 (M3) e 14
  (M4)**, gerando PWM de ESC na faixa **1000–2000 µs** na frequência validada para os ESCs desta
  placa.
- **FR-009**: O sistema MUST armar os ESCs em estado seguro (saída mínima) no boot e MUST NOT
  acionar motores acima do mínimo antes de o sistema estar explicitamente armado.
- **FR-010**: Pinos, faixas/frequência de PWM, mixagem, remap da IMU e limiares de segurança
  MUST estar **centralizados e documentados individualmente** (significado físico e origem do
  valor), como contrato de hardware explícito.

#### Comando por Wi-Fi (sem rádio RC)

- **FR-011**: O sistema MUST ser comandável **exclusivamente por Wi-Fi**, sem depender de rádio
  RC (a placa não possui receptor RC).
- **FR-012**: O sistema MUST disponibilizar um ponto de acesso Wi-Fi e servir o **console web**
  (reaproveitando o `index.html` do `Drone-main`) para comando, telemetria e calibração.
- **FR-013**: O sistema MUST aceitar, por **rotas HTTP que replicam o contrato** do `Drone-main`,
  os setpoints de voo (roll, pitch, yaw, throttle) e o liga/desliga da estabilização, alimentando
  a malha de controle com esses valores.
- **FR-014**: O sistema MUST expor telemetria por rota(s) HTTP/console: atitude estimada,
  leituras de sensores, estado de armar/failsafe e saídas de motor.
- **FR-015**: O sistema MUST permitir o acionamento de motores individuais e a varredura de
  deadband por motor via rotas HTTP, para os ensaios de bancada.

#### Failsafes (com latch)

- **FR-016**: O sistema MUST implementar failsafe de **perda de comando**: se nenhum comando
  válido é recebido dentro de **500 ms**, o acionamento vai ao estado seguro.
- **FR-017**: O sistema MUST implementar failsafe de **IMU inválida/não calibrada**: sem atitude
  confiável, o sistema recusa armar e/ou corta o acionamento.
- **FR-018**: O sistema MUST implementar failsafe de **inclinação excessiva**: ao ultrapassar
  **65°** de tilt, corta o acionamento.
- **FR-019**: O sistema MUST implementar **parada de emergência** comandada pelo operador que
  corta o acionamento imediatamente.
- **FR-020**: Todo failsafe MUST **travar (latch)** o estado seguro: o rearme só ocorre por
  **comando explícito do operador**, após a condição ter sido reconhecida/limpa. Nenhuma
  funcionalidade nova MUST NOT enfraquecer uma proteção existente.

#### Calibração e persistência

- **FR-021**: O sistema MUST permitir calibrar, pelo console/HTTP, a faixa/início (deadband) por
  motor e a calibração da IMU.
- **FR-022**: O sistema MUST **persistir** os parâmetros de calibração entre reinicializações,
  de modo que a placa reinicie já operando com os valores aferidos.

#### Documentação e segurança de processo (derivados da constitution)

- **FR-023**: Todo o código e documentação do firmware MUST ser **comentado em pt-BR** no estilo
  Doxygen exigido pela constitution (cabeçalho por arquivo, doc por função, porquê dos blocos
  não-óbvios, origem física de cada constante de hardware/controle).
- **FR-024**: O caminho de geração de PWM e o laço de controle MUST ser tratados como **contrato
  congelado**: qualquer alteração exige justificativa escrita e **revalidação em bancada**.
- **FR-025**: Nenhuma mudança no caminho de controle/PWM MUST ser considerada pronta sem passar
  pelo **roteiro de bancada sem hélices** (User Story 1).

### Key Entities

- **Estado de Atitude**: roll, pitch e yaw estimados (e suas taxas), derivados da IMU pela fusão
  complementar; insumo da malha de controle e da telemetria.
- **Setpoint de Voo**: ângulos comandados de roll/pitch, taxa de yaw e throttle, recebidos por
  Wi-Fi; entrada da malha em angle-mode.
- **Comando de Motor**: pulso/percentual destinado a cada um dos quatro motores (M1–M4),
  resultado da mixagem; respeita deadband e limites.
- **Estado de Armar/Failsafe**: condição de armado/desarmado e o conjunto de failsafes ativos
  com seu **latch**; governa se o acionamento é permitido.
- **Calibração**: faixas/início por motor e calibração da IMU; persistida entre reinicializações.
- **Contrato de Hardware**: pinos, faixa/frequência de PWM, mixagem, remap da IMU e limiares de
  segurança, centralizados e documentados.
- **Telemetria**: visão ao vivo de atitude, sensores, motores e estado de armar/failsafe exposta
  ao operador.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Com a placa nivelada e parada na bancada, a atitude reportada fica em roll ≈ 0° e
  pitch ≈ 0° dentro de ±2° (confirma o remap de orientação da IMU).
- **SC-002**: Ao inclinar a placa para a frente e para a direita, 100% das vezes o pitch e o roll
  variam no **sinal esperado** (nenhum eixo trocado ou invertido).
- **SC-003**: No teste de motores individuais, 100% dos acionamentos giram **apenas** o motor
  físico correto (M1→GPIO 25, M2→26, M3→27, M4→14); nenhum motor cruzado.
- **SC-004**: No teste de resposta da malha (placa inclinada à mão, sem hélices), 100% das
  perturbações fazem **acelerar os motores corretos** no sentido que reduz o erro de atitude
  (mixagem e sinais validados).
- **SC-005**: Cada um dos quatro failsafes (perda de comando em 500 ms, IMU inválida/não
  calibrada, inclinação excessiva acima de 65°, parada de emergência) é disparado de forma
  reproduzível na bancada e **trava o rearme** até comando explícito do operador, em 100% das
  tentativas.
- **SC-006**: O operador comanda o quadricóptero **somente por Wi-Fi** (console web + rotas HTTP)
  para setpoints, telemetria e calibração, sem nenhum uso de rádio RC.
- **SC-007**: O failsafe de perda de comando atua dentro de **500 ms** quando o comando Wi-Fi
  cessa (verificável em bancada cronometrando o corte).
- **SC-008**: Parâmetros de calibração gravados pelo console persistem após reinicialização em
  100% dos casos (lidos de volta iguais aos gravados).
- **SC-009**: O **roteiro de bancada sem hélices** é concluído e aprovado em todos os seus passos
  **antes** de qualquer voo com hélices (portão de liberação obrigatório).
- **SC-010**: Em estabilização com setpoint constante e perturbações de bancada, a malha
  converge para o setpoint sem oscilação crescente nem divergência (comportamento estável
  observável).

## Assumptions

- **Estratégia de junção (decidida — ver Clarifications)**: **Estratégia A** — o **código Arduino
  comprovado** (`ESP32-Flight-controller`: filtro complementar, PID em cascata, clamps e limites
  ±20°/±400 do termo I) é a base, **adaptado ao hardware do Drone-main**. A camada de Wi-Fi,
  console web, contrato das rotas HTTP, failsafes com latch e persistência de calibração é
  **portada/reimplementada no firmware Arduino**, reaproveitando o `index.html` como console e
  **replicando o contrato** das rotas do Drone-main (não se reusa o código-fonte ESP-IDF/C). A
  mixagem e a orientação da IMU são **re-derivadas e validadas** para esta placa (não herdadas
  cegamente).
- **Taxa do laço de controle (decidida — ver Clarifications)**: **250 Hz fixos**, a taxa nativa da
  base comprovada, para preservar a sintonia de PID validada em voo **sem re-sintonia**. A
  alternativa de 50 Hz fica descartada.
- **Frequência de ESC**: assume-se a frequência validada para os ESCs desta placa (250 Hz),
  conforme a análise técnica de porte; a faixa de pulso 1000–2000 µs é comum aos dois projetos.
- **Canal de comando**: assume-se **Wi-Fi (ponto de acesso + HTTP)** como único canal, conforme a
  recomendação da análise de porte; Bluetooth/BLE estão fora de escopo.
- **Escopo de controle**: o alvo é **angle-mode** (estabilização de atitude). Controle de
  altitude/velocidade vertical (barômetro), navegação por GPS e qualquer modo autônomo estão
  **fora de escopo** desta feature.
- **Magnetômetro**: o magnetômetro do MPU-9250 não é necessário para angle-mode e fica sem uso.
- **Ambiente de operação**: bring-up e toda verificação ocorrem **sem hélices**, em bancada;
  qualquer voo é posterior à aprovação total do roteiro.
- **Reaproveitamento de interface**: o `index.html` e as rotas HTTP do `Drone-main` (incluindo
  `/setFlight`, `/setMotor`, `/stopAll`, `/flightStatus`, `/calibration`, `/setCalibration`,
  `/resetCalibration`, `/findDeadband`, `/sensors`) são reutilizados como contrato de interface
  do operador.

## Dependencies

- **Análise técnica de porte**: `docs/PORTE_PARA_HARDWARE_Drone-main.md` (matriz de
  compatibilidade, remap da IMU, mixagem física, conflitos de pino) alimenta o plano.
- **Projeto-base de voo**: `ESP32-Flight-controller` — lógica de voo comprovada (ganhos, filtro,
  cascata) usada como linha de base.
- **Projeto de hardware/interface**: `Drone-main` — placa-alvo, Wi-Fi, console web (`index.html`),
  rotas HTTP, failsafes com latch e persistência de calibração.
- **Roteiro de bancada de referência**: `TESTE_BANCADA.md` do `Drone-main` (deadband, motores
  individuais, calibração, failsafes).
