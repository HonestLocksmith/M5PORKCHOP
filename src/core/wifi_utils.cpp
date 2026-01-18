#include "wifi_utils.h"
#include <Arduino.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <WiFi.h>

namespace WiFiUtils {

static void ensureNvsReady() {
    // Safe even if already initialised
    (void)nvs_flash_init();
}

void stopPromiscuous() {
    // Promiscuous must be off before switching modes / reconnecting
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
}

void hardReset() {
    stopPromiscuous();

    // IMPORTANT:
    // Do NOT power WiFi off. That triggers esp_wifi_deinit()/esp_wifi_init()
    // and on no-PSRAM builds it often fails to allocate RX buffers:
    //  wifiLowLevelInit(): esp_wifi_init 257
    WiFi.persistent(false);
    WiFi.setSleep(false);

    // Keep STA mode enabled (driver stays alive)
    WiFi.mode(WIFI_STA);

    // THIS IS THE FIX:
    // disconnect(wifioff=false, eraseap=true)
    // wifioff=true causes driver teardown → RX buffer allocation failure later
    WiFi.disconnect(false, true);

    delay(80);
    ensureNvsReady();
}

void shutdown() {
    stopPromiscuous();

    // Soft shutdown (no driver teardown)
    WiFi.persistent(false);

    // Same fix here: never power off WiFi driver
    WiFi.disconnect(false, true);

    // Keep STA mode (do not go WIFI_OFF)
    WiFi.mode(WIFI_STA);

    delay(80);
}

}  // namespace WiFiUtils
