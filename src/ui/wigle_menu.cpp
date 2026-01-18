// WiGLE Menu - View and upload wardriving files to wigle.net

#include "wigle_menu.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <string.h>
#include "display.h"
#include "../web/wigle.h"
#include "../core/config.h"

// For heap statistics logging
#include <esp_heap_caps.h>

// Static member initialization
std::vector<WigleFileInfo> WigleMenu::files;
uint8_t WigleMenu::selectedIndex = 0;
uint8_t WigleMenu::scrollOffset = 0;
bool WigleMenu::active = false;
bool WigleMenu::keyWasPressed = false;
bool WigleMenu::detailViewActive = false;
bool WigleMenu::nukeConfirmActive = false;
bool WigleMenu::connectingWiFi = false;
bool WigleMenu::uploadingFile = false;

TaskHandle_t WigleMenu::uploadTaskHandle = nullptr;
volatile bool WigleMenu::uploadTaskDone = false;
volatile bool WigleMenu::uploadTaskSuccess = false;
uint8_t WigleMenu::uploadTaskIndex = 0;
char WigleMenu::uploadTaskResultMsg[64] = {0};

static void formatDisplayName(const String& filename, char* out, size_t len, size_t maxChars,
                              const char* ellipsis, bool stripDecorators) {
    if (!out || len == 0) return;
    out[0] = '\0';
    if (len == 0) return;

    const char* name = filename.c_str();
    size_t total = strlen(name);
    size_t start = 0;
    size_t end = total;

    if (stripDecorators) {
        if (total >= 7 && strncmp(name, "warhog_", 7) == 0) start = 7;
        const char* suffix = ".wigle.csv";
        const size_t suffixLen = 10;
        if (total >= suffixLen && strcmp(name + total - suffixLen, suffix) == 0) {
            end = total - suffixLen;
        }
    }

    if (end < start) end = start;
    size_t avail = end - start;
    if (avail == 0) return;

    size_t limit = maxChars;
    if (limit >= len) limit = len - 1;
    if (limit == 0) return;

    size_t ellLen = (ellipsis ? strlen(ellipsis) : 0);
    bool truncated = avail > limit;
    size_t copyLen = avail;
    if (truncated) {
        copyLen = (limit > ellLen) ? (limit - ellLen) : limit;
    }
    if (copyLen >= len) copyLen = len - 1;
    memcpy(out, name + start, copyLen);
    if (truncated && ellipsis && ellLen > 0 && (copyLen + ellLen) < len) {
        memcpy(out + copyLen, ellipsis, ellLen);
        out[copyLen + ellLen] = '\0';
    } else {
        out[copyLen] = '\0';
    }
}

void WigleMenu::uploadTaskFn(void* pv) {
    UploadTaskCtx* ctx = reinterpret_cast<UploadTaskCtx*>(pv);

    // Run WiFi + TLS from a dedicated task with a large stack.
    // This avoids stack canary panics from mbedTLS handshake inside loopTask.

    bool weConnected = false;
    if (!WiGLE::isConnected()) {
        connectingWiFi = true;
        if (!WiGLE::connect()) {
            strncpy(uploadTaskResultMsg, WiGLE::getLastError(), sizeof(uploadTaskResultMsg) - 1);
            uploadTaskResultMsg[sizeof(uploadTaskResultMsg) - 1] = '\0';
            uploadTaskSuccess = false;
            uploadTaskDone = true;
            connectingWiFi = false;
            delete ctx;
            vTaskDelete(nullptr);
            return;
        }
        weConnected = true;
        connectingWiFi = false;
    }

    uploadingFile = true;
    bool ok = WiGLE::uploadFile(ctx->fullPath);
    uploadingFile = false;

    if (weConnected) {
        WiGLE::disconnect();
    }

    uploadTaskIndex = ctx->index;
    uploadTaskSuccess = ok;
    if (ok) {
        strncpy(uploadTaskResultMsg, "UPLOAD OK!", sizeof(uploadTaskResultMsg) - 1);
    } else {
        strncpy(uploadTaskResultMsg, WiGLE::getLastError(), sizeof(uploadTaskResultMsg) - 1);
    }
    uploadTaskResultMsg[sizeof(uploadTaskResultMsg) - 1] = '\0';

    uploadTaskDone = true;

    delete ctx;
    vTaskDelete(nullptr);
}

void WigleMenu::init() {
    files.clear();
    selectedIndex = 0;
    scrollOffset = 0;
}

