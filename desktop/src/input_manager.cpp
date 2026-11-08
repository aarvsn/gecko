// SPDX-License-Identifier: MIT
#include "input_manager.hpp"

#include <QKeySequence>

#if defined(GECKO_WITH_SDL2)
#include <SDL.h>
#endif

namespace gecko {

struct InputManager::SdlState {
#if defined(GECKO_WITH_SDL2)
    bool initialized = false;
#else
    bool initialized = false;
#endif
};

InputManager::InputManager(Config& config) : config_(config) {
    loadBindings();
}

InputManager::~InputManager() {
#if defined(GECKO_WITH_SDL2)
    if (sdl_ && sdl_->initialized) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
#endif
}

void InputManager::loadBindings() {
    for (unsigned i = 0; i < kJoypadButtonCount; ++i) {
        const std::string key = std::string("input.keys.") + kJoypadButtonNames[i];
        const std::string stored = config_.getString(key);
        if (stored.empty()) {
            keys_[i] = kDefaultKeys[i];
        } else {
            const int parsed = keyFromName(QString::fromStdString(stored));
            keys_[i] = parsed > 0 ? parsed : kDefaultKeys[i];
        }
    }
}

void InputManager::saveBindings() const {
    for (unsigned i = 0; i < kJoypadButtonCount; ++i) {
        const std::string key = std::string("input.keys.") + kJoypadButtonNames[i];
        config_.set(key, keyName(keys_[i]).toStdString());
    }
    config_.save();
}

void InputManager::setBinding(unsigned joypadButton, int key) {
    if (joypadButton >= kJoypadButtonCount) return;
    keys_[joypadButton] = key;
}

QString InputManager::keyName(int key) {
    if (key == 0) return QString("—");
    if (key == Qt::Key_Return) return QString("Enter");
    const QString name = QKeySequence(key).toString(QKeySequence::NativeText);
    return name.isEmpty() ? QString("Key 0x%1").arg(key, 0, 16) : name;
}

int InputManager::keyFromName(const QString& name) {
    if (name.isEmpty() || name == QStringLiteral("—") || name == QStringLiteral("-")) return 0;
    if (name == QStringLiteral("Enter")) return Qt::Key_Return;
    const QKeySequence sequence(name);
    if (sequence.count() == 1) return sequence[0].key();
    // Also accept single characters.
    if (name.length() == 1) return name.at(0).toUpper().unicode();
    return 0;
}

bool InputManager::handleKeyPress(int key, unsigned port) {
    for (unsigned i = 0; i < kJoypadButtonCount; ++i) {
        if (keys_[i] != 0 && keys_[i] == key) {
            source_.setButton(port, i, true);
            return true;
        }
    }
    return false;
}

bool InputManager::handleKeyRelease(int key, unsigned port) {
    for (unsigned i = 0; i < kJoypadButtonCount; ++i) {
        if (keys_[i] != 0 && keys_[i] == key) {
            source_.setButton(port, i, false);
            return true;
        }
    }
    return false;
}

void InputManager::initGamepads() {
    sdl_ = std::make_unique<SdlState>();
#if defined(GECKO_WITH_SDL2)
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        sdl_->initialized = false;
        return;
    }
    sdl_->initialized = true;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) SDL_GameControllerOpen(i);
    }
#endif
}

void InputManager::pollGamepads() {
#if defined(GECKO_WITH_SDL2)
    if (!sdl_ || !sdl_->initialized) return;
    SDL_GameControllerUpdate();

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        SDL_GameController* pad = SDL_GameControllerOpen(i);
        if (!pad) continue;
        const unsigned port = static_cast<unsigned>(i) % JoypadStateSource::kMaxPorts;

        static const std::pair<SDL_GameControllerButton, unsigned> buttons[] = {
            {SDL_CONTROLLER_BUTTON_A, RETRO_DEVICE_ID_JOYPAD_A},
            {SDL_CONTROLLER_BUTTON_B, RETRO_DEVICE_ID_JOYPAD_B},
            {SDL_CONTROLLER_BUTTON_X, RETRO_DEVICE_ID_JOYPAD_X},
            {SDL_CONTROLLER_BUTTON_Y, RETRO_DEVICE_ID_JOYPAD_Y},
            {SDL_CONTROLLER_BUTTON_BACK, RETRO_DEVICE_ID_JOYPAD_SELECT},
            {SDL_CONTROLLER_BUTTON_START, RETRO_DEVICE_ID_JOYPAD_START},
            {SDL_CONTROLLER_BUTTON_DPAD_UP, RETRO_DEVICE_ID_JOYPAD_UP},
            {SDL_CONTROLLER_BUTTON_DPAD_DOWN, RETRO_DEVICE_ID_JOYPAD_DOWN},
            {SDL_CONTROLLER_BUTTON_DPAD_LEFT, RETRO_DEVICE_ID_JOYPAD_LEFT},
            {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, RETRO_DEVICE_ID_JOYPAD_RIGHT},
            {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, RETRO_DEVICE_ID_JOYPAD_L},
            {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, RETRO_DEVICE_ID_JOYPAD_R},
            {SDL_CONTROLLER_BUTTON_LEFTSTICK, RETRO_DEVICE_ID_JOYPAD_L3},
            {SDL_CONTROLLER_BUTTON_RIGHTSTICK, RETRO_DEVICE_ID_JOYPAD_R3},
        };
        for (const auto& [sdlButton, retroId] : buttons) {
            const bool pressed =
                SDL_GameControllerGetButton(pad, sdlButton) == SDL_PRESSED;
            source_.setButton(port, retroId, pressed);
        }

        constexpr int kDeadzone = 8000;
        const auto axis = [&](SDL_GameControllerAxis a) {
            return SDL_GameControllerGetAxis(pad, a);
        };
        source_.setAnalog(port, 0, std::abs(axis(SDL_CONTROLLER_AXIS_LEFTX)) > kDeadzone
                                       ? axis(SDL_CONTROLLER_AXIS_LEFTX)
                                       : int16_t(0));
        source_.setAnalog(port, 1, std::abs(axis(SDL_CONTROLLER_AXIS_LEFTY)) > kDeadzone
                                       ? axis(SDL_CONTROLLER_AXIS_LEFTY)
                                       : int16_t(0));
        source_.setAnalog(port, 2, std::abs(axis(SDL_CONTROLLER_AXIS_RIGHTX)) > kDeadzone
                                       ? axis(SDL_CONTROLLER_AXIS_RIGHTX)
                                       : int16_t(0));
        source_.setAnalog(port, 3, std::abs(axis(SDL_CONTROLLER_AXIS_RIGHTY)) > kDeadzone
                                       ? axis(SDL_CONTROLLER_AXIS_RIGHTY)
                                       : int16_t(0));

        // Triggers as digital L2/R2.
        source_.setButton(port, RETRO_DEVICE_ID_JOYPAD_L2,
                          axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 8192);
        source_.setButton(port, RETRO_DEVICE_ID_JOYPAD_R2,
                          axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 8192);
    }
#endif
    (void)sdl_;
}

} // namespace gecko
