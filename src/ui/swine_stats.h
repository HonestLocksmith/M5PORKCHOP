// SWINE STATS - Lifetime statistics and active buff/debuff overlay
#pragma once

#include <M5Unified.h>

// Buff/Debuff flags (can have multiple active)
enum class PorkBuff : uint8_t {
    NONE = 0,
    // Buffs (positive effects)
    R4G3           = (1 << 0),  // +50% deauth burst when happiness > 70
    SNOUT_SHARP    = (1 << 1),  // +25% XP when happiness > 50
    H0TSTR3AK      = (1 << 2),  // +10% deauth when 2+ handshakes in session
    C4FF31N4T3D    = (1 << 3),  // -30% channel hop interval when happiness > 80
};

enum class PorkDebuff : uint8_t {
    NONE = 0,
    // Debuffs (negative effects)
    SLOP_SLUG      = (1 << 0),  // -30% deauth burst when happiness < -50
    F0GSNOUT       = (1 << 1),  // -15% XP when happiness < -30
    TR0UGHDR41N    = (1 << 2),  // +2ms deauth jitter when no activity 5min
    HAM_STR1NG     = (1 << 3),  // +50% channel hop interval when happiness < -70
};

// Active buff/debuff state
struct BuffState {
    uint8_t buffs;    // PorkBuff flags
    uint8_t debuffs;  // PorkDebuff flags
    
    bool hasBuff(PorkBuff b) const { return buffs & (uint8_t)b; }
    bool hasDebuff(PorkDebuff d) const { return debuffs & (uint8_t)d; }
};

class SwineStats {
public:
    static void init();
    static void show();
    static void hide();
    static void update();
    static void draw(M5Canvas& canvas);
    static bool isActive() { return active; }
    
    // Buff/debuff calculation (called by modes)
    static BuffState calculateBuffs();
    
    // Buff effect getters for game mechanics
    static uint8_t getDeauthBurstCount();     // Base 5, modified by buffs
    static uint8_t getDeauthJitterMax();      // Base 5ms, modified by debuffs
    static uint16_t getChannelHopInterval();  // Base from config, modified
    static float getXPMultiplier();           // 1.0 base, modified
    
    // Buff/debuff name getters for display
    static const char* getBuffName(PorkBuff b);
    static const char* getDebuffName(PorkDebuff d);
    static const char* getBuffDesc(PorkBuff b);
    static const char* getDebuffDesc(PorkDebuff d);

private:
    static bool active;
    static bool keyWasPressed;
    static BuffState currentBuffs;
    static uint32_t lastBuffUpdate;
    
    static void handleInput();
    static void drawStats(M5Canvas& canvas);
    static void drawBuffs(M5Canvas& canvas, int yStart);
};