void WigleMenu::show() {
    active = true;
    selectedIndex = 0;
    scrollOffset = 0;
    detailViewActive = false;
    nukeConfirmActive = false;
    connectingWiFi = false;
    uploadingFile = false;
    keyWasPressed = true;  // Ignore enter that brought us here
    scanFiles();
}

void WigleMenu::hide() {
    active = false;
    detailViewActive = false;
    files.clear();  // Release memory when not in menu
    files.shrink_to_fit();
}

void WigleMenu::scanFiles() {
    files.clear();
    
    if (!Config::isSDAvailable()) {
        Serial.println("[WIGLE_MENU] SD card not available");
        return;
    }
    
    // Scan /wardriving/ directory for .wigle.csv files
    File dir = SD.open("/wardriving");
    if (!dir || !dir.isDirectory()) {
        Serial.println("[WIGLE_MENU] /wardriving directory not found");
        return;
    }
    
    while (File entry = dir.openNextFile()) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            // Only show WiGLE format files (*.wigle.csv)
            if (name.endsWith(".wigle.csv")) {
                WigleFileInfo info;
                info.filename = name;
                info.fullPath = String("/wardriving/") + name;
                info.fileSize = entry.size();
                // Estimate network count: ~150 bytes per line after header
                info.networkCount = info.fileSize > 300 ? (info.fileSize - 300) / 150 : 0;
                
                // Check upload status
                info.status = WiGLE::isUploaded(info.fullPath.c_str()) ? 
                    WigleFileStatus::UPLOADED : WigleFileStatus::LOCAL;
                
                files.push_back(info);
                
                // Cap at 50 files to prevent memory issues
                if (files.size() >= 50) break;
            }
        }
        entry.close();
    }
    dir.close();
    
    // Sort by filename (newest first - filenames include timestamp)
    std::sort(files.begin(), files.end(), [](const WigleFileInfo& a, const WigleFileInfo& b) {
        return a.filename > b.filename;
    });
    
    Serial.printf("[WIGLE_MENU] Found %d WiGLE files\n", files.size());
}

void WigleMenu::handleInput() {
    bool anyPressed = M5Cardputer.Keyboard.isPressed();
    
    if (!anyPressed) {
        keyWasPressed = false;
        return;
    }
    
    if (keyWasPressed) return;
    keyWasPressed = true;
    
    auto keys = M5Cardputer.Keyboard.keysState();
    
    // Handle detail view input - U uploads, any other key closes
    if (detailViewActive) {
        if (M5Cardputer.Keyboard.isKeyPressed('u') || M5Cardputer.Keyboard.isKeyPressed('U')) {
            detailViewActive = false;
            if (!files.empty() && selectedIndex < files.size()) {
                uploadSelected();
            }
            return;
        }
        detailViewActive = false;
        return;
    }
    
    // Handle nuke confirmation modal
    if (nukeConfirmActive) {
        if (M5Cardputer.Keyboard.isKeyPressed('y') || M5Cardputer.Keyboard.isKeyPressed('Y')) {
            nukeTrack();
            nukeConfirmActive = false;
            Display::clearBottomOverlay();
            return;
        }
        if (M5Cardputer.Keyboard.isKeyPressed('n') || M5Cardputer.Keyboard.isKeyPressed('N') ||
            M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
            nukeConfirmActive = false;  // Cancel
            Display::clearBottomOverlay();
            return;
        }
        return;  // Ignore other keys when modal active
    }
    
    // Handle connecting/uploading states - ignore input
    if (connectingWiFi || uploadingFile) {
        return;
    }
    
    // Backspace - go back
    if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE)) {
        hide();
        return;
    }
    
    // Navigation with ; (prev) and . (next)
    if (M5Cardputer.Keyboard.isKeyPressed(';')) {
        if (selectedIndex > 0) {
            selectedIndex--;
            if (selectedIndex < scrollOffset) {
                scrollOffset = selectedIndex;
            }
        }
    }
    
    if (M5Cardputer.Keyboard.isKeyPressed('.')) {
        if (!files.empty() && selectedIndex < files.size() - 1) {
            selectedIndex++;
            if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) {
                scrollOffset = selectedIndex - VISIBLE_ITEMS + 1;
            }
        }
    }
    
    // Enter - show detail view
    if (keys.enter && !files.empty()) {
        detailViewActive = true;
    }
    
    // U key - upload selected file
    if ((M5Cardputer.Keyboard.isKeyPressed('u') || M5Cardputer.Keyboard.isKeyPressed('U')) && !files.empty()) {
        uploadSelected();
    }
    
    // R key - refresh list
    if (M5Cardputer.Keyboard.isKeyPressed('r') || M5Cardputer.Keyboard.isKeyPressed('R')) {
        scanFiles();
        Display::setTopBarMessage("WIGLE REFRESHED", 3000);
    }
    
    // D key - nuke selected track
    if ((M5Cardputer.Keyboard.isKeyPressed('d') || M5Cardputer.Keyboard.isKeyPressed('D')) && !files.empty()) {
        if (selectedIndex < files.size()) {
            nukeConfirmActive = true;
            Display::setBottomOverlay("PERMANENT | NO UNDO");
        }
    }
}

