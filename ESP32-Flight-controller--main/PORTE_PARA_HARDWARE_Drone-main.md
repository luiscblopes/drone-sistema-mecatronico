# Porte do `ESP32-Flight-controller` (Arduino) para o hardware do `Drone-main`

> Documento de análise e plano de migração. Objetivo: rodar a lógica de voo do
> projeto **`ESP32-Flight-controller`** (sketch Arduino, angle-mode comprovado em
> voo) no **hardware físico do `Drone-main`** (placa ESP32 desenhada para
> controle por Wi-Fi, com MPU-9250, BMP280 e GPS NEO-6M).

> ⚠️ **SEGURANÇA:** toda a validação descrita aqui é feita **SEM HÉLICES**. Os
> motores e hélices têm energia destrutiva. Siga o `TESTE_BANCADA.md` do
> `Drone-main` antes de qualquer voo.

---

## 1. Visão geral dos dois projetos

| Aspecto | `ESP32-Flight-controller` (origem) | `Drone-main` (hardware-alvo) |
|---|---|---|
| Framework | **Arduino** (core ESP32) | **ESP-IDF 4.4** (SDK nativo Espressif) |
| Linguagem | C++ (`.ino`) | **C puro** (`.c`/`.h`), estilo MISRA C |
| Build | Arduino IDE / `arduino-cli` | **CMake + `idf.py`** |
| Estrutura | 1 sketch `setup()`/`loop()` | Componentes modulares + tarefas FreeRTOS |
| Bibliotecas | `Wire`, `ESP32Servo` | `driver/ledc`, `driver/i2c`, `esp_http_server`, `nvs_flash`, `freertos` |
| Nº de ESP32 | **1** | **1** |
| Nº de ESCs | 4 (PWM) | 4 (PWM) |

> **Observação importante:** os dois rodam no **mesmo chip (ESP32)**, mas em
> **frameworks diferentes**. O Arduino-ESP32 é uma camada construída *por cima* do
> ESP-IDF. Por isso o `Drone-main` é mais baixo nível, modular e controlável.

---

## 2. Inventário de hardware

| Componente | `ESP32-Flight-controller` | `Drone-main` (alvo) |
|---|---|---|
| MCU | 1× ESP32 | 1× ESP32 |
| IMU | MPU-6050 (6 eixos) | **MPU-9250/9255** (9 eixos: + magnetômetro AK8963) |
| Barômetro | ❌ | ✅ BMP280 |
| GPS | ❌ | ✅ NEO-6M (UART) |
| Receptor RC | ✅ 6 canais PWM | ❌ **não existe** |
| Rede | ❌ (no FC principal) | ✅ Wi-Fi AP (`esp_http_server`) |
| ESCs | 4 | 4 |
| LED status | GPIO 15 | — |

---

## 3. Mapeamento de pinos (GPIO)

| Função | Arduino FC | Drone-main (alvo) | Situação |
|---|---|---|---|
| I2C **SDA** | 21 (Wire default) | 21 | ✅ **igual** |
| I2C **SCL** | 22 (Wire default) | 22 | ✅ **igual** |
| Motor **M1** | 13 | **25** | ⚠️ re-pinar |
| Motor **M2** | 12 | **26** | ⚠️ re-pinar |
| Motor **M3** | 14 | **27** | ⚠️ re-pinar |
| Motor **M4** | 27 | **14** | ⚠️ re-pinar |
| RC canal 1 (roll) | 34 | — | ❌ sem receptor |
| RC canal 2 (pitch) | 35 | — | ❌ sem receptor |
| RC canal 3 (throttle) | 32 | — | ❌ sem receptor |
| RC canal 4 (yaw) | 33 | — | ❌ sem receptor |
| RC canal 5/6 (aux) | **25 / 26** | — (são motores!) | ❌ **conflito direto** |
| GPS RX/TX | — | 16 / 17 (UART2) | não usado pelo Arduino |
| LED | 15 | — | opcional |

**Conflito crítico:** o Arduino lê os canais 5 e 6 do rádio nos GPIO **25 e 26** —
que no hardware do `Drone-main` são as **saídas dos motores M1 e M2**. Além disso,
o ESC do Arduino sai no GPIO **13**, que **não está conectado a nenhum ESC** na
placa do `Drone-main`. Ou seja: a pinagem do sketch **precisa ser totalmente
remapeada**.

---

## 4. Comparação detalhada por subsistema

