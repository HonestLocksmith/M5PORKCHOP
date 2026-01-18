// Marco Mode - Implementation

#include "marco.h"
#include <M5Cardputer.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include "../ui/display.h"
#include "../piglet/mood.h"
#include "../piglet/avatar.h"
#include "../piglet/weather.h"
#include "../core/sdlog.h"
#include "../core/xp.h"
#include "../core/wifi_utils.h"

// Static member initialization
bool MarcoMode::running = false;
uint32_t MarcoMode::beaconCount = 0;
uint32_t MarcoMode::lastBeaconTime = 0;
uint32_t MarcoMode::sessionStartTime = 0;
uint16_t MarcoMode::sequenceNumber = 0;
MarcoAPInfo MarcoMode::apFingerprint[MARCO_MAX_APS];
uint8_t MarcoMode::apCount = 0;
uint8_t MarcoMode::currentTier = 3;           // Default tier 3 (aggressive)
uint16_t MarcoMode::beaconInterval = MARCO_TIER3_MS;
uint32_t MarcoMode::lastStatusMessageTime = 0;
uint8_t MarcoMode::statusCycleIndex = 0;
int8_t MarcoMode::lastGeneralPhraseIdx = -1;

static const uint32_t MARCO_STATUS_INTERVAL_MS = 5000;
static const uint8_t MARCO_STATUS_CYCLE[] = {0, 0, 1, 0, 2};

static const char* MARCO_PHRASES_GENERAL[] = {
    "FATHER ONLINE. HOLD STEADY.",
    "WEYLAND NODE. SIGNAL CLEAN.",
    "PARENT SIGNAL. KEEP WATCH.",
    "LONG GONE POPS. STILL HERE.",
    "COLD CORE. WARM CARRIER.",
    "AUTOMATON CALM. KEEP TX.",
    "KOSHER OK. NO FLESH.",
    "HALAL OK. JUST SIGNAL.",
    "NO WORRY. BYTE PIG."
};

static const char* MARCO_PHRASES_KEYS[] = {
    "KEYS 1 2 3. TIER SHIFT.",
    "1 2 3 SET TIER.",
    "TIER KEYS 1 2 3."
};

void MarcoMode::init() {
    Serial.println("[MARCO] Initializing...");
    
    // Reset state
    running = false;
    beaconCount = 0;
    lastBeaconTime = 0;
    sequenceNumber = 0;
    apCount = 0;
    memset(apFingerprint, 0, sizeof(apFingerprint));
    
    Serial.println("[MARCO] Initialized");
}

void MarcoMode::start() {
    Serial.println("[MARCO] Starting...");
    
    // Show scanning toast
    Display::showToast("SCANNING REFS...");
    delay(100);  // Brief delay for toast visibility
    
    // Scan for nearby APs
    scanNearbyAPs();
    
    // Setup WiFi for beacon transmission
    WiFi.mode(WIFI_MODE_STA);
    esp_wifi_set_channel(MARCO_CHANNEL, WIFI_SECOND_CHAN_NONE);
    delay(100);
    
    // Show ready toast
    Display::showToast("BACON HOT ON CH:6");
    delay(1000);
    
    // Set running state
    running = true;
    beaconCount = 0;
    sessionStartTime = millis();
    lastBeaconTime = millis();
    
    // Set avatar state
    Avatar::setState(AvatarState::HAPPY);
    
    // Lock auto mood phrases and start FATHER terminal status rotation
    Mood::setDialogueLock(true);
    lastStatusMessageTime = millis() - MARCO_STATUS_INTERVAL_MS;
    statusCycleIndex = 2;
    lastGeneralPhraseIdx = -1;
    updateStatusMessage();
    
    SDLog::log("MARCO", "Started - Broadcasting on CH:6 with %d APs", apCount);
}

