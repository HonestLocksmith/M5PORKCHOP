// src/core/config.cpp
// Configuration management implementation

#include "config.h"
#include "sdlog.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <SPIFFS.h>
#include <SPI.h>

// ---- Cardputer microSD wiring (explicit, per Cardputer v1.1 schematic) ----
// ESP32-S3FN8:
//   microSD Socket  CS   MOSI  CLK   MISO
//                  G12  G14   G40   G39
//
// (Your previous patch used ESP32 “classic” pins + CS=4, which breaks SD on Cardputer/StampS3.)
static constexpr int SD_CS_PIN   = 12;  // CS
static constexpr int SD_MOSI_PIN = 14;  // MOSI
static constexpr int SD_MISO_PIN = 39;  // MISO
static constexpr int SD_SCK_PIN  = 40;  // SCK/CLK

// Dedicated SPI bus instance for SD.
// Cardputer microSD pinmap (from M5 docs): CS=12 MOSI=14 CLK=40 MISO=39.
// In practice, Arduino-ESP32/PlatformIO combos vary; using FSPI with explicit
// pins is the most reliable on Cardputer builds.
static SPIClass sdSPI(FSPI);
static bool sdSpiBegun = false;

// Static member initialization
GPSConfig Config::gpsConfig;
MLConfig Config::mlConfig;
WiFiConfig Config::wifiConfig;
BLEConfig Config::bleConfig;
PersonalityConfig Config::personalityConfig;
bool Config::initialized = false;
static bool sdAvailable = false;

static void ensureSdSpiReady() {
    // Re-init SD SPI bus cleanly
    if (sdSpiBegun) {
        sdSPI.end();
        sdSpiBegun = false;
        delay(20);
    }

    // Make sure CS is a sane GPIO output and deasserted before touching the bus.
    // This prevents random Select Failed errors on some cards.
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);

    // SCK, MISO, MOSI, SS/CS
    sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    sdSpiBegun = true;
    delay(20);
}

bool Config::init() {
    // Initialize SPIFFS first (always available)
    if (!SPIFFS.begin(true)) {
        Serial.println("[CONFIG] SPIFFS mount failed");
    }

    // Allow buses to stabilize after M5.begin()
    delay(50);

    // Ensure SD has a proper SPI bus configured
    ensureSdSpiReady();

    // Retry with progressive SPI speeds for reliability
    sdAvailable = false;
    const int maxRetries = 6;
    const uint32_t speeds[] = {
        25000000, // 25 MHz
        20000000, // 20 MHz
        10000000, // 10 MHz
        8000000,  // 8 MHz
        4000000,  // 4 MHz
        1000000   // 1 MHz
    };

    for (int attempt = 0; attempt < maxRetries && !sdAvailable; attempt++) {
        uint32_t speed = speeds[attempt];
        Serial.printf("[CONFIG] SD init attempt %d/%d at %luMHz\n",
                      attempt + 1, maxRetries, speed / 1000000);

        if (attempt > 0) {
            SD.end();     // Clean up previous failed attempt
            delay(80);    // Allow bus to settle
            ensureSdSpiReady();
        }

        // Use explicit CS + dedicated SPI + explicit speed
        if (SD.begin(SD_CS_PIN, sdSPI, speed)) {
            Serial.printf("[CONFIG] SD card mounted at %luMHz\n", speed / 1000000);
            sdAvailable = true;
        }
    }

    if (!sdAvailable) {
        Serial.println("[CONFIG] SD card init failed after retries, using SPIFFS");
    } else {
        SDLog::log("CFG", "SD card mounted OK");

        // Create directories on SD if needed
        if (!SD.exists("/handshakes")) SD.mkdir("/handshakes");
        if (!SD.exists("/mldata")) SD.mkdir("/mldata");
        if (!SD.exists("/models")) SD.mkdir("/models");
        if (!SD.exists("/logs")) SD.mkdir("/logs");
        if (!SD.exists("/wardriving")) SD.mkdir("/wardriving");
    }

    // Load personality from SPIFFS (always available)
    if (!loadPersonality()) {
        Serial.println("[CONFIG] Creating default personality");
        createDefaultPersonality();
        savePersonalityToSPIFFS();
    }

    // Load main config (SD if available, otherwise SPIFFS)
    if (!load()) {
        Serial.println("[CONFIG] Creating default config");
        createDefaultConfig();
        save();  // persist defaults to whichever FS is active
    }

    // Try to load WPA-SEC key from file (auto-deletes after import)
    if (loadWpaSecKeyFromFile()) {
        Serial.println("[CONFIG] WPA-SEC key loaded from file");
    }

    initialized = true;
    return true;
}

