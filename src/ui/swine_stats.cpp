// SWINE STATS - Lifetime statistics and active buff/debuff overlay

#include "swine_stats.h"
#include "display.h"
#include "../core/xp.h"
#include "../core/config.h"
#include "../piglet/mood.h"
#include <M5Cardputer.h>

// Static member initialization
bool SwineStats::active = false;
bool SwineStats::keyWasPressed = false;
BuffState SwineStats::currentBuffs = {0, 0};
uint32_t SwineStats::lastBuffUpdate = 0;

// Buff names and descriptions (leet one-word style)
static const char* BUFF_NAMES[] = {
    "R4G3",
    "SNOUT$HARP",
    "H0TSTR3AK",
    "C4FF31N4T3D"
};

static const char* BUFF_DESCS[] = {
    "+50% deauth pwr",
    "+25% XP gain",
    "+10% deauth eff",
    "-30% hop delay"
};

static const char* DEBUFF_NAMES[] = {
    "SLOP$LUG",
    "F0GSNOUT",
    "TR0UGHDR41N",
    "HAM$TR1NG"
};

static const char* DEBUFF_DESCS[] = {
    "-30% deauth pwr",
    "-15% XP gain",
    "+2ms jitter",
    "+50% hop delay"
};

// Stat names (leet one-word)
static const char* STAT_LABELS[] = {
    "N3TW0RKS",
    "H4NDSH4K3S",
    "PMK1DS",
    "D34UTHS",
    "D1ST4NC3",
    "BL3 BL4STS",
    "S3SS10NS",
    "GH0STS",
    "WP4THR33",
    "G30L0CS"
};

void SwineStats::init() {
    active = false;
    keyWasPressed = false;
    currentBuffs = {0, 0};
    lastBuffUpdate = 0;
}

void SwineStats::show() {
    active = true;
    keyWasPressed = true;  // Ignore the key that activated us
    currentBuffs = calculateBuffs();
    lastBuffUpdate = millis();
}

void SwineStats::hide() {
    active = false;
}

void SwineStats::update() {
    if (!active) return;
    
    // Update buffs periodically
    if (millis() - lastBuffUpdate > 1000) {
        currentBuffs = calculateBuffs();
        lastBuffUpdate = millis();
    }
    
    handleInput();
}

void SwineStats::handleInput() {
    bool anyPressed = M5Cardputer.Keyboard.isPressed();
    
    if (!anyPressed) {
        keyWasPressed = false;
        return;
    }
    
    if (keyWasPressed) return;
    keyWasPressed = true;
    
    // Any key exits (Enter, backtick, or any letter)
    // The mode change will be handled by porkchop.cpp
}

BuffState SwineStats::calculateBuffs() {
    BuffState state = {0, 0};
    int happiness = Mood::getEffectiveHappiness();
    const SessionStats& session = XP::getSession();
    
    // === BUFFS ===
    
    // R4G3: happiness > 70 = +50% deauth
    if (happiness > 70) {
        state.buffs |= (uint8_t)PorkBuff::R4G3;
    }
    
    // SNOUT$HARP: happiness > 50 = +25% XP
    if (happiness > 50) {
        state.buffs |= (uint8_t)PorkBuff::SNOUT_SHARP;
    }
    
    // H0TSTR3AK: 2+ handshakes in session
    if (session.handshakes >= 2) {
        state.buffs |= (uint8_t)PorkBuff::H0TSTR3AK;
    }
    
    // C4FF31N4T3D: happiness > 80 = faster hopping
    if (happiness > 80) {
        state.buffs |= (uint8_t)PorkBuff::C4FF31N4T3D;
    }
    
    // === DEBUFFS ===
    
    // SLOP$LUG: happiness < -50 = -30% deauth
    if (happiness < -50) {
        state.debuffs |= (uint8_t)PorkDebuff::SLOP_SLUG;
    }
    
    // F0GSNOUT: happiness < -30 = -15% XP
    if (happiness < -30) {
        state.debuffs |= (uint8_t)PorkDebuff::F0GSNOUT;
    }
    
    // TR0UGHDR41N: no activity for 5 minutes (uses Mood's activity tracking)
    uint32_t lastActivity = Mood::getLastActivityTime();
    uint32_t idleTime = (lastActivity > 0) ? (millis() - lastActivity) : 0;
    if (idleTime > 300000) {
        state.debuffs |= (uint8_t)PorkDebuff::TR0UGHDR41N;
    }
    
    // HAM$TR1NG: happiness < -70 = slow hopping
    if (happiness < -70) {
        state.debuffs |= (uint8_t)PorkDebuff::HAM_STR1NG;
    }
    
    return state;
}

