// Porkchop - ML-Enhanced Piglet Security Companion
// Main entry point
// Author: 0ct0

#include <M5Cardputer.h>
#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>              // <-- PATCH: init WiFi early (before heap fragmentation)
#include <esp_core_dump.h>
#include <esp_partition.h>
#include "core/porkchop.h"
#include "core/config.h"
#include "core/sdlog.h"
#include "ui/display.h"
#include "gps/gps.h"
#include "piglet/avatar.h"
#include "piglet/mood.h"
#include "modes/oink.h"
#include "modes/warhog.h"
#include "audio/sfx.h"

Porkchop porkchop;

// --- PATCH: Pre-init WiFi driver early to avoid later esp_wifi_init() failures
// Some reconnect flows (and some Arduino/M5 stacks) end up deinit/reinit WiFi later.
// If heap is fragmented by display sprites / big allocations, esp_wifi_init() may fail with:
//   "Expected to init 4 rx buffer, actual is X" and "wifiLowLevelInit(): esp_wifi_init 257"
static void preInitWiFiDriverEarly() {
    WiFi.persistent(false);

    // Force driver/buffers allocation while heap is still clean/contiguous
    WiFi.mode(WIFI_STA);

    // Keep driver ON (do NOT wifioff=true)
    WiFi.disconnect(true /* erase */, false /* wifioff */);

    // No modem sleep to reduce odd timing/latency during TLS + UI load
    WiFi.setSleep(false);

    delay(50);
}

static void exportCoreDumpToSD() {
    if (!Config::isSDAvailable()) {
        Serial.println("[COREDUMP] SD not available, skipping export");
        return;
    }

    esp_err_t check = esp_core_dump_image_check();
    if (check != ESP_OK) {
        return;  // No core dump to export
    }

    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_COREDUMP,
        nullptr
    );
    if (!part) {
        Serial.println("[COREDUMP] No coredump partition found");
        return;
    }

    size_t addr = 0;
    size_t size = 0;
    if (esp_core_dump_image_get(&addr, &size) != ESP_OK || size == 0) {
        Serial.println("[COREDUMP] No coredump image available");
        return;
    }

    size_t partOffset = 0;
    if (addr >= part->address) {
        partOffset = addr - part->address;
    }
    if (partOffset + size > part->size) {
        Serial.println("[COREDUMP] Image bounds exceed partition size");
        return;
    }

    if (!SD.exists("/crash")) {
        SD.mkdir("/crash");
    }

    uint32_t stamp = millis();
    char dumpPath[64];
    snprintf(dumpPath, sizeof(dumpPath), "/crash/coredump_%lu.elf",
             static_cast<unsigned long>(stamp));

    File out = SD.open(dumpPath, FILE_WRITE);
    if (!out) {
        Serial.println("[COREDUMP] Failed to open output file");
        return;
    }

    const size_t kChunkSize = 512;
    uint8_t buf[kChunkSize];
    size_t remaining = size;
    size_t offset = 0;

    while (remaining > 0) {
        size_t toRead = remaining > kChunkSize ? kChunkSize : remaining;
        if (esp_partition_read(part, partOffset + offset, buf, toRead) != ESP_OK) {
            Serial.println("[COREDUMP] Read failed");
            break;
        }
        out.write(buf, toRead);
        remaining -= toRead;
        offset += toRead;
    }
    out.close();

    if (remaining == 0) {
        Serial.printf("[COREDUMP] Exported %u bytes to %s\n",
                      static_cast<unsigned int>(size), dumpPath);
#if CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF
        esp_core_dump_summary_t summary;
        if (esp_core_dump_get_summary(&summary) == ESP_OK) {
            char sumPath[64];
            snprintf(sumPath, sizeof(sumPath), "/crash/coredump_%lu.txt",
                     static_cast<unsigned long>(stamp));
            File sum = SD.open(sumPath, FILE_WRITE);
            if (sum) {
                sum.printf("task=%s\n", summary.exc_task);
                sum.printf("pc=0x%08lx\n", static_cast<unsigned long>(summary.exc_pc));
                sum.printf("bt_depth=%u\n", summary.exc_bt_info.depth);
                sum.printf("bt_corrupted=%u\n", summary.exc_bt_info.corrupted ? 1 : 0);
                for (uint32_t i = 0; i < summary.exc_bt_info.depth; i++) {
                    sum.printf("bt%lu=0x%08lx\n",
                               static_cast<unsigned long>(i),
                               static_cast<unsigned long>(summary.exc_bt_info.bt[i]));
                }
                sum.printf("exc_cause=0x%08lx\n", static_cast<unsigned long>(summary.ex_info.exc_cause));
                sum.printf("exc_vaddr=0x%08lx\n", static_cast<unsigned long>(summary.ex_info.exc_vaddr));
                sum.printf("elf_sha256=%s\n", summary.app_elf_sha256);
                sum.close();
            }
        }
#endif
        esp_core_dump_image_erase();
    } else {
        Serial.println("[COREDUMP] Export incomplete, keeping core dump in flash");
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\n=== PORKCHOP STARTING ===");

    // Init M5Cardputer hardware
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);   // enableKeyboard = true

    // Configure G0 button (GPIO0) as input with pullup
    pinMode(0, INPUT_PULLUP);

    // --- PATCH: Initialize WiFi driver BEFORE config/display allocate big chunks
    // This dramatically reduces "esp_wifi_init 257" failures on reconnect later.
    preInitWiFiDriverEarly();

    // Load configuration from SD
    if (!Config::init()) {
        Serial.println("[MAIN] Config init failed, using defaults");
    }

    // Init SD logging (will be enabled via settings if user wants)
    SDLog::init();

    // Export any stored core dump to SD (if present)
    exportCoreDumpToSD();

    // Init display system
    Display::init();

    // Init audio early so boot sound plays
    SFX::init();

    // Show boot splash (3 screens: OINK OINK, MY NAME IS, PORKCHOP)
    Display::showBootSplash();

    // Apply saved brightness
    M5.Display.setBrightness(Config::personality().brightness * 255 / 100);

    // Initialize piglet personality
    Avatar::init();
    Mood::init();

    // Initialize GPS (if enabled)
    if (Config::gps().enabled) {
        // Hardware detection: warn if Cap LoRa GPS selected on non-ADV hardware
        if (Config::gps().source == GPSSource::CAP_LORA) {
            auto board = M5.getBoard();
            if (board != m5::board_t::board_M5CardputerADV) {
                Serial.println("[GPS] WARNING: Cap LoRa868 GPS selected but hardware is not Cardputer ADV!");
                Serial.println("[GPS] Cap LoRa868 requires Cardputer ADV EXT bus. Check config.");
            }
        }
        GPS::init(Config::gps().rxPin, Config::gps().txPin, Config::gps().baudRate);
    }

    // Initialize ML subsystem
    //FeatureExtractor::init();
    //MLInference::init();

    // Initialize modes
    OinkMode::init();
    WarhogMode::init();
    porkchop.init();

    Serial.println("=== PORKCHOP READY ===");
    Serial.printf("Piglet: %s\n", Config::personality().name);
}

void loop() {
    M5Cardputer.update();

    // Update GPS
    if (Config::gps().enabled) {
        GPS::update();
    }

    // Update mood system
    Mood::update();

    // Update main controller (handles modes, input, state)
    porkchop.update();

    // Update ML (process any pending callbacks)
    //MLInference::update();

    // Update display
    Display::update();

    // Slower update rate for smoother animation
}
