// Marco Mode - Hide and Seek Beacon Broadcaster
// Broadcasts WiFi beacons on CH:6 with AP fingerprint (Vendor IE)
#pragma once

#include <Arduino.h>
#include <esp_wifi.h>
#include <M5Unified.h>

// Constants
#define MARCO_CHANNEL 6
#define MARCO_JITTER_MAX 50        // ms random jitter (0-50)
#define MARCO_MAX_APS 3            // Top 3 APs for fingerprint

// Beacon TX tiers (key 1/2/3)
#define MARCO_TIER1_MS 200         // Conservative
#define MARCO_TIER2_MS 100         // Balanced  
#define MARCO_TIER3_MS 50          // Aggressive

// AP fingerprint structure (what we broadcast)
struct MarcoAPInfo {
    uint8_t bssid[6];
    int8_t rssi;        // RSSI as seen by Porkchop
    uint8_t channel;
    char ssid[33];      // AP name (for display)
};

// Vendor IE structure for beacon (OUI: 0x50:52:4B = "PRK")
struct MarcoVendorIE {
    uint8_t elementId;      // 0xDD (Vendor Specific)
    uint8_t length;         // 5 + (apCount * 8)
    uint8_t oui[3];         // {0x50, 0x52, 0x4B}
    uint8_t type;           // 0x01 (Marco Polo game)
    uint8_t apCount;        // 1-3
    MarcoAPInfo aps[MARCO_MAX_APS];
} __attribute__((packed));

class MarcoMode {
public:
    static void init();
    static void start();
    static void stop();
    static void update();
    static void draw(M5Canvas& canvas);  // Called by Display::update()
    static bool isRunning() { return running; }
    
    // Getters for Display bottom bar
    static uint32_t getBeaconCount() { return beaconCount; }
    static uint32_t getSessionTime() { return running ? (millis() - sessionStartTime) / 1000 : 0; }
    static float getBeaconRate();  // Beacons per second
    static uint8_t getAPCount() { return apCount; }
    static const MarcoAPInfo* getAPList() { return apFingerprint; }
    static uint8_t getCurrentTier() { return currentTier; }
    static uint16_t getCurrentInterval() { return beaconInterval; }
    
private:
    static bool running;
    static uint32_t beaconCount;
    static uint32_t lastBeaconTime;
    static uint32_t sessionStartTime;
    static uint16_t sequenceNumber;
    static MarcoAPInfo apFingerprint[MARCO_MAX_APS];
    static uint8_t apCount;
    static uint8_t currentTier;      // 1-3
    static uint16_t beaconInterval;  // Current interval in ms
    static uint32_t lastStatusMessageTime;
    static uint8_t statusCycleIndex;
    static int8_t lastGeneralPhraseIdx;
    
    // Internal methods
    static void scanNearbyAPs();
    static void buildBeaconFrame(uint8_t* buffer, size_t* len);
    static void buildVendorIE(uint8_t* buffer, size_t* len);
    static void sendBeacon();
    static void handleInput();
    static void updateStatusMessage();
};