bool Config::isSDAvailable() {
    return sdAvailable;
}

bool Config::reinitSD() {
    Serial.println("[CONFIG] Attempting SD card re-initialization...");

    // Clean up any existing SD state
    SD.end();
    delay(80);

    // Re-init SD SPI bus explicitly
    ensureSdSpiReady();

    // Retry with progressive SPI speeds
    sdAvailable = false;
    const int maxRetries = 6;
    const uint32_t speeds[] = {
        25000000,
        20000000,
        10000000,
        8000000,
        4000000,
        1000000
    };

    for (int attempt = 0; attempt < maxRetries && !sdAvailable; attempt++) {
        uint32_t speed = speeds[attempt];
        Serial.printf("[CONFIG] SD reinit attempt %d/%d at %luMHz\n",
                      attempt + 1, maxRetries, speed / 1000000);

        if (attempt > 0) {
            SD.end();
            delay(80);
            ensureSdSpiReady();
        }

        if (SD.begin(SD_CS_PIN, sdSPI, speed)) {
            Serial.printf("[CONFIG] SD card mounted at %luMHz\n", speed / 1000000);
            sdAvailable = true;
        }
    }

    if (sdAvailable) {
        SDLog::log("CFG", "SD card re-initialized OK");

        if (!SD.exists("/handshakes")) SD.mkdir("/handshakes");
        if (!SD.exists("/mldata")) SD.mkdir("/mldata");
        if (!SD.exists("/models")) SD.mkdir("/models");
        if (!SD.exists("/logs")) SD.mkdir("/logs");
        if (!SD.exists("/wardriving")) SD.mkdir("/wardriving");
    } else {
        Serial.println("[CONFIG] SD card reinit failed");
    }

    return sdAvailable;
}