void WigleMenu::uploadSelected() {
    if (files.empty() || selectedIndex >= files.size()) return;

    // Prevent starting multiple parallel uploads
    if (uploadTaskHandle != nullptr) {
        Display::setTopBarMessage("WIGLE BUSY", 3000);
        return;
    }
    // Guard: ensure enough contiguous heap for task stack (~8 KB)
    if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 12000) {
        Display::setTopBarMessage("LOW HEAP FOR WIGLE", 3000);
        return;
    }
    
    WigleFileInfo file = files[selectedIndex];
    
    // Check if already uploaded
    if (file.status == WigleFileStatus::UPLOADED) {
        Display::setTopBarMessage("ALREADY UPLOADED", 3000);
        return;
    }

    // Check for credentials
    if (!WiGLE::hasCredentials()) {
        Display::setTopBarMessage("NO WIGLE API KEY", 4000);
        return;
    }
    
    // Kick off upload in a dedicated FreeRTOS task with a larger stack.
    // TLS handshakes can easily overflow Arduino's loopTask stack.
    uploadTaskDone = false;
    uploadTaskSuccess = false;
    uploadTaskIndex = selectedIndex;
    uploadTaskResultMsg[0] = '\0';

    UploadTaskCtx* ctx = new UploadTaskCtx();
    strncpy(ctx->fullPath, file.fullPath.c_str(), sizeof(ctx->fullPath) - 1);
    ctx->fullPath[sizeof(ctx->fullPath) - 1] = '\0';
    ctx->index = selectedIndex;

    uploadingFile = true;
    Display::setTopBarMessage("WIGLE UP...", 0);

    // Pin to core 0 to keep Arduino loopTask responsive (loopTask usually runs on core 1).
    BaseType_t ok = xTaskCreatePinnedToCore(
        uploadTaskFn,
        "wigle_upload",
        6144,            // smaller stack to reduce heap use
        ctx,
        1,
        &uploadTaskHandle,
        0
    );
    if (ok != pdPASS) {
        uploadTaskHandle = nullptr;
        uploadingFile = false;
        delete ctx;
        Display::setTopBarMessage("WIGLE TASK FAIL", 4000);
    }
}

