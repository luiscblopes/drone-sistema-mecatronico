<!-- SPECKIT START -->
## Feature ativa: 001-firmware-juncao-voo

Firmware **Arduino-ESP32** novo (pasta `Junção/firmware`) que roda na placa do `Drone-main`,
unindo a lógica de voo comprovada do `ESP32-Flight-controller` (angle-mode, filtro
complementar 0.991/0.009, PID cascata a **250 Hz**, clamps ±20°/±400) à infraestrutura
Wi-Fi/web/failsafes do `Drone-main`. Estratégia A: manter o código Arduino e reimplementar a
camada Wi-Fi/web/NVS com libs do core (`WiFi`, `WebServer`, `Preferences`, `ESP32Servo`,
`Wire`), reaproveitando `index.html` e replicando o contrato das rotas HTTP.

**Hardware**: IMU MPU-9250 I2C 21/22 (remap +90°Y); motores GPIO 25/26/27/14; ESC 1000–2000 µs
@ 250 Hz. **Failsafes** (latch): timeout 500 ms, IMU inválida, tilt 65°, e-stop (`/stopAll`
destrava). **Toda validação SEM HÉLICES** (quickstart.md é portão de aceite).

Artefatos de planejamento:
- Plano: `specs/001-firmware-juncao-voo/plan.md`
- Pesquisa/decisões: `specs/001-firmware-juncao-voo/research.md`
- Modelo de dados: `specs/001-firmware-juncao-voo/data-model.md`
- Contratos: `specs/001-firmware-juncao-voo/contracts/` (http-api, hardware-config)
- Roteiro de bancada: `specs/001-firmware-juncao-voo/quickstart.md`

Idioma de código/docs: **pt-BR**, Doxygen (Princípio II). Caminho de PWM/controle é
**contrato congelado** (Princípio I). Próximo: `/speckit-tasks`.
<!-- SPECKIT END -->
