// Simple RAII helper to resume sprites on scope exit.
#pragma once

#include "../ui/display.h"

class ScopeResume {
public:
    explicit ScopeResume(bool active = true) : active_(active) {}
    ~ScopeResume() {
        if (active_) Display::requestResumeSprites();
    }
    // Disable copy
    ScopeResume(const ScopeResume&) = delete;
    ScopeResume& operator=(const ScopeResume&) = delete;
    // Allow move
    ScopeResume(ScopeResume&& other) noexcept : active_(other.active_) { other.active_ = false; }
private:
    bool active_;
};
