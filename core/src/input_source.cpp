// SPDX-License-Identifier: MIT
#include "gecko/input_source.hpp"

namespace gecko {

const char* const kJoypadButtonNames[kJoypadButtonCount] = {
    "B", "Y", "Select", "Start",
    "Up", "Down", "Left", "Right",
    "A", "X", "L", "R",
    "L2", "R2", "L3", "R3",
};

const char* const kJoypadButtonShortNames[kJoypadButtonCount] = {
    "B", "Y", "SEL", "STA",
    "UP", "DN", "LF", "RT",
    "A", "X", "L", "R",
    "L2", "R2", "L3", "R3",
};

void JoypadStateSource::Pad::setButton(unsigned id, bool pressed) {
    if (id < kJoypadButtonCount) buttons[id] = pressed;
}

void JoypadStateSource::Pad::setAnalog(unsigned index, int16_t value) {
    if (index < analog.size()) analog[index] = value;
}

uint16_t JoypadStateSource::Pad::buttonMask() const {
    uint16_t mask = 0;
    for (unsigned i = 0; i < kJoypadButtonCount; ++i)
        if (buttons[i]) mask |= static_cast<uint16_t>(1u << i);
    return mask;
}

void JoypadStateSource::Pad::clear() {
    buttons.fill(false);
    analog.fill(0);
}

int16_t JoypadStateSource::state(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port >= kMaxPorts) return 0;

    std::lock_guard<std::mutex> guard(mutex_);
    const Pad& pad = pads_[port];

    switch (device) {
        case RETRO_DEVICE_JOYPAD:
            if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
                return static_cast<int16_t>(pad.buttonMask());
            if (id < kJoypadButtonCount)
                return pad.buttons[id] ? 1 : 0;
            return 0;
        case RETRO_DEVICE_ANALOG: {
            if (index == RETRO_DEVICE_INDEX_ANALOG_LEFT && id < 2)
                return pad.analog[id];
            if (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT && id < 2)
                return pad.analog[2 + id];
            return 0;
        }
        default:
            return 0;
    }
}

void JoypadStateSource::setButton(unsigned port, unsigned id, bool pressed) {
    if (port >= kMaxPorts || id >= kJoypadButtonCount) return;
    std::lock_guard<std::mutex> guard(mutex_);
    pads_[port].buttons[id] = pressed;
}

void JoypadStateSource::setAnalog(unsigned port, unsigned index, int16_t value) {
    if (port >= kMaxPorts || index >= 4) return;
    std::lock_guard<std::mutex> guard(mutex_);
    pads_[port].analog[index] = value;
}

void JoypadStateSource::clearAll() {
    std::lock_guard<std::mutex> guard(mutex_);
    for (auto& pad : pads_) pad.clear();
}

JoypadStateSource::Pad JoypadStateSource::pad(unsigned port) const {
    if (port >= kMaxPorts) return Pad{};
    std::lock_guard<std::mutex> guard(mutex_);
    return pads_[port];
}

} // namespace gecko
