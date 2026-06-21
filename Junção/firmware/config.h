/**
 * @file config.h
 * @brief Contrato CONGELADO de hardware e controle do firmware de Junção.
 *
 * Centraliza, em um único lugar, todas as constantes de pino, PWM, orientação da
 * IMU, mixagem, ganhos de PID e limiares de segurança (Princípio IV da
 * constitution). Cada valor documenta seu significado físico e a ORIGEM:
 *   - "sketch": vem do firmware Arduino comprovado em voo (ESP32-Flight-controller).
 *   - "Drone-main": vem do hardware/contrato do projeto Drone-main.
 *   - "Clarifications": decidido na sessão de clarificação da spec.
 *   - "bancada": ponto de partida que DEVE ser confirmado sem hélices (quickstart.md).
 *
 * @warning Estes números definem o comportamento de voo e a segurança. Alterar
 * qualquer um exige justificativa escrita e revalidação em bancada (Princípio I/III).
 * O caminho de geração de PWM e o laço de controle são contrato congelado.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* ===========================================================================
 *  Motores / ESCs
 * ===========================================================================
 * A faixa 1000..2000 us e o padrao de ESC de drone (1 ms = parado, 2 ms = max).
 * 250 Hz e a taxa validada nos ESCs desta placa (Drone-main; o sketch usava 500
 * Hz, incompativel aqui). O arming segura o minimo por alguns segundos para o
 * ESC reconhecer "throttle zero" e habilitar, evitando partida acidental. */
#define NUM_MOTORS            (4)        /**< Quadricoptero em X. */
#define ESC_MIN_US            (1000)     /**< Pulso minimo (motor parado). [Drone-main/sketch] */
#define ESC_MAX_US            (2000)     /**< Pulso maximo. [Drone-main/sketch] */
#define ESC_FREQUENCY_HZ      (250)      /**< Taxa de atualizacao dos ESCs. [Drone-main] */
#define ESC_ARM_HOLD_MS       (5000U)    /**< Tempo segurando minimo no boot (arming). [Drone-main] */
#define MOTOR_OUTPUT_MAX_US   (1999)     /**< Teto de saturacao por motor. [sketch] */

/* GPIOs dos ESCs M1..M4, na ordem fisica (config em X). A ordem define a
 * mixagem; nao reordene sem revalidar. Re-pinagem 13/12/14/27 -> 25/26/27/14
 * (o sketch usava outros pinos; estes sao os ESCs reais desta placa). [Drone-main] */
#define ESC_GPIO_M1           (25)       /**< M1 frente-esquerda. */
#define ESC_GPIO_M2           (26)       /**< M2 frente-direita. */
#define ESC_GPIO_M3           (27)       /**< M3 tras-direita. */
#define ESC_GPIO_M4           (14)       /**< M4 tras-esquerda. */

/* Piso/corte de throttle por motor com a malha ativa. Abaixo do idle, o motor e
 * mantido no idle (evita regiao nao-linear de baixissima rotacao). O cutoff leva
 * ao minimo absoluto (desarmado). [sketch: ThrottleIdle=1170, ThrottleCutOff=1000] */
#define THROTTLE_IDLE_US      (1170)     /**< Piso por motor com malha ativa. [sketch] */
#define THROTTLE_CUTOFF_US    (1000)     /**< Corte total (desarmado). [sketch] */
#define THROTTLE_ARM_MIN_US   (1030)     /**< Abaixo disso nao arma os motores. [sketch] */
#define THROTTLE_MAX_CMD_US   (1800)     /**< Teto do throttle comandado. [sketch] */

/* Calibracao por motor (defaults), em microssegundos. start = pulso onde o motor
 * gira de forma confiavel; max = topo util; trim = ajuste fino. Persistidos na
 * NVS e ajustaveis pela interface. [Drone-main] */
#define DEFAULT_MOTOR_START_US   { 1100, 1100, 1100, 1100 }
#define DEFAULT_MOTOR_MAX_US     { 2000, 2000, 2000, 2000 }
#define DEFAULT_MOTOR_TRIM_US    { 0, 0, 0, 0 }
#define MOTOR_TRIM_MIN_US        (-100)  /**< Saturacao do trim (sanitizacao). [Drone-main] */
#define MOTOR_TRIM_MAX_US        (100)

/* ===========================================================================
 *  Barramento I2C / IMU
 * ===========================================================================
 * I2C a 400 kHz (fast mode). MPU-9250 e register-compatible com o MPU-6050 no
 * nucleo accel/gyro (endereco 0x68); o codigo de leitura do sketch funciona sem
 * mudar registradores. O magnetometro (AK8963) fica sem uso (angle-mode). */
#define I2C_SDA_GPIO          (21)       /**< SDA (igual nos dois projetos). */
#define I2C_SCL_GPIO          (22)       /**< SCL (igual nos dois projetos). */
#define I2C_CLOCK_HZ          (400000U)  /**< Fast mode. */
#define IMU_I2C_ADDR          (0x68)     /**< Endereco do MPU-9250. */

/* Escalas do sketch: gyro ±500 °/s (/65.5), accel ±8 g (/4096). */
#define GYRO_LSB_PER_DPS      (65.5f)    /**< ±500 °/s (reg 0x1B=0x08). [sketch] */
#define ACCEL_LSB_PER_G       (4096.0f)  /**< ±8 g (reg 0x1C=0x10). [sketch] */