void WigleMenu::formatSize(char* out, size_t len, uint32_t bytes) {
    if (!out || len == 0) return;
    if (bytes < 1024) {
        snprintf(out, len, "%uB", (unsigned)bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(out, len, "%uKB", (unsigned)(bytes / 1024));
    } else {
        snprintf(out, len, "%uMB", (unsigned)(bytes / (1024 * 1024)));
    }
}

void WigleMenu::getSelectedInfo(char* out, size_t len) {
    if (!out || len == 0) return;
    if (files.empty() || selectedIndex >= files.size()) {
        snprintf(out, len, "D0PAM1N3 SH0P: [U] [R] [D]");
        return;
    }
    const WigleFileInfo& file = files[selectedIndex];
    const char* status = (file.status == WigleFileStatus::UPLOADED) ? "[OK]" : "[--]";
    char sizeBuf[12];
    formatSize(sizeBuf, sizeof(sizeBuf), file.fileSize);
    snprintf(out, len, "~%uNETS %s %s", (unsigned)file.networkCount, sizeBuf, status);
}

void WigleMenu::update() {
    if (!active) return;

    // Handle completion of background upload task
    if (uploadTaskHandle != nullptr && uploadTaskDone) {
        // Task self-deletes; clear handle and report result in the UI thread.
        uploadTaskHandle = nullptr;
        uploadingFile = false;
        connectingWiFi = false;

        if (uploadTaskSuccess) {
            if (!files.empty() && uploadTaskIndex < files.size()) {
                files[uploadTaskIndex].status = WigleFileStatus::UPLOADED;
            }
            Display::setTopBarMessage(uploadTaskResultMsg[0] ? uploadTaskResultMsg : "WIGLE UPLOAD OK", 4000);
        } else {
            Display::setTopBarMessage(uploadTaskResultMsg[0] ? uploadTaskResultMsg : "WIGLE UPLOAD FAIL", 4000);
        }

        // Refresh the list to reflect new status
        scanFiles();

        uploadTaskDone = false;
    }

    handleInput();
}

void WigleMenu::draw(M5Canvas& canvas) {
    if (!active) return;
    
    canvas.fillSprite(COLOR_BG);
    canvas.setTextColor(COLOR_FG);
    canvas.setTextSize(1);
    
    // Empty state
    if (files.empty()) {
        canvas.setCursor(4, 35);
        canvas.print("No WiGLE files found");
        canvas.setCursor(4, 50);
        canvas.print("Go wardriving first!");
        canvas.setCursor(4, 65);
        canvas.print("[W] for WARHOG mode.");
        return;
    }
    
    // File list (always drawn, modals overlay on top)
    int y = 2;
    int lineHeight = 18;
    
    for (uint8_t i = scrollOffset; i < files.size() && i < scrollOffset + VISIBLE_ITEMS; i++) {
        const WigleFileInfo& file = files[i];
        
        // Highlight selected
        if (i == selectedIndex) {
            canvas.fillRect(0, y - 1, canvas.width(), lineHeight, COLOR_FG);
            canvas.setTextColor(COLOR_BG);
        } else {
            canvas.setTextColor(COLOR_FG);
        }
        
        // Filename first (truncated) - extract just the date/time part
        char displayName[24];
        formatDisplayName(file.filename, displayName, sizeof(displayName), 15, "..", true);
        canvas.setCursor(4, y);
        canvas.print(displayName);
        
        // Status indicator (second column, matches LOOT menu)
        canvas.setCursor(105, y);
        if (file.status == WigleFileStatus::UPLOADED) {
            canvas.print("[OK]");
        } else {
            canvas.print("[--]");
        }
        
        // Network count and size
        canvas.setCursor(140, y);
        char sizeBuf[12];
        formatSize(sizeBuf, sizeof(sizeBuf), file.fileSize);
        canvas.printf("~%u %s", (unsigned)file.networkCount, sizeBuf);
        
        y += lineHeight;
    }
    
    // Scroll indicators
    if (scrollOffset > 0) {
        canvas.setCursor(canvas.width() - 10, 2);
        canvas.setTextColor(COLOR_FG);
        canvas.print("^");
    }
    if (scrollOffset + VISIBLE_ITEMS < files.size()) {
        canvas.setCursor(canvas.width() - 10, 2 + (VISIBLE_ITEMS - 1) * lineHeight);
        canvas.setTextColor(COLOR_FG);
        canvas.print("v");
    }
    
    // Draw modals on top of list (matching captures_menu pattern)
    if (nukeConfirmActive) {
        drawNukeConfirm(canvas);
    }
    
    if (detailViewActive) {
        drawDetailView(canvas);
    }
    
}

void WigleMenu::drawDetailView(M5Canvas& canvas) {
    const WigleFileInfo& file = files[selectedIndex];
    
    // Modal box dimensions - matches other confirmation dialogs
    const int boxW = 200;
    const int boxH = 75;
    const int boxX = (canvas.width() - boxW) / 2;
    const int boxY = (canvas.height() - boxH) / 2 - 5;
    
    // Black border then pink fill
    canvas.fillRoundRect(boxX - 2, boxY - 2, boxW + 4, boxH + 4, 8, COLOR_BG);
    canvas.fillRoundRect(boxX, boxY, boxW, boxH, 8, COLOR_FG);
    
    // Black text on pink
    canvas.setTextColor(COLOR_BG, COLOR_FG);
    canvas.setTextDatum(top_center);
    
    // Filename
    char displayName[32];
    formatDisplayName(file.filename, displayName, sizeof(displayName), 22, "...", false);
    canvas.drawString(displayName, boxX + boxW / 2, boxY + 8);
    
    // Stats
    char sizeBuf[12];
    formatSize(sizeBuf, sizeof(sizeBuf), file.fileSize);
    char statsBuf[64];
    snprintf(statsBuf, sizeof(statsBuf), "~%u networks, %s", (unsigned)file.networkCount, sizeBuf);
    canvas.drawString(statsBuf, boxX + boxW / 2, boxY + 24);
    
    // Status
    const char* statusText = (file.status == WigleFileStatus::UPLOADED) ? "UPLOADED" : "NOT UPLOADED";
    canvas.drawString(statusText, boxX + boxW / 2, boxY + 40);
    
    // Action hint
    canvas.drawString("[U]PLOAD  [ANY]CLOSE", boxX + boxW / 2, boxY + 56);
    
    canvas.setTextDatum(top_left);
}

void WigleMenu::drawConnecting(M5Canvas& canvas) {
    const int boxW = 160;
    const int boxH = 50;
    const int boxX = (canvas.width() - boxW) / 2;
    const int boxY = (canvas.height() - boxH) / 2 - 5;
    
    canvas.fillRoundRect(boxX - 2, boxY - 2, boxW + 4, boxH + 4, 8, COLOR_BG);
    canvas.fillRoundRect(boxX, boxY, boxW, boxH, 8, COLOR_FG);
    
    canvas.setTextColor(COLOR_BG, COLOR_FG);
    canvas.setTextDatum(top_center);
    
    if (connectingWiFi) {
        canvas.drawString("CONNECTING...", boxX + boxW / 2, boxY + 12);
    } else if (uploadingFile) {
        canvas.drawString("UPLOADING...", boxX + boxW / 2, boxY + 12);
    }
    
    canvas.drawString(WiGLE::getStatus(), boxX + boxW / 2, boxY + 30);
    
    canvas.setTextDatum(top_left);
}

void WigleMenu::drawNukeConfirm(M5Canvas& canvas) {
    if (files.empty() || selectedIndex >= files.size()) return;
    
    const WigleFileInfo& file = files[selectedIndex];
    
    // Modal box dimensions - matches other confirmation dialogs
    const int boxW = 200;
    const int boxH = 70;
    const int boxX = (canvas.width() - boxW) / 2;
    const int boxY = (canvas.height() - boxH) / 2 - 5;
    
    // Black border then pink fill
    canvas.fillRoundRect(boxX - 2, boxY - 2, boxW + 4, boxH + 4, 8, COLOR_BG);
    canvas.fillRoundRect(boxX, boxY, boxW, boxH, 8, COLOR_FG);
    
    // Black text on pink background
    canvas.setTextColor(COLOR_BG, COLOR_FG);
    canvas.setTextDatum(top_center);
    canvas.setTextSize(1);
    
    int centerX = canvas.width() / 2;
    
    // Truncate filename for display
    canvas.drawString("!! NUKE THE TRACK !!", centerX, boxY + 8);
    char displayName[32];
    formatDisplayName(file.filename, displayName, sizeof(displayName), 22, "...", false);
    canvas.drawString(displayName, centerX, boxY + 24);
    canvas.drawString("THIS KILLS THE FILE.", centerX, boxY + 38);
    canvas.drawString("[Y] DO IT  [N] ABORT", centerX, boxY + 54);
    
    canvas.setTextDatum(top_left);
}

void WigleMenu::nukeTrack() {
    if (files.empty() || selectedIndex >= files.size()) return;
    
    const WigleFileInfo& file = files[selectedIndex];
    
    Serial.printf("[WIGLE_MENU] Nuking track: %s\n", file.fullPath.c_str());
    
    // Delete the .wigle.csv file
    bool deleted = SD.remove(file.fullPath);
    
    // Also delete matching internal CSV if exists (same name without .wigle)
    String internalPath = file.fullPath;
    internalPath.replace(".wigle.csv", ".csv");
    if (SD.exists(internalPath)) {
        SD.remove(internalPath);
        Serial.printf("[WIGLE_MENU] Also nuked: %s\n", internalPath.c_str());
    }
    
    // Remove from uploaded tracking if present
    WiGLE::removeFromUploaded(file.fullPath.c_str());
    
    if (deleted) {
        Display::setTopBarMessage("TRACK NUKED!", 4000);
    } else {
        Display::setTopBarMessage("NUKE FAILED", 4000);
    }

    // Refresh the file list
    scanFiles();
    
    // Adjust selection if needed
    if (selectedIndex >= files.size() && selectedIndex > 0) {
        selectedIndex = files.size() - 1;
    }
    if (scrollOffset > selectedIndex) {
        scrollOffset = selectedIndex;
    }
}
