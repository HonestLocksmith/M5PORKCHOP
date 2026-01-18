// ML Feature Extraction for WiFi analysis
#pragma once

#include <Arduino.h>
#include <esp_wifi.h>
#include <vector>
#include "ml_config.h"

// Feature vector size for Edge Impulse model
#define FEATURE_VECTOR_SIZE 32

#if PORKCHOP_ENABLE_ML
struct WiFiFeatures {
    // Signal characteristics
    int8_t rssi;
    int8_t noise;
    float snr;
    
    // Channel info
    uint8_t channel;
    uint8_t secondaryChannel;
    
    // Beacon analysis
    uint16_t beaconInterval;
    uint16_t capability;
    bool hasWPS;
    bool hasWPA;
    bool hasWPA2;
    bool hasWPA3;
    bool isHidden;
    
    // Timing features
    uint32_t responseTime;
    uint16_t beaconCount;
    float beaconJitter;
    
    // Probe response analysis
    bool respondsToProbe;
    uint16_t probeResponseTime;
    
    // IEs (Information Elements)
    uint8_t vendorIECount;
    uint8_t supportedRates;
    uint8_t htCapabilities;
    uint8_t vhtCapabilities;
    
    // Derived
    float anomalyScore;
};

struct ProbeFeatures {
    uint8_t macPrefix[3];
    uint8_t probeCount;
    uint8_t uniqueSSIDCount;
    bool randomMAC;
    int8_t avgRSSI;
    uint32_t lastSeen;
};

class FeatureExtractor {
public:
    static void init();
    
    // Extract features from raw WiFi scan
    static WiFiFeatures extractFromScan(const wifi_ap_record_t* ap);
    static WiFiFeatures extractFromBeacon(const uint8_t* frame, uint16_t len, int8_t rssi);
    
    // Extract basic features when only Arduino WiFi accessors are available
    static WiFiFeatures extractBasic(int8_t rssi, uint8_t channel, wifi_auth_mode_t authmode);
    
    // Extract probe request features
    static ProbeFeatures extractFromProbe(const uint8_t* frame, uint16_t len, int8_t rssi);
    
    // Convert to feature vector for ML
    static void toFeatureVector(const WiFiFeatures& features, float* output);
    static void probeToFeatureVector(const ProbeFeatures& features, float* output);
    
    // Batch feature extraction
    static std::vector<float> extractBatchFeatures(const std::vector<WiFiFeatures>& networks);
    
    // Normalization (must be called after model training)
    static void setNormalizationParams(const float* means, const float* stds);
    
private:
    static float featureMeans[FEATURE_VECTOR_SIZE];
    static float featureStds[FEATURE_VECTOR_SIZE];
    static bool normParamsLoaded;
    
    static uint16_t parseBeaconInterval(const uint8_t* frame, uint16_t len);
    static uint16_t parseCapability(const uint8_t* frame, uint16_t len);
    static void parseIEs(const uint8_t* frame, uint16_t len, WiFiFeatures& features);
    static bool isRandomMAC(const uint8_t* mac);
    static float normalize(float value, float mean, float std);
};
#else
// ML disabled: provide lightweight stubs so callers compile without pulling ML code
struct WiFiFeatures {
    int8_t rssi{0};
    int8_t noise{-95};
    float snr{0.0f};
    uint16_t beaconCount{0};
};

struct ProbeFeatures {
    uint8_t reserved{0};
};

class FeatureExtractor {
public:
    static void init() {}
    static WiFiFeatures extractFromScan(const wifi_ap_record_t*) { return WiFiFeatures(); }
    static WiFiFeatures extractFromBeacon(const uint8_t*, uint16_t, int8_t) { return WiFiFeatures(); }
    static WiFiFeatures extractBasic(int8_t, uint8_t, wifi_auth_mode_t) { return WiFiFeatures(); }
    static ProbeFeatures extractFromProbe(const uint8_t*, uint16_t, int8_t) { return ProbeFeatures(); }
    static void toFeatureVector(const WiFiFeatures&, float*) {}
    static void probeToFeatureVector(const ProbeFeatures&, float*) {}
    static std::vector<float> extractBatchFeatures(const std::vector<WiFiFeatures>&) { return {}; }
    static void setNormalizationParams(const float*, const float*) {}
};
#endif
