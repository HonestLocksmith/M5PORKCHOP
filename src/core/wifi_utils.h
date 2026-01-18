#pragma once

#include <WiFi.h>

namespace WiFiUtils {
    void hardReset();
    void shutdown();
    void stopPromiscuous();
}
