# Painel de controle web — ESP32 Flight Controller (sintonia de PID)

`index.html` é um **painel moderno** que representa o "controle" por Wi-Fi deste projeto: a
**sintonia de PID** exposta pelo firmware de teste
[`test/anglemode_flightcontroller_ver3.1_PID_values_tuning_webserver`](../test/anglemode_flightcontroller_ver3.1_PID_values_tuning_webserver).

Substitui o formulário cru original por uma interface limpa, responsiva (celular/PC), agrupada
por malha (taxa, ângulo, yaw) + tempo de ciclo — **sem mudar o firmware** (mesmos endpoints).

## O que ele controla

| Grupo | Campos (endpoint) | Referência |
|---|---|---|
| Taxa Roll/Pitch | `pGain`, `iGain`, `dGain` | 0.625 / 2.1 / 0.008 |
| Ângulo Roll/Pitch | `pAGain`, `iAGain`, `dAGain` | 2 / 0 / 0.007 |
| Yaw (taxa) | `pYaw`, `iYaw`, `dYaw` | 4 / 3 / 0 |
| Tempo de ciclo | `tc` | 0.004 s (250 Hz) |

Cada "Salvar" envia `GET /get?<param>=<valor>` (um parâmetro por requisição, como o firmware
espera). O firmware grava no SPIFFS; os valores **só são aplicados em voo quando o canal 5 do
rádio (CH5/gear) passa de 1500**.

## Como usar

### Opção A — servido pelo próprio ESP (drop-in, recomendado)
O firmware serve o HTML de uma string PROGMEM `index_html[]` com `send_P(..., processor)`, que
preenche os tokens `%pGain%`…`%tc%` com os valores atuais. Para trocar a interface:

1. Abra `index.html` desta pasta, copie todo o conteúdo.
2. No `.ino`, substitua o conteúdo entre `R"rawliteral(` e `)rawliteral"` da variável
   `index_html[]` por este HTML (os tokens `%param%` já estão preservados aqui).
3. Compile e grave. Acesse o IP do ESP (mostrado no monitor serial, modo STA) no navegador.

> Os valores atuais aparecem sozinhos (o `processor` do firmware preenche os `%tokens%`).

### Opção B — standalone (preview ou controle remoto)
Abra `index.html` direto no navegador. Os valores atuais aparecem como "—" (não há como lê-los
sem o servidor). Informe o **IP do ESP** no campo "ESP (modo standalone)" e use os botões
"Salvar" — eles enviam para `http://<ip>/get?...`.

> Em standalone, o navegador usa `mode:'no-cors'` (o firmware responde texto simples); o envio
> funciona, mas a resposta não é lida — confirme pelo comportamento do drone/serial.

## Segurança
**SEM HÉLICES** durante toda a sintonia. Mudar `tc` altera os termos I e D (eles dependem do
passo de tempo) — ajuste com cautela e revalide.
