# Contrato — Rotas HTTP (replicado do Drone-main)

O firmware da Junção **replica o contrato** das rotas do `Drone-main` (mesmos caminhos,
parâmetros e formato de resposta) para reaproveitar o `index.html` **sem alterá-lo** e manter a
familiaridade do operador. Implementado em Arduino-ESP32 com `WebServer` (substitui o
`esp_http_server`). Todas as rotas são **GET** com parâmetros em query string. Base do AP:
SSID `EQUIPE4-AP`, senha `12345678`, host `192.168.4.1`.

> **Escopo**: as rotas de controle vertical/altitude (`/setVerticalHold`, `/setVerticalGains`)
> ficam **fora desta feature** (sem barômetro). O console as ignora quando ausentes.

## Regras transversais

- **Durante o arming** (~5 s no boot): rotas que mexem em motor respondem **423 Locked**.
- **Failsafe travado**: `/setFlight` com `stabilize=1` responde **409 Conflict** ("execute
  PARAR TUDO antes de reabilitar"). `/stopAll` limpa o latch.
- **Saturação**: parâmetros fora de faixa são **saturados**, nunca aplicados crus.
- Respostas de status são **JSON**; ações simples respondem texto curto ("OK").

## Rotas de telemetria / página

| Rota | Resposta | Campos principais |
|------|----------|-------------------|
| `GET /` | HTML | Serve `index.html` (no-cache) |
| `GET /status` | JSON | `speeds[4]`(%), `outputs[4]`(µs), `arming`, `flightControlEnabled`, `failsafe`(texto) |
| `GET /health` | JSON | `ok`, `heap`, `commandTask`, `telemetryTask`, `flightTask`, `motorsAtMinimum` |
| `GET /sensors` | JSON | Atitude estimada (roll/pitch/yaw, taxas), accel/gyro, validade/idade da IMU |
| `GET /flightStatus` | JSON | Setpoint, estado (atitude), taxa, correção, saída de motores, `failsafe`, `latched` |

## Rotas de comando de voo

### `GET /setFlight` — rota central de voo
Parâmetros (todos opcionais; ausente ⇒ mantém valor atual):

| Param | Faixa (saturação) | Mapeia para |
|-------|-------------------|-------------|
| `throttle` | 1000..2000 µs | Empuxo base |
| `rollSp` | **±20°** (envelope proven) | `DesiredAngleRoll` |
| `pitchSp` | **±20°** | `DesiredAnglePitch` |
| `yawSp` | taxa (°/s) | `DesiredRateYaw` |
| `rollKp/rollKi/rollKd` … `yawKp/yawKi/yawKd` | — | Ganhos PID (sintonia em runtime) |
| `apply` | `0|1` | Aplica saída aos motores |
| `stabilize` | `0|1` | `apply=1&stabilize=1` ⇒ engata malha estabilizada |

Respostas: `200 OK`; `423` em arming; `503` se a tarefa de controle não existe; `409` se
failsafe travado. Deve ser **reenviada periodicamente (< 500 ms)** para manter a malha ativa
(watchdog).

> Nota de junção: o contrato do Drone-main satura `rollSp/pitchSp` em ±35°. A Junção adota
> **±20°** para coincidir com o clamp do filtro complementar comprovado do Arduino (research
> D8). O `index.html` continua funcionando (envia o valor; o firmware satura).

### `GET /resetPid`
Zera os controladores (mixagem e cascata). Resposta `200 OK`.

### `GET /stopAll` — parada de emergência / destrava
Desabilita o controle, leva motores ao mínimo e **limpa o latch** de failsafe (permite
rearmar). Resposta `200 OK`. **É a ação de destravar após qualquer failsafe.**

## Rotas de bancada (motor / deadband)

### `GET /setMotor?motor=&speed=`
`motor` ∈ 0..3; `speed` ∈ 0..100 (%). Aciona **um** motor; desabilita a malha (override
manual). `423` em arming. Usado no teste de motor individual (quickstart §5).

### `GET /findDeadband?motor=&from=&to=&step=&dwell=`
Varre o pulso de um motor de `from` a `to` (µs) em passos `step`, com `dwell` ms por passo,
para achar onde o motor começa a girar. Usado na caracterização de deadband (quickstart §3).

## Rotas de calibração (persistidas em NVS)

| Rota | Efeito |
|------|--------|
| `GET /calibration` | Lê calibração atual: JSON `start[4]`, `max[4]`, `trim[4]`, `outputs[4]`, `arming` |
| `GET /setCalibration?start{N}=&max{N}=&trim{N}=` (N=0..3) | Grava por motor (omitidos mantêm atual), **persiste na NVS**, reaplica saídas. Desabilita a malha por segurança. `423` em arming |
| `GET /resetCalibration` | Restaura defaults de fábrica, persiste e reaplica |

**Sanitização**: `trim` ∈ −100..+100 µs; `start/max` dentro de 1000..2000. Valores persistem
após reboot (SC-008).
