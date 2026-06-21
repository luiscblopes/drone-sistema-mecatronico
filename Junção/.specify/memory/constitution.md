# Constitution — Junção (Controladora de Voo ESP32)

Princípios inegociáveis do projeto de junção: unir a lógica de voo **comprovada em
voo** do firmware Arduino (`ESP32-Flight-controller`) ao **hardware e à interface
Wi-Fi** do projeto `Drone-main`, gerando um firmware novo nesta pasta.

## Core Principles

### I. Segurança de Voo Acima de Tudo (NÃO-NEGOCIÁVEL)
Os motores e hélices têm energia destrutiva. Toda verificação e bring-up é feita
**SEM HÉLICES**. O caminho de geração de PWM e o laço de controle são tratados
como **contrato congelado**: só mudam com motivo concreto e revalidação em
bancada. Failsafes (perda de comando, IMU inválida/não calibrada, inclinação
excessiva, parada de emergência) são obrigatórios e devem **travar (latch)** o
rearme até comando explícito do operador. Nenhuma funcionalidade nova pode
enfraquecer uma proteção existente.

### II. Código 100% Comentado (NÃO-NEGOCIÁVEL)
Todo o código da junção deve ser **completamente comentado**, no estilo do
`Drone-main` (Doxygen, em **português pt-BR**). Especificamente:
- **Todo arquivo** tem cabeçalho `@file` + `@brief` explicando seu papel no sistema.
- **Toda função** (pública e privada não-trivial) é documentada: propósito,
  parâmetros, retorno e efeitos colaterais.
- **Todo bloco não-óbvio** tem comentário explicando o **porquê** (a intenção/razão
  física ou de segurança), não apenas o "o quê".
- **Toda constante de hardware/controle** (pino, faixa de PWM, ganho, limiar)
  documenta seu significado físico e a **origem do valor** (calibração, ensaio,
  datasheet).
- Não se deixa código morto/comentado sem uma explicação do motivo de estar ali.
Comentário ausente ou que apenas repete o código é considerado defeito e bloqueia
a aceitação. O idioma de comentários e documentação é **pt-BR**.

### III. Base Comprovada em Voo
O firmware Arduino é a **linha de base validada** (angle-mode, filtro complementar,
PID em cascata). Sua lógica de controle, ganhos e filtros são o ponto de partida.
Qualquer desvio dessa base (mudança de ganho, de filtro, de taxa de laço) deve ser
**justificado por escrito** e **revalidado em bancada** antes de ser aceito.

### IV. Contrato de Hardware Explícito e Centralizado
Pinos, faixas de PWM, frequência de ESC, mixagem dos motores, orientação/remap da
IMU e limiares de segurança ficam **centralizados** (estilo `config.h`) e
**documentados individualmente**. A mixagem e a orientação da IMU **não são
herdadas cegamente** de nenhum dos dois projetos: são **re-derivadas e validadas**
para o airframe físico (sinais de roll/pitch/yaw conferidos motor a motor).

### V. Validação em Bancada Antes do Voo
Nenhuma mudança no caminho de controle/PWM é considerada pronta sem passar pelo
roteiro de bancada **sem hélices**: boot, IMU nivelada (remap correto), deadband
por motor, sentido de rotação, resposta da malha no sentido certo e failsafes.
Voo só depois disso.

## Restrições Técnicas

- **Plataforma:** ESP32, framework **Arduino** (decisão: a base que já voa).
- **Hardware-alvo:** placa do `Drone-main` — IMU MPU-9250 no I2C (SDA 21 / SCL 22),
  motores nos GPIO 25/26/27/14, ESC 1000–2000 µs.
- **Comando:** **Wi-Fi** (sem rádio RC, que o hardware não possui), reaproveitando
  o console web (`index.html`) e as rotas HTTP do `Drone-main`.
- **Documentação de referência:** `docs/PORTE_PARA_HARDWARE_Drone-main.md` é a
  análise técnica que alimenta o plano de implementação.
- **Compatibilidade IMU:** o MPU-9250 é register-compatible com o MPU-6050 no
  núcleo accel/gyro; a orientação física exige remap (ver análise).

## Fluxo de Desenvolvimento

O projeto segue o fluxo do **spec-kit**: `/speckit-constitution` →
`/speckit-specify` → `/speckit-clarify` → `/speckit-plan` → `/speckit-tasks` →
`/speckit-implement`. Por ser trabalho de mestrado, após `plan` a especificação é
verificada quanto à aderência às **normas ISO/IEC** de engenharia de software
(skill `verificar-normas`). Cada artefato (spec, plan, tasks) deve ser rastreável
aos princípios desta constitution.

## Governance

Esta constitution prevalece sobre outras práticas do projeto. Os princípios
marcados **NÃO-NEGOCIÁVEL** não podem ser flexibilizados sem emenda registrada
aqui. Toda revisão de código verifica: (1) segurança/contrato congelado,
(2) cobertura de comentários conforme o Princípio II, (3) rastreabilidade à spec.
Complexidade adicional deve ser justificada.

**Version**: 1.0.0 | **Ratified**: 2026-06-21 | **Last Amended**: 2026-06-21