### 4.1 IMU — ✅ compatível (a melhor notícia)

O MPU-9250 é **register-compatible** com o MPU-6050 no núcleo accel/gyro:

| Item | MPU-6050 (Arduino) | MPU-9250 (Drone) | Compatível? |
|---|---|---|---|
| Endereço I2C | 0x68 | 0x68/0x69 | ✅ |
| Power mgmt (0x6B) | igual | igual | ✅ |
| Config DLPF (0x1A), Gyro FS (0x1B), Accel FS (0x1C) | igual | igual | ✅ |
| Accel data (0x3B), Gyro data (0x43) | igual | igual | ✅ |
| Escala giro `/65.5` (±500°/s) | igual | igual | ✅ |
| Escala accel `/4096` (±8g) | igual | igual | ✅ |
| Magnetômetro (AK8963) | inexistente | existe (ignorável) | — |

➡️ **O código de leitura de IMU do Arduino funciona no MPU-9250 sem alteração de
registradores.** O magnetômetro fica sem uso (o Arduino não precisa dele para
angle-mode).

### 4.2 Orientação física da IMU — ⚠️ precisa de remap

O `Drone-main` monta a IMU **girada 90°** e compensa por software
(`config.h: MPU_REMAP_ROTATE_Y_90 = 1`). Em `mpu9259.c`, antes de calcular a
atitude, aplica a accel **e** gyro:

```
novo_x = antigo_z ;  novo_y = antigo_y ;  novo_z = -antigo_x   (rotação +90° em Y)
```

Em repouso, a gravidade cai no eixo **−X** (`accel ≈ [-1, 0, 0]`), não no +Z.
O Arduino assume IMU "deitada" (Z para cima) e calcula:

```c
AngleRoll  =  atan(AccY / sqrt(AccX*AccX + AccZ*AccZ)) * 57.29;
AnglePitch = -atan(AccX / sqrt(AccY*AccY + AccZ*AccZ)) * 57.29;
```

➡️ **Sem aplicar o mesmo remap (ou remontar a IMU com Z para cima), roll/pitch do
Arduino sairão trocados/invertidos** nesta placa. O `Drone-main` ainda usa
`CONTROL_SWAP_ROLL_PITCH = true`, outro indício da orientação não-trivial.

### 4.3 Comando de voo — ❌ bloqueio arquitetural

| | Arduino FC | Drone-main (alvo) |
|---|---|---|
| Fonte de comando | **Rádio RC** (6 canais PWM) | **Wi-Fi** (HTTP/JSON) |
| Mapeamento | ch1=roll, ch2=pitch, ch3=throttle, ch4=yaw | rotas `/setFlight`, `/setMotor` |

O hardware do `Drone-main` **não tem receptor RC**, e os pinos que o RC do Arduino
usaria (25/26) já são motores. **Não dá para rodar o Arduino "como está".** O
comando precisa vir do Wi-Fi.

No sketch, os 4 setpoints entram aqui (substituir a leitura do `ReceiverValue[]`):

```c
DesiredAngleRoll  = 0.1f  * (ReceiverValue[0] - 1500);  // ch1 → roll
DesiredAnglePitch = 0.1f  * (ReceiverValue[1] - 1500);  // ch2 → pitch
InputThrottle     =          ReceiverValue[2];          // ch3 → throttle
DesiredRateYaw    = 0.15f * (ReceiverValue[3] - 1500);  // ch4 → yaw
```

➡️ Estes 4 valores passariam a vir do servidor web (como o `Drone-main` já faz em
`web_server.c`/`flight_control.c`).

#### 4.3.1 Opções de canal de comando (sem receptor RC)

O ESP32 tem **um único rádio 2.4 GHz** que faz Wi-Fi, Bluetooth Classic (SPP) e
BLE. Como a placa não tem receptor RC, o comando pode vir de qualquer um destes
canais:

| Opção | Como funciona | Alcance típico | Latência | UI | Esforço |
|---|---|---|---|---|---|
| **Wi-Fi AP** *(o Drone-main já faz)* | ESP32 cria a rede `EQUIPE4-AP`; abre-se `192.168.4.1` no navegador | ~30–50 m (linha de visada) | baixa-média | ✅ navegador, sem app | **zero** (já existe) |
| **Bluetooth Classic (SPP)** | "Porta serial" sobre BT; pareia o celular/PC e envia comandos | ~10 m | baixa | ❌ precisa de app | médio |
| **BLE** | Comandos via app ou Web Bluetooth (Chrome) | ~10–30 m | média | app ou web | médio |
| **RC de verdade** (referência) | Receptor + transmissora | centenas de m | mínima | rádio físico | precisa de HW que a placa não tem |

