// Log Viewer Menu
#pragma once

#include <Arduino.h>
#include <vector>

class LogViewer {
public:
    static void init();
    static void show();
    static void hide();
    static void update();
    static bool isActive() { return active; }
    
private:
    static bool active;
    static std::vector<String> logLines;
    static uint16_t scrollOffset;
    static uint16_t totalLines;
    static bool keyWasPressed;
    
    static void loadLogFile();
    static void render();
    static String findLatestLogFile();
};
