/**
 * @file flight_task.h
 * @brief Tarefa de controle de tempo real (250 Hz, core 1). CAMINHO CONGELADO.
 *
 * Roda o laço congelado a cada 4 ms: le a IMU, estima a atitude (filtro
 * complementar), avalia os failsafes, calcula a cascata e a mixagem e escreve nos
 * ESCs — mas SO quando a malha esta habilitada e sem failsafe. Quando desabilitada
 * (override manual/bancada), nao toca nos motores, deixando as rotas de bancada
 * (/setMotor, /findDeadband) atuarem.
 *
 * Fica numa tarefa FreeRTOS separada do servidor web para preservar o periodo de
 * 4 ms (o web roda no core 0). Publica um instantaneo de telemetria a cada ciclo.
 */
#ifndef FLIGHT_TASK_H
#define FLIGHT_TASK_H

#include <stdbool.h>

/** @brief Inicializa filtro, cascata e failsafe. Chamar antes de start. */
void flight_task_init(void);

/**
 * @brief Cria a tarefa de controle (core 1, prioridade alta).
 * @return true se a tarefa foi criada.
 */
bool flight_task_start(void);

/** @brief Indica se a tarefa de controle existe (usado por /health e /setFlight). */
bool flight_task_running(void);

/**
 * @brief Engata a malha estabilizada (chamado pela rota /setFlight).
 *
 * Reinicia a cascata e o filtro para nao "puxar" o drone ao ligar, e habilita o
 * controle. Recusa silenciosamente se houver failsafe travado (o chamador ja
 * verifica e responde 409).
 */
void flight_task_engage(void);

/**
 * @brief Desabilita a malha; opcionalmente para os motores. Registra o motivo.
 * @param reason failsafe_reason_t (int) do motivo da desabilitacao.
 * @param stop_motors Se true, leva os motores ao minimo imediatamente.
 */
void flight_task_disable(int reason, bool stop_motors);

#endif /* FLIGHT_TASK_H */
