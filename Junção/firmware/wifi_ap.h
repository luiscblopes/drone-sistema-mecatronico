/**
 * @file wifi_ap.h
 * @brief Ponto de acesso Wi-Fi (SoftAP) — único canal de comando.
 *
 * A placa nao tem receptor RC; o comando vem por Wi-Fi. Este modulo sobe um AP
 * proprio (SSID/senha em config.h), sem depender de roteador ou internet. O
 * cliente (celular/PC) conecta nesse AP e acessa o console em 192.168.4.1.
 */
#ifndef WIFI_AP_H
#define WIFI_AP_H

#include <stdbool.h>

/**
 * @brief Sobe o SoftAP com o SSID/senha do contrato.
 * @return true se o AP foi iniciado.
 */
bool wifi_ap_start(void);

#endif /* WIFI_AP_H */