uint8_t SwineStats::getDeauthBurstCount() {
    BuffState buffs = calculateBuffs();
    uint8_t base = 5;  // Default burst count
    
    // R4G3: +50% (5 -> 8)
    if (buffs.hasBuff(PorkBuff::R4G3)) {
        base = 8;
    }
    // H0TSTR3AK: +10% (stacks with R4G3 check first)
    else if (buffs.hasBuff(PorkBuff::H0TSTR3AK)) {
        base = 6;
    }
    
    // SLOP$LUG: -30% (5 -> 3, or 8 -> 5, or 6 -> 4)
    if (buffs.hasDebuff(PorkDebuff::SLOP_SLUG)) {
        base = (base * 7) / 10;  // 70% of original
        if (base < 2) base = 2;  // Minimum 2 frames
    }
    
    return base;
}

uint8_t SwineStats::getDeauthJitterMax() {
    BuffState buffs = calculateBuffs();
    uint8_t base = 5;  // Default 1-5ms jitter
    
    // TR0UGHDR41N: +2ms jitter
    if (buffs.hasDebuff(PorkDebuff::TR0UGHDR41N)) {
        base = 7;  // 1-7ms jitter
    }
    
    return base;
}

uint16_t SwineStats::getChannelHopInterval() {
    BuffState buffs = calculateBuffs();
    uint16_t base = Config::wifi().channelHopInterval;  // Default from config
    
    // C4FF31N4T3D: -30% interval (faster)
    if (buffs.hasBuff(PorkBuff::C4FF31N4T3D)) {
        base = (base * 7) / 10;
    }
    
    // HAM$TR1NG: +50% interval (slower)
    if (buffs.hasDebuff(PorkDebuff::HAM_STR1NG)) {
        base = (base * 15) / 10;
    }
    
    return base;
}

float SwineStats::getXPMultiplier() {
    BuffState buffs = calculateBuffs();
    float mult = 1.0f;
    
    // SNOUT$HARP: +25% XP
    if (buffs.hasBuff(PorkBuff::SNOUT_SHARP)) {
        mult += 0.25f;
    }
    
    // F0GSNOUT: -15% XP
    if (buffs.hasDebuff(PorkDebuff::F0GSNOUT)) {
        mult -= 0.15f;
    }
    
    return mult;
}

const char* SwineStats::getBuffName(PorkBuff b) {
    switch (b) {
        case PorkBuff::R4G3: return BUFF_NAMES[0];
        case PorkBuff::SNOUT_SHARP: return BUFF_NAMES[1];
        case PorkBuff::H0TSTR3AK: return BUFF_NAMES[2];
        case PorkBuff::C4FF31N4T3D: return BUFF_NAMES[3];
        default: return "???";
    }
}

const char* SwineStats::getDebuffName(PorkDebuff d) {
    switch (d) {
        case PorkDebuff::SLOP_SLUG: return DEBUFF_NAMES[0];
        case PorkDebuff::F0GSNOUT: return DEBUFF_NAMES[1];
        case PorkDebuff::TR0UGHDR41N: return DEBUFF_NAMES[2];
        case PorkDebuff::HAM_STR1NG: return DEBUFF_NAMES[3];
        default: return "???";
    }
}

const char* SwineStats::getBuffDesc(PorkBuff b) {
    switch (b) {
        case PorkBuff::R4G3: return BUFF_DESCS[0];
        case PorkBuff::SNOUT_SHARP: return BUFF_DESCS[1];
        case PorkBuff::H0TSTR3AK: return BUFF_DESCS[2];
        case PorkBuff::C4FF31N4T3D: return BUFF_DESCS[3];
        default: return "";
    }
}

const char* SwineStats::getDebuffDesc(PorkDebuff d) {
    switch (d) {
        case PorkDebuff::SLOP_SLUG: return DEBUFF_DESCS[0];
        case PorkDebuff::F0GSNOUT: return DEBUFF_DESCS[1];
        case PorkDebuff::TR0UGHDR41N: return DEBUFF_DESCS[2];
        case PorkDebuff::HAM_STR1NG: return DEBUFF_DESCS[3];
        default: return "";
    }
}

