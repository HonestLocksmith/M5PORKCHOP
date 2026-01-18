// Edge Impulse Model Inference
#pragma once

#include <Arduino.h>
#include <functional>
#include "ml_config.h"
#include "features.h"

// Model labels
enum class MLLabel {
    NORMAL = 0,
    ROGUE_AP = 1,
    EVIL_TWIN = 2,
    DEAUTH_TARGET = 3,
    VULNERABLE = 4,
    UNKNOWN = 255
};

struct MLResult {
    MLLabel label;
    float confidence;
    float scores[5];  // Confidence for each class
    uint32_t inferenceTimeUs;
    bool valid;
};

typedef std::function<void(MLResult)> MLCallback;

#if PORKCHOP_ENABLE_ML
class MLInference {
public:
    static void init();
    static void update();
    
    // Inference
    static MLResult classify(const float* features, size_t featureCount);
    static MLResult classifyNetwork(const WiFiFeatures& network);
    
    // Async inference with callback
    static void classifyAsync(const float* features, size_t featureCount, MLCallback callback);
    
    // Model management
    static bool loadModel(const char* path);
    static bool saveModel(const char* path);
    static bool updateModel(const uint8_t* modelData, size_t size);
    
    // Model info
    static const char* getModelVersion();
    static size_t getModelSize();
    static bool isModelLoaded();
    
    // OTA model update
    static bool checkForUpdate(const char* serverUrl);
    static bool downloadAndUpdate(const char* url, bool promptUser = true);
    
    // Statistics
    static uint32_t getInferenceCount() { return inferenceCount; }
    static uint32_t getAvgInferenceTimeUs() { return avgInferenceTime; }
    
private:
    static bool modelLoaded;
    static char modelVersion[16];
    static size_t modelSize;
    static uint32_t inferenceCount;
    static uint32_t avgInferenceTime;
    
    // Model weights stored in SPIFFS
    static const char* MODEL_PATH;
    
    static MLResult runInference(const float* input, size_t size);
    static bool validateModel(const uint8_t* data, size_t size);
};
#else
// ML disabled: provide no-op stubs to keep callers compiling without pulling model
class MLInference {
public:
    static void init() {}
    static void update() {}
    static MLResult classify(const float*, size_t) { return {MLLabel::UNKNOWN, 0.0f, {0}, 0, false}; }
    static MLResult classifyNetwork(const WiFiFeatures&) { return {MLLabel::UNKNOWN, 0.0f, {0}, 0, false}; }
    static void classifyAsync(const float*, size_t, MLCallback) {}
    static bool loadModel(const char*) { return false; }
    static bool saveModel(const char*) { return false; }
    static bool updateModel(const uint8_t*, size_t) { return false; }
    static const char* getModelVersion() { return "ML-OFF"; }
    static size_t getModelSize() { return 0; }
    static bool isModelLoaded() { return false; }
    static bool checkForUpdate(const char*) { return false; }
    static bool downloadAndUpdate(const char*, bool = true) { return false; }
    static uint32_t getInferenceCount() { return 0; }
    static uint32_t getAvgInferenceTimeUs() { return 0; }
};
#endif