**Pontos de decisão:**
- **Wi-Fi já está pronto** (`wifi_ap.c`, `web_server.c`, console `index.html`).
  Migrar para BT exige reescrever essa camada e, no BT Classic, **perde-se a
  interface web** (precisaria de um app dedicado).
- **Alcance:** Wi-Fi (~30–50 m) > Bluetooth (~10 m). Nenhum chega perto do RC, mas
  para bancada/voos curtos o Wi-Fi é mais folgado.
- **Coexistência (armadilha):** rádio único 2.4 GHz — rodar **Wi-Fi + BT ao mesmo
  tempo** degrada os dois e consome muita RAM/flash. Na prática é **ou Wi-Fi ou
  BT** como canal principal.
- **BLE** é o meio-termo: mantém UI no navegador via **Web Bluetooth** (sem app),
  com baixo consumo, mas menor alcance/taxa que Wi-Fi e exige uma camada GATT nova.

➡️ **Recomendação:** ficar com o **Wi-Fi** que o `Drone-main` já tem (menor risco e
esforço, mais alcance, comando + telemetria + calibração pelo navegador).
**Bluetooth só compensa** se houver requisito específico (ex.: app de celular
dedicado sem entrar em rede Wi-Fi) — e, nesse caso, preferir **BLE** (mantém UI no
navegador), não BT Classic.

### 4.4 PWM dos ESCs — ⚠️ reconfigurar frequência

| Item | Arduino | Drone-main |
|---|---|---|
| Geração | `ESP32Servo` (`writeMicroseconds`) | `driver/ledc` |
| Faixa | 1000–2000 µs | 1000–2000 µs |
| **Frequência** | **500 Hz** | **250 Hz** |
| Arming | escreve 1000 µs no boot | segura mínimo por `ESC_ARM_HOLD_MS` (5 s) |

➡️ A faixa de pulso é igual; só a **frequência muda (500 → 250 Hz)**. Use 250 Hz
(valor validado nos ESCs desta placa).

### 4.5 Taxa do loop de controle — ⚠️ re-sintonia de PID

| | Arduino | Drone-main |
|---|---|---|
| Taxa | **250 Hz** (`t = 0,004 s`) | **50 Hz** (20 ms) |

Os termos I e D do PID dependem do `dt`. **Os ganhos do Arduino foram sintonizados
a 250 Hz e não são transferíveis direto para 50 Hz.** Ou mantém-se 250 Hz na
implementação portada, ou re-sintoniza para a taxa escolhida.

### 4.6 Mixagem de motores — ⚠️ NÃO portável (re-derivar)

**Mixagem do Arduino** (pinos 13/12/14/27):

```
M1 (frente-dir, CCW) = T − R − P − Y
M2 (trás-dir,  CW)   = T − R + P + Y
M3 (trás-esq,  CCW)  = T + R + P − Y
M4 (frente-esq,CW)   = T + R − P + Y
```

**Mixagem do Drone-main** (pinos 25/26/27/14, de `drone_pid.c`):

```
M1 (frente-esq) = T + P + R − Y
M2 (frente-dir) = T + P − R + Y
M3 (trás-dir)   = T − P − R − Y
M4 (trás-esq)   = T − P + R + Y
```

Comparando por **posição física**:

| Posição | Arduino | Drone-main | Diferença |
|---|---|---|---|
| Roll (±R) | esquerda +R / direita −R | esquerda +R / direita −R | ✅ mesma convenção |
| **Pitch (±P)** | frente −P / trás +P | frente +P / trás −P | ❌ **sinal invertido** |
| **Yaw / diagonais** | par CW = trás-dir + frente-esq | par CW = frente-dir + trás-esq | ❌ **diagonal de rotação oposta** |

➡️ **A mixagem do Arduino inverteria o pitch e atribuiria o torque de yaw à
diagonal errada nesta placa.** A mixagem **deve seguir as convenções físicas do
`Drone-main`** (posição dos motores + sentido de rotação validados nesse
airframe), não as do Arduino.

### 4.7 Demais diferenças

