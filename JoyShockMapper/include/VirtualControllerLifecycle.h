#pragma once
#include "Gamepad.h"
#include <memory>
#include <mutex>

// Caller owns the configuration lock. ViGEm may wait for notification callbacks
// during creation/destruction; those callbacks acquire callbackLock themselves.
inline bool setVirtualController(std::unique_ptr<Gamepad>& current, std::mutex& callbackLock,
    ControllerScheme scheme, Gamepad::Callback callback, std::string& error,
    std::unique_ptr<Gamepad>* parked = nullptr) {
    std::unique_ptr<Gamepad> replacement;
    {
        std::lock_guard lock(callbackLock);
        if (!current && scheme == ControllerScheme::NONE) return true;
        if (current && current->getType() == scheme && current->isInitialized()) return true;
        if (parked && scheme == ControllerScheme::NONE) {
            current->neutralize();
            replacement = std::move(*parked);
            *parked = std::move(current);
        } else if (parked && *parked && (*parked)->getType() == scheme && (*parked)->isInitialized()) {
            replacement = std::move(*parked);
        }
    }
    if (parked && scheme == ControllerScheme::NONE) return true;
    if (scheme != ControllerScheme::NONE) {
        if (!replacement) replacement.reset(Gamepad::getNew(scheme, std::move(callback)));
        if (!replacement || !replacement->isInitialized(&error)) return false;
    }
    {
        std::lock_guard lock(callbackLock);
        current.swap(replacement);
    }
    // Destruction happens with callbackLock released, including failed targets.
    return true;
}
