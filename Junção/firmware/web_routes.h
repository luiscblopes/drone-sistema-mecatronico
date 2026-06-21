/**
 * @file web_routes.h
 * @brief Servidor HTTP (rotas do contrato Drone-main), no core 0.
 *
 * Sobe um WebServer e registra as rotas que o console (index.html) consome:
 * pagina, telemetria, comando de voo, bancada e calibracao. Roda numa tarefa
 * separada da malha de controle para nao roubar o tempo do laco de 250 Hz.
 * Replica o CONTRATO das rotas do Drone-main (ver contracts/http-api.md).
 */
#ifndef WEB_ROUTES_H
#define WEB_ROUTES_H

#include <stdbool.h>

/** @brief Inicializa o WebServer e registra as rotas (nao inicia a tarefa). */
void web_routes_init(void);

/**
 * @brief Cria a tarefa do servidor web (core 0) que serve as requisicoes.
 * @return true se a tarefa foi criada.
 */
bool web_routes_start(void);

/** @brief Indica se a tarefa do servidor web existe (usado por /health). */
bool web_routes_running(void);

#endif /* WEB_ROUTES_H */
