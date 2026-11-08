// SPDX-License-Identifier: MIT
// InputManager — maps Qt keyboard events (and optionally SDL gamepads) onto
// the libretro joypad, with a persisted key map in the config.

#pragma once

#include "gecko/config.hpp"
#include "gecko/input_source.hpp"

#include <QString>

#include <array>
#include <cstdint>
#include <memory>

namespace gecko {

class InputManager {
public:
    // Default keyboard bindings (RetroArch-flavoured).
    static constexpr int kDefaultKeys[kJoypadButtonCount] = {
        Qt::Key_Z,        // B
        Qt::Key_A,        // Y
        Qt::Key_Shift,    // Select
        Qt::Key_Return,   // Start
        Qt::Key_Up,       // Up
        Qt::Key_Down,     // Down
        Qt::Key_Left,     // Left
        Qt::Key_Right,    // Right
        Qt::Key_X,        // A
        Qt::Key_S,        // X
        Qt::Key_Q,        // L
        Qt::Key_W,        // R
        0,                // L2
        0,                // R2
        0,                // L3
        0,                // R3
    };

    explicit InputManager(Config& config);
    ~InputManager();

    // Loads/saves the key map (config keys "input.keys.<button>").
    void loadBindings();
    void saveBindings() const;

    int binding(unsigned joypadButton) const { return keys_[joypadButton]; }
    void setBinding(unsigned joypadButton, int key);
    static QString keyName(int key);       // Qt::Key -> readable label
    static int keyFromName(const QString& name);

    // Feed a key event; returns true when it mapped to a joypad button.
    bool handleKeyPress(int key, unsigned port = 0);
    bool handleKeyRelease(int key, unsigned port = 0);

    // The source wired into the Emulator.
    JoypadStateSource& source() { return source_; }

    void clearAllButtons(unsigned = 0) { source_.clearAll(); }

    // SDL2 gamepad support (compiled in only when GECKO_WITH_SDL2). Called
    // once at startup / per input poll.
    void initGamepads();
    void pollGamepads();

private:
    Config& config_;
    JoypadStateSource source_;
    std::array<int, kJoypadButtonCount> keys_{};

    struct SdlState;
    std::unique_ptr<SdlState> sdl_;
};

} // namespace gecko
