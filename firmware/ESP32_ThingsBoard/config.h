#ifndef PUMP_GATEWAY_CONFIG_H
#define PUMP_GATEWAY_CONFIG_H

/* Optional private file. Copy secrets.h.example to secrets.h and edit it. */
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef THINGSBOARD_TOKEN
#define THINGSBOARD_TOKEN ""
#endif
#ifndef THINGSBOARD_HOST
#define THINGSBOARD_HOST "mqtt.thingsboard.cloud"
#endif
#ifndef MQTT_USE_TLS
#define MQTT_USE_TLS 1
#endif
#ifndef THINGSBOARD_PORT
#if MQTT_USE_TLS
#define THINGSBOARD_PORT 8883
#else
#define THINGSBOARD_PORT 1883
#endif
#endif
#ifndef THINGSBOARD_ROOT_CA
#include "trusted_roots.h"
#define THINGSBOARD_ROOT_CA PUMP_TRUSTED_ROOTS
#endif
/* WiFi SSID/password are provisioned by WiFiManager and retained in NVS. */
#ifndef WIFI_PORTAL_BUTTON_PIN
#define WIFI_PORTAL_BUTTON_PIN 27
#endif
#ifndef WIFI_PORTAL_PASSWORD
#define WIFI_PORTAL_PASSWORD "PumpSetup27"
#endif
#define WIFI_PORTAL_HOLD_MS 5000UL
#define WIFI_PORTAL_DEBOUNCE_MS 40UL
#define WIFI_PORTAL_TIMEOUT_MS 180000UL
#define WIFI_RECONNECT_MS 15000UL
#define STM32_UART_RX_PIN 16
#define STM32_UART_TX_PIN 17
#define STM32_UART_BAUD 115200
#define MASTER_UART_TIMEOUT_MS 3500UL
#define NODE_RF_TIMEOUT_MS 8000UL
#define SENSOR_FRESH_MS 5000UL
#endif