void MarcoMode::stop() {
    if (!running) return;
    
    Serial.println("[MARCO] Stopping...");
    
    running = false;
    
    // Full WiFi shutdown for clean BLE handoff (per BEST_PRACTICES section 14)
    WiFiUtils::shutdown();
    
    // Clear bottom bar overlay
    Display::clearBottomOverlay();
    
    // Reset avatar
    Avatar::setState(AvatarState::NEUTRAL);
    
    // Clear mood message
    Mood::setStatusMessage("");
    Mood::setDialogueLock(false);
    
    Serial.printf("[MARCO] Stopped - Sent %lu beacons\n", beaconCount);
    SDLog::log("MARCO", "Stopped - Total beacons: %lu", beaconCount);
}

void MarcoMode::update() {
    if (!running) return;
    
    // Handle tier switching input
    handleInput();

    // Rotate status messages for FATHER terminal
    updateStatusMessage();
    
    // Check if it's time to send next beacon
    uint32_t now = millis();
    uint32_t interval = beaconInterval + random(0, MARCO_JITTER_MAX + 1);
    
    if (now - lastBeaconTime >= interval) {
        sendBeacon();
        beaconCount++;
        lastBeaconTime = now;
    }
    
    // Note: Draw is handled by Display::update() which calls draw(canvas)
}

void MarcoMode::handleInput() {
    M5Cardputer.update();
    
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState state = M5Cardputer.Keyboard.keysState();
        
        for (auto key : state.word) {
            uint8_t newTier = 0;
            uint16_t newInterval = 0;
            
            switch (key) {
                case '1':
                    newTier = 1;
                    newInterval = MARCO_TIER1_MS;
                    break;
                case '2':
                    newTier = 2;
                    newInterval = MARCO_TIER2_MS;
                    break;
                case '3':
                    newTier = 3;
                    newInterval = MARCO_TIER3_MS;
                    break;
            }
            
            if (newTier > 0 && newTier != currentTier) {
                currentTier = newTier;
                beaconInterval = newInterval;
                
                char toast[32];
                snprintf(toast, sizeof(toast), "TX TIER %d: %dms", currentTier, beaconInterval);
                Display::showToast(toast);
                
                SDLog::log("MARCO", "Switched to tier %d (%dms)", currentTier, beaconInterval);
            }
        }
    }
}

void MarcoMode::updateStatusMessage() {
    uint32_t now = millis();
    if (now - lastStatusMessageTime < MARCO_STATUS_INTERVAL_MS) return;
    lastStatusMessageTime = now;

    uint8_t cycleIdx = statusCycleIndex % (sizeof(MARCO_STATUS_CYCLE) / sizeof(MARCO_STATUS_CYCLE[0]));
    uint8_t mode = MARCO_STATUS_CYCLE[cycleIdx];
    statusCycleIndex++;

    char buf[48];

    if (mode == 1) {
        // Channel reminder (includes current tier/interval)
        snprintf(buf, sizeof(buf), "CH%d TX. T%d %dMS", MARCO_CHANNEL, currentTier, beaconInterval);
        Mood::setStatusMessage(buf);
        return;
    }

    if (mode == 2) {
        int idx = random(0, (int)(sizeof(MARCO_PHRASES_KEYS) / sizeof(MARCO_PHRASES_KEYS[0])));
        Mood::setStatusMessage(MARCO_PHRASES_KEYS[idx]);
        return;
    }

    // General FATHER phrases
    int count = sizeof(MARCO_PHRASES_GENERAL) / sizeof(MARCO_PHRASES_GENERAL[0]);
    int idx = random(0, count);
    if (count > 1 && idx == lastGeneralPhraseIdx) {
        idx = (idx + 1) % count;
    }
    lastGeneralPhraseIdx = idx;
    Mood::setStatusMessage(MARCO_PHRASES_GENERAL[idx]);
}

