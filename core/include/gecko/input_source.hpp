// SPDX-License-Identifier: MIT
// Input abstraction. UI layers implement InputSource; the emulator queries it
// whenever the core polls input.

#pragma once

#include "libretro.h"

#include <array>
#include <cstdint>
#include <mutex>

namespace gecko {

// Number of joypad buttons in the base libretro joypad (RETRO_DEVICE_JOYPAD).
constexpr unsigned kJoypadButtonCount = 16;

// Human readable names, indexed by RETRO_DEVICE_ID_JOYPAD_*.
extern const char* const kJoypadButtonNames[kJoypadButtonCount];
// Short labels used in compact UIs (input mapper tables, virtual gamepads).
extern const char* const kJoypadButtonShortNames[kJoypadButtonCount];

class InputSource {
public:
    virtual ~InputSource() = default;

    // Called when the core calls retro_input_poll().
    virtual void poll() {}

    // Mirrors retro_input_state_t. Implementations are expected to answer
    // RETRO_DEVICE_JOYPAD (including RETRO_DEVICE_ID_JOYPAD_MASK bitmask
    // queries when RETRO_ENVIRONMENT_GET_INPUT_BITMASKS was advertised) and
    // RETRO_DEVICE_ANALOG queries.
    virtual int16_t state(unsigned port, unsigned device, unsigned index, unsigned id) = 0;
};

// A ready-made InputSource that serves digital joypad state per port plus
// analog sticks. Keyboard (desktop), touch (Android) and the test harness all
// feed this structure.
class JoypadStateSource : public InputSource {
public:
    static constexpr unsigned kMaxPorts = 4;

    struct Pad {
        std::array<bool, kJoypadButtonCount> buttons{};
        // 0 = left X, 1 = left Y, 2 = right X, 3 = right Y.
        std::array<int16_t, 4> analog{};

        void setButton(unsigned id, bool pressed);
        void setAnalog(unsigned index, int16_t value);
        uint16_t buttonMask() const;
        void clear();
    };

    // --- InputSource ---
    int16_t state(unsigned port, unsigned device, unsigned index, unsigned id) override;

    // UI-thread side. Thread safety: state() runs on the emu thread while the
    // setters run on the UI thread; a small mutex keeps this strictly correct.
    void setButton(unsigned port, unsigned id, bool pressed);
    void setAnalog(unsigned port, unsigned index, int16_t value);
    void clearAll();

    // Non-locking read access, e.g. for the test harness.
    Pad pad(unsigned port) const;

private:
    mutable std::mutex mutex_;
    std::array<Pad, kMaxPorts> pads_{};
};

} // namespace gecko