void SwineStats::draw(M5Canvas& canvas) {
    if (!active) return;
    
    canvas.fillSprite(COLOR_BG);
    canvas.setTextColor(COLOR_FG);
    
    // Level and title bar (no title header - it's in top bar now)
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);
    
    uint8_t level = XP::getLevel();
    const char* title = XP::getTitle();
    uint8_t progress = XP::getProgress();
    
    char lvlBuf[48];
    snprintf(lvlBuf, sizeof(lvlBuf), "LVL %d: %s", level, title);
    canvas.drawString(lvlBuf, 5, 2);
    
    // XP text on right
    char xpBuf[32];
    snprintf(xpBuf, sizeof(xpBuf), "%lu XP (%d%%)", (unsigned long)XP::getTotalXP(), progress);
    canvas.setTextDatum(top_right);
    canvas.drawString(xpBuf, DISPLAY_W - 5, 2);
    
    // Progress bar
    int barX = 5;
    int barY = 12;
    int barW = DISPLAY_W - 10;
    int barH = 6;
    canvas.drawRect(barX, barY, barW, barH, COLOR_FG);
    int fillW = (barW - 2) * progress / 100;
    if (fillW > 0) {
        canvas.fillRect(barX + 1, barY + 1, fillW, barH - 2, COLOR_FG);
    }
    
    // Stats section (moved up)
    drawStats(canvas);
    
    // Buffs section (moved up)
    drawBuffs(canvas, 68);
}

void SwineStats::drawStats(M5Canvas& canvas) {
    const PorkXPData& data = XP::getData();
    
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);
    
    int y = 22;  // Start right after progress bar
    int lineH = 10;
    int col1 = 5;
    int col2 = 75;
    int col3 = 125;
    int col4 = 195;
    
    // Row 1: Networks, Handshakes
    canvas.drawString("N3TW0RKS:", col1, y);
    char buf[16];
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)data.lifetimeNetworks);
    canvas.drawString(buf, col2, y);
    
    canvas.drawString("H4NDSH4K3S:", col3, y);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)data.lifetimeHS);
    canvas.drawString(buf, col4, y);
    
    y += lineH;
    
    // Row 2: PMKIDs, Deauths
    canvas.drawString("PMK1DS:", col1, y);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)data.lifetimePMKID);
    canvas.drawString(buf, col2, y);
    
    canvas.drawString("D34UTHS:", col3, y);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)data.lifetimeDeauths);
    canvas.drawString(buf, col4, y);
    
    y += lineH;
    
    // Row 3: Distance, BLE
    canvas.drawString("D1ST4NC3:", col1, y);
    snprintf(buf, sizeof(buf), "%.1fkm", data.lifetimeDistance / 1000.0f);
    canvas.drawString(buf, col2, y);
    
    canvas.drawString("BL3 BL4STS:", col3, y);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)data.lifetimeBLE);
    canvas.drawString(buf, col4, y);
    
    y += lineH;
    
    // Row 4: Sessions, Hidden
    canvas.drawString("S3SS10NS:", col1, y);
    snprintf(buf, sizeof(buf), "%u", data.sessions);
    canvas.drawString(buf, col2, y);
    
    canvas.drawString("GH0STS:", col3, y);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)data.hiddenNetworks);
    canvas.drawString(buf, col4, y);
}

void SwineStats::drawBuffs(M5Canvas& canvas, int yStart) {
    canvas.setTextSize(1);
    canvas.setTextDatum(top_left);
    
    // Section header (no divider line)
    canvas.drawString("ACT1V3 B00STS:", 5, yStart);
    
    int y = yStart + 10;
    int buffCount = 0;
    
    // Draw active buffs
    if (currentBuffs.buffs != 0) {
        for (int i = 0; i < 4; i++) {
            PorkBuff b = (PorkBuff)(1 << i);
            if (currentBuffs.hasBuff(b)) {
                char buf[48];
                snprintf(buf, sizeof(buf), "[+] %s %s", getBuffName(b), getBuffDesc(b));
                canvas.drawString(buf, 5, y);
                y += 10;
                buffCount++;
                if (buffCount >= 2) break;  // Max 2 lines for buffs
            }
        }
    }
    
    // Draw active debuffs
    int debuffCount = 0;
    if (currentBuffs.debuffs != 0) {
        for (int i = 0; i < 4; i++) {
            PorkDebuff d = (PorkDebuff)(1 << i);
            if (currentBuffs.hasDebuff(d)) {
                char buf[48];
                snprintf(buf, sizeof(buf), "[-] %s %s", getDebuffName(d), getDebuffDesc(d));
                canvas.drawString(buf, 5, y);
                y += 10;
                debuffCount++;
                if (debuffCount >= 2) break;  // Max 2 lines for debuffs
            }
        }
    }
    
    // If no buffs/debuffs
    if (buffCount == 0 && debuffCount == 0) {
        canvas.drawString("[=] N0N3 ACT1V3", 5, y);
    }
    
    // Footer hint
    canvas.setTextDatum(bottom_center);
    canvas.drawString("press any key to exit", DISPLAY_W / 2, DISPLAY_H - 2);
}