bool Config::load() {
    // Use SD if available, else SPIFFS fallback
    fs::FS& cfgFS = sdAvailable ? (fs::FS&)SD : (fs::FS&)SPIFFS;

    File file = cfgFS.open(CONFIG_FILE, FILE_READ);
    if (!file) {
        Serial.println("[CONFIG] Cannot open config file");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        Serial.printf("[CONFIG] JSON parse error: %s\n", err.c_str());
        return false;
    }

    // GPS config
    if (doc["gps"].is<JsonObject>()) {
        gpsConfig.enabled = doc["gps"]["enabled"] | true;
        gpsConfig.source = static_cast<GPSSource>(doc["gps"]["gpsSource"] | 0);

        // Auto-set pins based on source (ignore legacy rxPin/txPin from config)
        if (gpsConfig.source == GPSSource::CAP_LORA) {
            gpsConfig.rxPin = 15;  // Cap LoRa868 GPS RX
            gpsConfig.txPin = 13;  // Cap LoRa868 GPS TX
        } else {
            gpsConfig.rxPin = 1;   // Grove GPS RX (default)
            gpsConfig.txPin = 2;   // Grove GPS TX (default)
        }

        gpsConfig.baudRate = doc["gps"]["baudRate"] | 115200;
        gpsConfig.updateInterval = doc["gps"]["updateInterval"] | 5;
        gpsConfig.sleepTimeMs = doc["gps"]["sleepTimeMs"] | 5000;
        gpsConfig.powerSave = doc["gps"]["powerSave"] | true;
        gpsConfig.timezoneOffset = doc["gps"]["timezoneOffset"] | 0;
    }

    // ML config
    if (doc["ml"].is<JsonObject>()) {
        mlConfig.enabled = doc["ml"]["enabled"] | true;
        mlConfig.collectionMode = static_cast<MLCollectionMode>(doc["ml"]["collectionMode"] | 0);
        mlConfig.modelPath = doc["ml"]["modelPath"] | "/models/porkchop_model.bin";
        mlConfig.confidenceThreshold = doc["ml"]["confidenceThreshold"] | 0.7f;
        mlConfig.rogueApThreshold = doc["ml"]["rogueApThreshold"] | 0.8f;
        mlConfig.vulnScorerThreshold = doc["ml"]["vulnScorerThreshold"] | 0.6f;
        mlConfig.autoUpdate = doc["ml"]["autoUpdate"] | false;
        mlConfig.updateUrl = doc["ml"]["updateUrl"] | "";
    }

    // WiFi config
    if (doc["wifi"].is<JsonObject>()) {
        wifiConfig.channelHopInterval = doc["wifi"]["channelHopInterval"] | 500;
        wifiConfig.lockTime = doc["wifi"]["lockTime"] | 12000;
        wifiConfig.enableDeauth = doc["wifi"]["enableDeauth"] | true;
        wifiConfig.randomizeMAC = doc["wifi"]["randomizeMAC"] | true;
        wifiConfig.otaSSID = doc["wifi"]["otaSSID"] | "";
        wifiConfig.otaPassword = doc["wifi"]["otaPassword"] | "";
        wifiConfig.autoConnect = doc["wifi"]["autoConnect"] | false;
        wifiConfig.wpaSecKey = doc["wifi"]["wpaSecKey"] | "";
        wifiConfig.wigleApiName = doc["wifi"]["wigleApiName"] | "";
        wifiConfig.wigleApiToken = doc["wifi"]["wigleApiToken"] | "";
    }

    // BLE config (PIGGY BLUES)
    if (doc["ble"].is<JsonObject>()) {
        bleConfig.burstInterval = doc["ble"]["burstInterval"] | 200;
        bleConfig.advDuration = doc["ble"]["advDuration"] | 100;
    }

    Serial.println("[CONFIG] Loaded successfully");
    return true;
}

bool Config::loadPersonality() {
    // Load from SPIFFS (always available)
    File file = SPIFFS.open(PERSONALITY_FILE, FILE_READ);
    if (!file) {
        Serial.println("[CONFIG] Personality file not found in SPIFFS");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        Serial.printf("[CONFIG] Personality JSON error: %s\n", err.c_str());
        return false;
    }

    const char* name = doc["name"] | "Porkchop";
    strncpy(personalityConfig.name, name, sizeof(personalityConfig.name) - 1);
    personalityConfig.name[sizeof(personalityConfig.name) - 1] = '\0';

    personalityConfig.mood = doc["mood"] | 50;
    personalityConfig.experience = doc["experience"] | 0;
    personalityConfig.curiosity = doc["curiosity"] | 0.7f;
    personalityConfig.aggression = doc["aggression"] | 0.3f;
    personalityConfig.patience = doc["patience"] | 0.5f;
    personalityConfig.soundEnabled = doc["soundEnabled"] | true;
    personalityConfig.brightness = doc["brightness"] | 80;
    personalityConfig.dimLevel = doc["dimLevel"] | 20;
    personalityConfig.dimTimeout = doc["dimTimeout"] | 30;
    personalityConfig.themeIndex = doc["themeIndex"] | 0;
    uint8_t g0Action = doc["g0Action"] | static_cast<uint8_t>(G0Action::SCREEN_TOGGLE);
    if (g0Action >= G0_ACTION_COUNT) {
        g0Action = static_cast<uint8_t>(G0Action::SCREEN_TOGGLE);
    }
    personalityConfig.g0Action = static_cast<G0Action>(g0Action);

    Serial.printf("[CONFIG] Personality: %s (mood: %d, sound: %s, bright: %d%%, dim: %ds, theme: %d)\n",
                  personalityConfig.name,
                  personalityConfig.mood,
                  personalityConfig.soundEnabled ? "ON" : "OFF",
                  personalityConfig.brightness,
                  personalityConfig.dimTimeout,
                  personalityConfig.themeIndex);
    return true;
}