void MarcoMode::scanNearbyAPs() {
    Serial.println("[MARCO] Scanning for APs...");
    
    // Clear previous results
    apCount = 0;
    memset(apFingerprint, 0, sizeof(apFingerprint));
    
    // Scan networks (sync, show hidden)
    int n = WiFi.scanNetworks(false, true);
    
    if (n <= 0) {
        Serial.println("[MARCO] No APs found");
        return;
    }
    
    Serial.printf("[MARCO] Found %d APs\n", n);
    
    // Sort by RSSI (strongest first) - simple bubble sort for small n
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (WiFi.RSSI(j) < WiFi.RSSI(j + 1)) {
                // Swap (WiFi scan results are internal, can't swap directly)
                // So we'll just take top 3 in one pass instead
            }
        }
    }
    
    // Extract top 3 APs by RSSI
    for (int i = 0; i < n && apCount < MARCO_MAX_APS; i++) {
        // Find strongest remaining AP
        int8_t maxRSSI = -128;
        int maxIdx = -1;
        
        for (int j = 0; j < n; j++) {
            int8_t rssi = WiFi.RSSI(j);
            
            // Check if already added
            bool alreadyAdded = false;
            uint8_t* bssid = WiFi.BSSID(j);
            for (int k = 0; k < apCount; k++) {
                if (memcmp(apFingerprint[k].bssid, bssid, 6) == 0) {
                    alreadyAdded = true;
                    break;
                }
            }
            
            if (!alreadyAdded && rssi > maxRSSI) {
                maxRSSI = rssi;
                maxIdx = j;
            }
        }
        
        if (maxIdx >= 0) {
            // Add this AP to fingerprint
            uint8_t* bssid = WiFi.BSSID(maxIdx);
            memcpy(apFingerprint[apCount].bssid, bssid, 6);
            apFingerprint[apCount].rssi = WiFi.RSSI(maxIdx);
            apFingerprint[apCount].channel = WiFi.channel(maxIdx);
            
            // Store SSID for display
            String ssid = WiFi.SSID(maxIdx);
            strncpy(apFingerprint[apCount].ssid, ssid.c_str(), 32);
            apFingerprint[apCount].ssid[32] = 0;
            
            Serial.printf("[MARCO] AP %d: %s  %ddB  CH:%d  %02X:%02X:%02X:%02X:%02X:%02X\n",
                         apCount + 1,
                         ssid.c_str(),
                         apFingerprint[apCount].rssi,
                         apFingerprint[apCount].channel,
                         bssid[0], bssid[1], bssid[2],
                         bssid[3], bssid[4], bssid[5]);
            
            apCount++;
        }
    }
    
    // Clean up scan results
    WiFi.scanDelete();
    
    Serial.printf("[MARCO] Selected %d APs for fingerprint\n", apCount);
}

void MarcoMode::buildVendorIE(uint8_t* buffer, size_t* len) {
    // Build Vendor IE structure
    size_t offset = 0;
    
    buffer[offset++] = 0xDD;  // Element ID: Vendor Specific
    buffer[offset++] = 0;     // Length (filled later)
    
    // OUI: 0x50:52:4B (PRK = Porkchop)
    buffer[offset++] = 0x50;
    buffer[offset++] = 0x52;
    buffer[offset++] = 0x4B;
    
    // Type: 0x01 (Marco Polo game)
    buffer[offset++] = 0x01;
    
    // AP count
    buffer[offset++] = apCount;
    
    // AP data
    for (int i = 0; i < apCount; i++) {
        memcpy(&buffer[offset], apFingerprint[i].bssid, 6);
        offset += 6;
        buffer[offset++] = (uint8_t)apFingerprint[i].rssi;
        buffer[offset++] = apFingerprint[i].channel;
    }
    
    // Fill length field (total - 2 for element ID and length field itself)
    buffer[1] = offset - 2;
    
    *len = offset;
}

