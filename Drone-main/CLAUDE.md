# CLAUDE.md

Este arquivo orienta o Claude Code (claude.ai/code) ao trabalhar com o codigo
deste repositorio.

## O que e

Firmware em **C** sobre **ESP-IDF 4.4** para uma controladora de voo de
quadricoptero baseada em **ESP32**. Gera o PWM dos 4 ESCs, faz a fusao dos
sensores (IMU/barometro/GPS), estabiliza o voo com um laco PID em cascata e expoe
um console web por Wi-Fi (Access Point) para comando, telemetria e calibracao. O
codigo e escrito para ser compativel com **MISRA C**; os caminhos de PWM e de
controle sao tratados como *contrato congelado*, validado em voo real.
`main/config.h` e esse contrato — pinos, faixas de PWM, ganhos PID e limiares de
seguranca. Seus valores vem de calibracao/voo reais; nao altere sem motivo
concreto.

Comentarios e documentacao estao em portugues (pt-BR). Mantenha esse idioma ao
editar comentarios/docs.

## Build / flash / monitor

O ESP-IDF 4.4 precisa estar instalado e seu ambiente exportado primeiro.

```bash
. $IDF_PATH/export.sh            # Linux/macOS; no Windows: %IDF_PATH%\export.bat
idf.py set-target esp32          # apenas na primeira vez
idf.py build
idf.py -p <PORTA> flash monitor  # <PORTA> = COMx (Windows) ou /dev/ttyUSB0; sair do monitor: Ctrl+]
idf.py -p <PORTA> erase-flash    # reset de fabrica, apaga a calibracao da NVS
idf.py menuconfig                # opcional; defaults relevantes ja em sdkconfig.defaults
```

`main/index.html` e embutido no binario via `EMBED_TXTFILES` — nao ha sistema de
arquivos para gravar. Adicionar um `.c` exige inclui-lo em `main/CMakeLists.txt`
(`SRCS`) e qualquer novo componente ESP-IDF em `REQUIRES`.

## Testes

**Nao ha suite de testes automatizada.** A validacao e manual, em bancada, **SEM
HELICES** — siga `TESTE_BANCADA.md` (boot, conectividade, deadband por motor,
sensores, malha de controle, failsafes e conferencia do pulso do ESC no
osciloscopio). `TESTE_ALTERANDO_INICIAL.md` e um registro de ensaios de sentido
de rotacao dos motores.

Documentacao: `doxygen Doxyfile` → `docs/doxygen/html/index.html`. A CI
(`.github/workflows/docs.yml`) regenera isso para o GitHub Pages a cada push na
`main`.

## Arquitetura de runtime

A ordem de boot em `app_main()` e deliberada e voltada a seguranca:
carregar calibracao (NVS) → init ESC → **armar (segura o minimo por
`ESC_ARM_HOLD_MS`)** → sensores → Wi-Fi AP → init do controle → servidor web →
tarefas. Os ESCs sao armados no minimo *antes* de a rede e o laco de controle
subirem, mantendo os motores no minimo durante toda a partida.

Tres contextos de execucao concorrentes compartilham estado:

- **`telemetry_task`** (core 1, prio 1, ~2 ms): chama `sensor_hub_update()`.
  Mantida fora do caminho de tempo real porque leituras I2C/UART podem bloquear.
- **`flight_control_task`** (core 1, prio 2, 20 ms fixo via `vTaskDelayUntil`):
  o laco de controle de tempo real. Prioridade maior que a telemetria.
- **httpd** (`web_server.c`): serve o console embutido e as rotas JSON, alterando
  setpoints/ganhos/calibracao a partir dos handlers de requisicao.

### Agregador de sensores (`sensor_hub.c/.h`)
A unica fronteira produtor/consumidor dos sensores. `telemetry_task` escreve;
controle de voo e httpd leem via getters `*_snapshot`. Todas as copias entre
tarefas ocorrem dentro de uma secao critica `portMUX`, garantindo snapshots
consistentes. O hub tambem gerencia a cadencia por sensor (IMU rapida, barometro
lento, GPS continuo) e reconecta sensores que cairam. Drivers por tras dele:
`mpu9259` (IMU + fusao de atitude), `bmp280` (altitude relativa), `neo6m_gps`
(parser NMEA), todos sobre o `i2c_bus` compartilhado.