void Config::savePersonalityToSPIFFS() {
    JsonDocument doc;
    doc["name"] = personalityConfig.name;
    doc["mood"] = personalityConfig.mood;
    doc["experience"] = personalityConfig.experience;
    doc["curiosity"] = personalityConfig.curiosity;
    doc["aggression"] = personalityConfig.aggression;
    doc["patience"] = personalityConfig.patience;
    doc["soundEnabled"] = personalityConfig.soundEnabled;
    doc["brightness"] = personalityConfig.brightness;
    doc["dimLevel"] = personalityConfig.dimLevel;
    doc["dimTimeout"] = personalityConfig.dimTimeout;
    doc["themeIndex"] = personalityConfig.themeIndex;
    doc["g0Action"] = static_cast<uint8_t>(personalityConfig.g0Action);

    File file = SPIFFS.open(PERSONALITY_FILE, FILE_WRITE);
    if (file) {
        serializeJsonPretty(doc, file);
        file.close();
        Serial.printf("[CONFIG] Saved personality to SPIFFS (sound: %s)\n",
                      personalityConfig.soundEnabled ? "ON" : "OFF");
    } else {
        Serial.println("[CONFIG] Failed to save personality to SPIFFS");
    }
}

bool Config::save() {
    JsonDocument doc;

    // GPS config
    doc["gps"]["enabled"] = gpsConfig.enabled;
    doc["gps"]["gpsSource"] = static_cast<uint8_t>(gpsConfig.source);
    doc["gps"]["baudRate"] = gpsConfig.baudRate;
    doc["gps"]["updateInterval"] = gpsConfig.updateInterval;
    doc["gps"]["sleepTimeMs"] = gpsConfig.sleepTimeMs;
    doc["gps"]["powerSave"] = gpsConfig.powerSave;
    doc["gps"]["timezoneOffset"] = gpsConfig.timezoneOffset;

    // ML config
    doc["ml"]["enabled"] = mlConfig.enabled;
    doc["ml"]["collectionMode"] = static_cast<uint8_t>(mlConfig.collectionMode);
    doc["ml"]["modelPath"] = mlConfig.modelPath;
    doc["ml"]["confidenceThreshold"] = mlConfig.confidenceThreshold;
    doc["ml"]["rogueApThreshold"] = mlConfig.rogueApThreshold;
    doc["ml"]["vulnScorerThreshold"] = mlConfig.vulnScorerThreshold;
    doc["ml"]["autoUpdate"] = mlConfig.autoUpdate;
    doc["ml"]["updateUrl"] = mlConfig.updateUrl;

    // WiFi config
    doc["wifi"]["channelHopInterval"] = wifiConfig.channelHopInterval;
    doc["wifi"]["lockTime"] = wifiConfig.lockTime;
    doc["wifi"]["enableDeauth"] = wifiConfig.enableDeauth;
    doc["wifi"]["randomizeMAC"] = wifiConfig.randomizeMAC;
    doc["wifi"]["otaSSID"] = wifiConfig.otaSSID;
    doc["wifi"]["otaPassword"] = wifiConfig.otaPassword;
    doc["wifi"]["autoConnect"] = wifiConfig.autoConnect;
    doc["wifi"]["wpaSecKey"] = wifiConfig.wpaSecKey;
    doc["wifi"]["wigleApiName"] = wifiConfig.wigleApiName;
    doc["wifi"]["wigleApiToken"] = wifiConfig.wigleApiToken;

    // BLE config
    doc["ble"]["burstInterval"] = bleConfig.burstInterval;
    doc["ble"]["advDuration"] = bleConfig.advDuration;

    // Save to SD if available, otherwise SPIFFS
    fs::FS& cfgFS = sdAvailable ? (fs::FS&)SD : (fs::FS&)SPIFFS;

    File file = cfgFS.open(CONFIG_FILE, FILE_WRITE);
    if (!file) {
        return false;
    }

    size_t written = serializeJsonPretty(doc, file);
    file.close();

    return written > 0;
}