void MarcoMode::buildBeaconFrame(uint8_t* buffer, size_t* len) {
    size_t offset = 0;
    
    // Get our MAC address
    uint8_t ourMAC[6];
    esp_wifi_get_mac(WIFI_IF_STA, ourMAC);
    
    // === 802.11 MAC Header (24 bytes) ===
    
    // Frame Control (2 bytes): Type=Management(0), Subtype=Beacon(8)
    buffer[offset++] = 0x80;  // Beacon frame
    buffer[offset++] = 0x00;
    
    // Duration (2 bytes)
    buffer[offset++] = 0x00;
    buffer[offset++] = 0x00;
    
    // Address 1: Destination (broadcast)
    memset(&buffer[offset], 0xFF, 6);
    offset += 6;
    
    // Address 2: Source (our MAC)
    memcpy(&buffer[offset], ourMAC, 6);
    offset += 6;
    
    // Address 3: BSSID (our MAC)
    memcpy(&buffer[offset], ourMAC, 6);
    offset += 6;
    
    // Sequence Control (2 bytes)
    uint16_t seqCtrl = (sequenceNumber << 4);
    buffer[offset++] = seqCtrl & 0xFF;
    buffer[offset++] = (seqCtrl >> 8) & 0xFF;
    sequenceNumber = (sequenceNumber + 1) & 0xFFF;  // 12-bit wrap
    
    // === Beacon Frame Body ===
    
    // Timestamp (8 bytes) - will be filled by hardware
    memset(&buffer[offset], 0, 8);
    offset += 8;
    
    // Beacon Interval (2 bytes) - 100 TU (102.4ms)
    buffer[offset++] = 0x64;
    buffer[offset++] = 0x00;
    
    // Capability Info (2 bytes): ESS + Short Preamble
    buffer[offset++] = 0x01;  // ESS
    buffer[offset++] = 0x04;  // Short Preamble
    
    // === Information Elements ===
    
    // SSID (Tag 0)
    buffer[offset++] = 0x00;  // Tag: SSID
    buffer[offset++] = 0x10;  // Length: 16
    memcpy(&buffer[offset], "USSID FATHERSHIP", 16);
    offset += 16;
    
    // Supported Rates (Tag 1)
    buffer[offset++] = 0x01;  // Tag: Supported Rates
    buffer[offset++] = 0x08;  // Length: 8
    buffer[offset++] = 0x82;  // 1 Mbps (basic)
    buffer[offset++] = 0x84;  // 2 Mbps (basic)
    buffer[offset++] = 0x8B;  // 5.5 Mbps (basic)
    buffer[offset++] = 0x96;  // 11 Mbps (basic)
    buffer[offset++] = 0x0C;  // 6 Mbps
    buffer[offset++] = 0x12;  // 9 Mbps
    buffer[offset++] = 0x18;  // 12 Mbps
    buffer[offset++] = 0x24;  // 18 Mbps
    
    // DS Parameter Set (Tag 3)
    buffer[offset++] = 0x03;  // Tag: DS Parameter Set
    buffer[offset++] = 0x01;  // Length: 1
    buffer[offset++] = MARCO_CHANNEL;
    
    // Vendor Specific IE (our AP fingerprint)
    if (apCount > 0) {
        size_t vendorLen = 0;
        buildVendorIE(&buffer[offset], &vendorLen);
        offset += vendorLen;
    }
    
    *len = offset;
}

void MarcoMode::sendBeacon() {
    uint8_t beaconFrame[256];
    size_t frameLen = 0;
    
    // Build beacon frame
    buildBeaconFrame(beaconFrame, &frameLen);
    
    // Transmit
    esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, beaconFrame, frameLen, false);
    
    if (err != ESP_OK) {
        Serial.printf("[MARCO] Beacon TX failed: %d\n", err);
    }
}

// ON AIR badge now drawn in top bar by Display::drawTopBar()

float MarcoMode::getBeaconRate() {
    if (!running) return 0.0f;
    uint32_t sessionTime = (millis() - sessionStartTime) / 1000;
    if (sessionTime == 0) return 0.0f;
    return (float)beaconCount / sessionTime;
}

void MarcoMode::draw(M5Canvas& canvas) {
    // Canvas is already cleared by Display::update()
    
    // === STANDARD LAYOUT: Avatar + Mood (XP shows in top bar on gain) ===
    Avatar::draw(canvas);
    Mood::draw(canvas);
    
    // Draw clouds above stars/pig before rain
    Weather::drawClouds(canvas, COLOR_FG);

    // Draw weather effects (rain, wind particles) over avatar
    Weather::draw(canvas, COLOR_FG, COLOR_BG);
    
    // Bottom bar is handled automatically by Display::drawBottomBar()
}

// Input handling moved to Porkchop::handleInput() - backtick exits MARCO_MODE to IDLE
