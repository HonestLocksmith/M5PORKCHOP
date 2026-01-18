// Crash Viewer Menu
#pragma once

#include <Arduino.h>
#include <vector>
#include <M5Unified.h>

class CrashViewer {
public:
    static void init();
    static void show();
    static void hide();
    static void update();
    static bool isActive() { return active; }
    static void draw(M5Canvas& canvas);
    static void getStatusLine(char* out, size_t len);
    
private:
    struct CrashEntry {
        String path;
        time_t timestamp = 0;
    };
    static bool active;
    static std::vector<CrashEntry> crashFiles;
    static std::vector<String> fileLines;
    static uint16_t listScroll;
    static uint16_t fileScroll;
    static uint16_t totalLines;
    static uint8_t selectedIndex;
    static bool fileViewActive;
    static bool nukeConfirmActive;
    static bool keyWasPressed;
    static String activeFile;
    
    static void scanCrashFiles();
    static void loadCrashFile(const String& path);
    static void drawList(M5Canvas& canvas);
    static void drawFile(M5Canvas& canvas);
    static void drawNukeConfirm(M5Canvas& canvas);
    static void nukeCrashFiles();
    static String getDisplayName(const String& path);
    static String formatTime(time_t t);
};