/* Remap de orientacao da IMU: a IMU e montada girada nesta placa. Com o drone
 * nivelado, a gravidade cai no eixo -X. Aplica-se rotacao +90° em Y a accel E
 * gyro:  x' = z ; y' = y ; z' = -x  (assim "nivelado" volta a ler ~[0,0,+1] e
 * roll/pitch ~0). Defina 0 se a IMU for remontada com Z para cima.
 * ⚠️ Sinais finais (incl. yaw) confirmados na bancada (quickstart §2 e §5). [Drone-main] */
#define IMU_REMAP_ROTATE_Y_90 (1)

/* ===========================================================================
 *  Laço de controle (250 Hz) — base comprovada do sketch
 * ===========================================================================
 * dt = 0.004 s (250 Hz). Os termos I e D do PID dependem de dt; manter 250 Hz
 * preserva a validade dos ganhos comprovados em voo (Clarifications: 250 Hz fixos,
 * sem re-sintonia). */
#define CONTROL_LOOP_HZ       (250U)
#define CONTROL_DT_S          (0.004f)   /**< Periodo do laco. [sketch: t=0.004] */

/* Filtro complementar (estimador de atitude). Peso alto no giroscopio integrado e
 * baixo no acelerometro. Saida clampada a ±20°. [sketch] */
#define COMP_FILTER_GYRO_W    (0.991f)
#define COMP_FILTER_ACC_W     (0.009f)
#define ANGLE_CLAMP_DEG       (20.0f)    /**< Clamp do angulo estimado e comandado. [sketch] */

/* Saturacao dos PIDs: termo integral e saida de cada eixo limitados a ±400. [sketch] */
#define PID_OUTPUT_CLAMP      (400.0f)
#define PID_ITERM_CLAMP       (400.0f)

/* Ganhos comprovados (NAO re-sintonizar sem justificativa — Princípio III). [sketch] */
/* Malha externa (angulo -> setpoint de taxa). */
#define ANGLE_KP              (2.0f)
#define ANGLE_KI              (0.5f)
#define ANGLE_KD              (0.007f)
/* Malha interna (taxa) roll/pitch. */
#define RATE_RP_KP            (0.625f)
#define RATE_RP_KI            (2.1f)
#define RATE_RP_KD            (0.0088f)
/* Malha interna (taxa) yaw. */
#define RATE_YAW_KP           (4.0f)
#define RATE_YAW_KI           (3.0f)
#define RATE_YAW_KD           (0.0f)

/* Escala do setpoint de taxa de yaw (comando de guinada em angle-mode). [sketch: 0.15] */
#define YAW_RATE_SCALE        (0.15f)
/* Envelope de angulo comandado por Wi-Fi: ±20° (coincide com o clamp do filtro
 * complementar comprovado; mais conservador que os ±35° do Drone-main). [research D8] */
#define SETPOINT_ANGLE_MAX_DEG (20.0f)

/* ===========================================================================
 *  Segurança / Failsafe
 * ===========================================================================
 * Limiares decididos nas Clarifications da spec. Qualquer condicao perigosa leva
 * os motores ao minimo, desabilita a malha e TRAVA (latch) o rearme ate o
 * operador comandar PARAR TUDO (/stopAll). */
#define FLIGHT_COMMAND_TIMEOUT_MS (500U)   /**< Sem comando valido por mais que isso ⇒ failsafe. [Clarifications] */
#define MAX_SAFE_TILT_DEG         (65.0f)  /**< Inclinacao acima da qual desarma. [Clarifications/Drone-main] */
#define MPU_MAX_AGE_MS            (150U)   /**< Amostra de IMU mais velha ⇒ invalida. [Drone-main] */
#define CONTROL_MIN_THROTTLE_US   (1050)   /**< Abaixo disso, motores ao minimo (sem corrigir sem empuxo). [Drone-main] */

/* ===========================================================================
 *  Rede (Access Point Wi-Fi) — unico canal de comando (sem radio RC)
 * ===========================================================================*/
#define WIFI_AP_SSID          "EQUIPE4-AP" /**< SSID do AP. [Drone-main] */
#define WIFI_AP_PASSWORD      "12345678"   /**< Senha do AP. [Drone-main] */
#define WIFI_AP_CHANNEL       (1)
#define WIFI_AP_MAX_CONN      (4)
#define WEB_SERVER_PORT       (80)

/* ===========================================================================
 *  Tarefas FreeRTOS (preserva o caminho congelado: controle separado do web)
 * ===========================================================================*/
#define FLIGHT_TASK_CORE      (1)        /**< Tarefa de controle no core 1 (tempo real). */
#define WEB_TASK_CORE         (0)        /**< Servidor web no core 0. */
#define FLIGHT_TASK_PRIO      (configMAX_PRIORITIES - 1)
#define WEB_TASK_PRIO         (5)
#define FLIGHT_TASK_STACK     (8192)
#define WEB_TASK_STACK        (8192)

/* ===========================================================================
 *  Persistência (NVS via Preferences)
 * ===========================================================================*/
#define NVS_NS_MOTOR_CAL      "motor-cal"  /**< Namespace da calibracao de motor. [Drone-main] */
#define NVS_NS_IMU_CAL        "imu-cal"    /**< Namespace dos offsets da IMU. */

/* Offsets default de calibracao da IMU (do sketch; substituidos pelos da NVS se
 * existirem). Medidos no FC original; devem ser recalibrados nesta montagem. [sketch] */
#define IMU_DEFAULT_GYRO_OFFSET_DPS   { 0.27f, -0.85f, -2.09f } /**< roll, pitch, yaw */
#define IMU_DEFAULT_ACCEL_OFFSET_G    { 0.03f, 0.01f, -0.07f }  /**< x, y, z */

#endif /* CONFIG_H */
