/**
 * @file wifi_ap.cpp
 * @brief Implementação do SoftAP Wi-Fi.
 */
#include "wifi_ap.h"
#include "config.h"

#include <Arduino.h>
#include <WiFi.h>

bool wifi_ap_start(void)
{
    /* Modo somente AP (sem cliente). Rádio 2.4 GHz dedicado ao Wi-Fi (sem BT). */
    WiFi.mode(WIFI_AP);
    const bool ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL,
                                0 /* SSID visivel */, WIFI_AP_MAX_CONN);
    if (ok)
    {
        /* IP padrao do AP do ESP32: 192.168.4.1 (host do console). */
        Serial.print("[wifi] AP '");
        Serial.print(WIFI_AP_SSID);
        Serial.print("' em ");
        Serial.println(WiFi.softAPIP());
    }
    else
    {
        Serial.println("[wifi] FALHA ao iniciar o AP");
    }
    return ok;
}