| Subsistema | Arduino | Drone-main |
|---|---|---|
| Estimador de atitude | Filtro complementar (Kalman comentado) | Fusão complementar no driver |
| Controle | Cascata ângulo→taxa **inline** no `loop()` | Cascata modular (`drone_pid.c`) |
| Failsafe | só "desarma" se throttle < 1030 | latch completo: timeout, IMU inválida/não calibrada, tilt > 65°, e-stop |
| Calibração | hardcoded no `setup()` | persistida em NVS + interface web |
| Concorrência | nenhuma (loop único) | 2 tarefas FreeRTOS + mutexes/`portMUX` |

---

## 5. Matriz de compatibilidade (resumo)

| Subsistema | Veredito | Ação |
|---|---|---|
| I2C (pinos 21/22) | ✅ Direto | Nenhuma |
| Leitura IMU (registradores/escala) | ✅ Direto | Nenhuma |
| Orientação da IMU | ⚠️ Reconfigurar | Aplicar remap +90° Y (accel+gyro) |
| Pinos de motor | ⚠️ Reconfigurar | 13/12/14/27 → 25/26/27/14 |
| Frequência ESC | ⚠️ Reconfigurar | 500 → 250 Hz |
| Taxa do loop / PID | ⚠️ Re-sintonia | Manter 250 Hz **ou** re-sintonizar p/ 50 Hz |
| Mixagem de motores | ⚠️ Re-derivar | Usar convenção física do Drone-main |
| **Comando de voo (RC)** | ❌ **Bloqueio** | Substituir RC por Wi-Fi |
| Failsafes / calibração NVS | ➕ Ausentes no Arduino | Recomendado adicionar |

---

## 6. O que deve ser feito — duas estratégias

### Estratégia A — Portar o sketch Arduino para a placa Drone-main

Reescrever/adaptar o firmware do Arduino para rodar fisicamente na placa. Como o
hardware é controlado por Wi-Fi e o ESP-IDF é o framework nativo, há dois caminhos:

- **A1 — Manter Arduino:** usar o core Arduino-ESP32 dentro do ESP-IDF (ou
  Arduino IDE) e adaptar o sketch. Mais simples de escrever, mas você **perde** a
  infraestrutura pronta do Drone-main (Wi-Fi, web, NVS, failsafes) e teria que
  recriá-la.
- **A2 — Reescrever em ESP-IDF:** traduzir a lógica do sketch para C/ESP-IDF na
  estrutura do Drone-main. Mais trabalho, mas alinhado ao hardware-alvo.

**Passos mínimos (A1/A2):**
1. **Pinos de motor:** `13,12,14,27` → `25,26,27,14`.
2. **Frequência ESC:** `500` → `250` Hz.
3. **Remap da IMU:** aplicar `novo_x=z, novo_y=y, novo_z=−x` a accel e gyro antes
   das equações de ângulo (ou remontar a IMU com Z para cima).
4. **Mixagem:** trocar pela convenção física do Drone-main (seção 4.6).
5. **Comando RC → Wi-Fi:** remover ISR/leitura dos canais; alimentar
   `DesiredAngleRoll/Pitch`, `InputThrottle`, `DesiredRateYaw` a partir de um
   servidor HTTP.
6. **PID:** manter 250 Hz **ou** re-sintonizar para a nova taxa.

### Estratégia B — Transplantar o "bom" do Arduino para o Drone-main (recomendada)

O `Drone-main` **já roda na sua placa** com Wi-Fi, web, NVS, failsafes e cascata
PID. O diferencial do Arduino é ser **comprovado em voo** (ganhos + filtro). Então,
em vez de portar o Arduino inteiro, transplante só o que vale:

1. **Valores de PID comprovados** do Arduino como ponto de partida da sintonia no
   Drone-main — lembrando de **converter para a taxa de 50 Hz** (ou alinhar a taxa).
2. **Lógica do filtro complementar** do Arduino (pesos `0.991/0.009`), se o
   comportamento dela for desejado, comparando com a fusão atual do `mpu9259.c`.
3. **Clamps e limites** (±20° de ângulo, ±400 de termo I) como referência.

➡️ **Menos retrabalho, mantém failsafes/segurança e a interface web, e aproveita o
que o Arduino tem de melhor.** Esta é a recomendação técnica.

---

## 7. Mudanças concretas de código (se seguir a Estratégia A com o sketch)