### Caminho de controle (`flight_control.c`, `drone_pid.c/.h`)
A cada ciclo de 20 ms, com o controle habilitado, o laco roda as verificacoes de
seguranca **em ordem** (timeout de comando → throttle abaixo do minimo → IMU
valida → IMU calibrada → inclinacao segura), depois calcula o controlador, faz a
mixagem e escreve nos ESCs. O `drone_pid` tem tres camadas:
1. `pid_controller_t` — PID escalar com anti-windup + saturacao de saida.
2. `quad_pid_controller_t` — mixagem direta angulo→us (sem realimentacao da IMU);
   preview/manual.
3. `attitude_rate_controller_t` — **cascata** (P externo: angulo→setpoint de taxa;
   PID interno por eixo sobre a taxa do giroscopio). E o modo estabilizado em voo.

`quad_pid_mix_x()` mistura o empuxo base + correcoes por eixo nos 4 pulsos de
motor para o **frame em X** (ordem M1 frente-esq, M2 frente-dir, M3 tras-dir,
M4 tras-esq — definida em `config.h`; **nao reordene**, isso define a mixagem).

### Failsafes
Os motivos sao `flight_failsafe_reason_t`. Condicoes perigosas (timeout de
comando, IMU invalida/nao calibrada, inclinacao excessiva, parada de emergencia)
**travam (latch)** — o laco nao rearma ate o operador limpar a trava
explicitamente (`flight_control_clear_failsafe_latch`).

### Modelo de concorrencia (importante ao editar flight_control.c)
Dois locks distintos, cada um com sua funcao:
- `s_data_mutex` (secao critica `portMUX`): protege os dados de voo compartilhados
  entre a tarefa de controle e o httpd (setpoint, estado, ultimas saidas). Segure
  por pouco tempo.
- `s_controller_mutex` (semaforo FreeRTOS com **timeouts curtos**): serializa o
  `compute` do controlador vs. mudancas de ganho. O laco de 20 ms usa timeout de
  5 ms e trata a falha em obter o mutex como failsafe, em vez de travar o laco.

Toda escrita em `s_vertical` ocorre **somente** na tarefa de controle — as funcoes
`set_vertical_*` apenas setam flags `volatile` que a tarefa aplica, mantendo esse
estado com um unico escritor.

### Controle vertical (climb-rate) (`vertical_control.c/.h`)
Um hold de velocidade vertical interno, **opcional e desligado por padrao** (PI
sobre a velocidade vertical de um filtro complementar de acelerometro + baro). E
mantido deliberadamente **separado do caminho de controle congelado** e so
substitui o empuxo base quando engatado. Uma "trava" do baro o desengata
(bumpless, devolvendo o throttle ao operador) se a referencia do baro estiver
velha.

## Calibracao dos ESCs (`esc_pwm.c/.h`)
A saida dos ESCs e um singleton (nao ha instancia a passar). Distingue *valor
logico* (1000–2000 us, o que o resto do sistema usa) da *saida calibrada* (por
motor: start/max/trim + piso de operacao, persistidos na NVS via
`calibration_store`). `esc_set_motor_speed_quiet()` e a variante sem log que o
laco de 20 ms deve usar (logar por motor a cada ciclo prejudicaria o tempo real).
`esc_run_deadband_sweep()` e uma ferramenta de bancada **bloqueante** — sem
helices.

## API web
Rotas (handlers em `web_server.c`, pagina em `index.html`):
`/` `/status` `/health` `/sensors` `/setMotor` `/stopAll` `/setFlight`
`/flightStatus` `/resetPid` `/calibration` `/setCalibration` `/resetCalibration`
`/findDeadband`. Defaults do AP: SSID `EQUIPE4-AP`, senha `12345678`,
http://192.168.4.1 (definidos em `config.h`). O JSON e montado com a macro
`J_APPEND` (snprintf, limitada por tamanho) — um desvio MISRA documentado,
intencionalmente fora da cadeia de PWM.