bool Config::createDefaultConfig() {
    gpsConfig = GPSConfig();
    mlConfig = MLConfig();
    wifiConfig = WiFiConfig();
    bleConfig = BLEConfig();
    return true;
}

bool Config::createDefaultPersonality() {
    strcpy(personalityConfig.name, "Porkchop");
    personalityConfig.mood = 50;
    personalityConfig.experience = 0;
    personalityConfig.curiosity = 0.7f;
    personalityConfig.aggression = 0.3f;
    personalityConfig.patience = 0.5f;
    personalityConfig.soundEnabled = true;
    personalityConfig.g0Action = G0Action::SCREEN_TOGGLE;
    return true;
}

void Config::setGPS(const GPSConfig& cfg) {
    gpsConfig = cfg;
    save();
}

void Config::setML(const MLConfig& cfg) {
    mlConfig = cfg;
    save();
}

void Config::setWiFi(const WiFiConfig& cfg) {
    wifiConfig = cfg;
    save();
}

void Config::setBLE(const BLEConfig& cfg) {
    bleConfig = cfg;
    save();
}

void Config::setPersonality(const PersonalityConfig& cfg) {
    personalityConfig = cfg;
    savePersonalityToSPIFFS();
}

bool Config::loadWpaSecKeyFromFile() {
    const char* keyFile = "/wpasec_key.txt";

    if (!sdAvailable || !SD.exists(keyFile)) {
        return false;
    }

    File f = SD.open(keyFile, FILE_READ);
    if (!f) {
        Serial.println("[CONFIG] Failed to open wpasec_key.txt");
        return false;
    }

    String key = f.readStringUntil('\n');
    key.trim();
    f.close();

    if (key.length() != 32) {
        Serial.printf("[CONFIG] Invalid WPA-SEC key length: %d (expected 32)\n", key.length());
        return false;
    }

    for (int i = 0; i < 32; i++) {
        char c = key.charAt(i);
        if (!isxdigit(c)) {
            Serial.printf("[CONFIG] Invalid hex char in WPA-SEC key at position %d\n", i);
            return false;
        }
    }

    wifiConfig.wpaSecKey = key;
    save();

    if (SD.remove(keyFile)) {
        Serial.println("[CONFIG] Deleted wpasec_key.txt after import");
        SDLog::log("CFG", "WPA-SEC key imported from file");
    } else {
        Serial.println("[CONFIG] Warning: Could not delete wpasec_key.txt");
    }

    return true;
}

bool Config::loadWigleKeyFromFile() {
    const char* keyFile = "/wigle_key.txt";

    if (!sdAvailable || !SD.exists(keyFile)) {
        return false;
    }

    File f = SD.open(keyFile, FILE_READ);
    if (!f) {
        Serial.println("[CONFIG] Failed to open wigle_key.txt");
        return false;
    }

    String content = f.readStringUntil('\n');
    content.trim();
    f.close();

    int colonPos = content.indexOf(':');
    if (colonPos < 1) {
        Serial.println("[CONFIG] Invalid WiGLE key format (expected name:token)");
        return false;
    }

    String apiName = content.substring(0, colonPos);
    String apiToken = content.substring(colonPos + 1);

    apiName.trim();
    apiToken.trim();

    if (apiName.isEmpty() || apiToken.isEmpty()) {
        Serial.println("[CONFIG] WiGLE API name or token is empty");
        return false;
    }

    wifiConfig.wigleApiName = apiName;
    wifiConfig.wigleApiToken = apiToken;
    save();

    if (SD.remove(keyFile)) {
        Serial.println("[CONFIG] Deleted wigle_key.txt after import");
        SDLog::log("CFG", "WiGLE API keys imported from file");
    } else {
        Serial.println("[CONFIG] Warning: Could not delete wigle_key.txt");
    }

    return true;
}