```c
// --- Pinos dos motores (era 13/12/14/27) ---
const int mot1_pin = 25;   // M1 frente-esq
const int mot2_pin = 26;   // M2 frente-dir
const int mot3_pin = 27;   // M3 trás-dir
const int mot4_pin = 14;   // M4 trás-esq

// --- Frequência do ESC (era 500) ---
int ESCfreq = 250;

// --- Remap da IMU (+90° em Y) aplicado após ler accel/gyro brutos ---
// novo_x = z ; novo_y = y ; novo_z = -x
float ax = AccZ,  ay = AccY,  az = -AccX;
float gx = RateYaw, gy = RatePitch, gz = -RateRoll;   // ajustar nomes/uso conforme o eixo
// ... usar ax/ay/az e gx/gy/gz nas equações de ângulo e no PID

// --- Mixagem na convenção física do Drone-main ---
MotorInput1 = InputThrottle + InputPitch + InputRoll - InputYaw; // M1 frente-esq
MotorInput2 = InputThrottle + InputPitch - InputRoll + InputYaw; // M2 frente-dir
MotorInput3 = InputThrottle - InputPitch - InputRoll - InputYaw; // M3 trás-dir
MotorInput4 = InputThrottle - InputPitch + InputRoll + InputYaw; // M4 trás-esq

// --- Comando: substituir ReceiverValue[] por valores recebidos via Wi-Fi ---
```

> ⚠️ Os sinais de remap e de mixagem acima são o **ponto de partida** derivado da
> documentação do Drone-main; precisam ser **confirmados em bancada** (seção 8),
> motor a motor, porque dependem do sentido de rotação real e do encaixe das
> hélices nesta placa.

---

## 8. Checklist de validação em bancada (SEM HÉLICES)

1. **Boot:** firmware sobe, ESCs armam no mínimo, IMU detectada em 0x68.
2. **IMU nivelada:** com a placa parada e nivelada, `roll ≈ 0` e `pitch ≈ 0`
   (confirma que o remap está correto). Incline para frente → pitch reage no
   sinal esperado; incline para a direita → roll reage no sinal esperado.
3. **Deadband por motor:** subir o pulso de cada motor isoladamente e anotar onde
   começa a girar (equivalente ao `/findDeadband` do Drone-main).
4. **Sentido de rotação:** confirmar CW/CCW de cada motor conforme o diagrama do
   airframe; inverter 2 das 3 fases motor↔ESC se necessário.
5. **Resposta da malha (sem hélices):** inclinar a placa na mão e verificar que os
   motores **certos** aceleram para corrigir (valida a mixagem e os sinais).
6. **Failsafes:** timeout de comando, tilt excessivo, parada de emergência.
7. **Só então:** hélices, ao ar livre, com extrema cautela.

---

## 9. Riscos e armadilhas

- **Inverter um eixo na mixagem** = drone capota imediatamente ao decolar. Validar
  na bancada (passo 5) é obrigatório.
- **Remap da IMU errado** = a malha "corrige" para o lado errado. Validar passo 2.
- **Reaproveitar ganhos de PID sem converter a taxa** = oscilação ou resposta
  lenta. Ganhos de 250 Hz ≠ ganhos de 50 Hz.
- **Frequência de ESC errada** = ESC apita / não arma. Usar 250 Hz nesta placa.
- **Esquecer os failsafes do Arduino** (ele praticamente não tem): rodar o Arduino
  "puro" nesta placa significa abrir mão das proteções que o Drone-main já tem.

---

## 10. Conclusão

| Pergunta | Resposta |
|---|---|
| A IMU é compatível? | **Sim** (MPU-9250 ≈ MPU-6050 no núcleo accel/gyro). |
| O I2C é compatível? | **Sim** (mesmos pinos 21/22). |
| Os motores/pinos? | **Não** — remapear 25/26/27/14 e re-derivar a mixagem. |
| O comando RC? | **Não** — a placa não tem receptor; usar Wi-Fi. |
| Caminho de menor risco? | **Estratégia B:** transplantar PID/filtro comprovados do Arduino para o Drone-main, que já roda nesta placa. |

O `Drone-main` foi **desenhado para esta placa e para controle por Wi-Fi**; o
Arduino é um FC angle-mode **comprovado em voo, mas amarrado a rádio RC**. A parte
de **sensor migra fácil**; **comando e pinagem/mixagem de motor não**. A
recomendação técnica é a **Estratégia B**.
