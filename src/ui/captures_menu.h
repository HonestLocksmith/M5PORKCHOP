// Captures Menu - View saved handshake captures
#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#include <vector>

// FreeRTOS task helpers (ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// WPA-SEC status for display
enum class CaptureStatus {
    LOCAL,      // Not uploaded yet
    UPLOADED,   // Uploaded, waiting for crack
    CRACKED     // Password found!
};

struct CaptureInfo {
    String filename;
    String ssid;
    String bssid;
    uint32_t fileSize;
    time_t captureTime;  // File modification time
    bool isPMKID;        // true = .22000 PMKID, false = .pcap handshake
    CaptureStatus status; // WPA-SEC status
    String password;      // Cracked password (if status == CRACKED)
};

class CapturesMenu {
public:
    static void init();
    static void show();
    static void hide();
    static void update();
    static void draw(M5Canvas& canvas);
    static bool isActive() { return active; }
    static const char* getSelectedBSSID();
    static size_t getCount() { return captures.size(); }
    
private:
    static std::vector<CaptureInfo> captures;
    static uint8_t selectedIndex;
    static uint8_t scrollOffset;
    static bool active;
    static bool keyWasPressed;
    static bool nukeConfirmActive;  // Nuke confirmation modal
    static bool detailViewActive;   // Password detail view
    static bool connectingWiFi;     // WiFi connection in progress
    static bool uploadingFile;      // Upload in progress
    static bool refreshingResults;  // Fetching WPA-SEC results

    // WPA‑SEC worker task (runs TLS off the Arduino loopTask stack)
    enum class WpaTaskAction : uint8_t { NONE = 0, UPLOAD, REFRESH };
    static TaskHandle_t wpaTaskHandle;
    static volatile bool wpaTaskDone;
    static volatile bool wpaTaskSuccess;
    static volatile WpaTaskAction wpaTaskAction;
    static uint8_t wpaTaskIndex;  // which capture was uploaded
    static char wpaTaskResultMsg[64];

    struct WpaTaskCtx {
        WpaTaskAction action;
        char pcapPath[128];
        uint8_t index;
    };

    static void wpaTaskFn(void* pv);
    
    static const uint8_t VISIBLE_ITEMS = 5;
    
    static bool scanCaptures();  // Returns true if successful, false if SD access failed
    static void handleInput();
    static void drawNukeConfirm(M5Canvas& canvas);
    static void drawDetailView(M5Canvas& canvas);
    static void drawConnecting(M5Canvas& canvas);
    static void nukeLoot();
    static void updateWPASecStatus();
    static void uploadSelected();
    static void refreshResults();
    static void formatTime(char* out, size_t len, time_t t);
    static const size_t MAX_CAPTURES = 200;
};
