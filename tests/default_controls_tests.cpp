#include "input/input_manager.h"
#include "mouse_keyboard_source.h"
#include "runtime/config.h"
#include <SDL.h>

#include <array>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace {
std::array<Uint8, SDL_NUM_SCANCODES> host_keys{};
Uint32 host_mouse = 0;

using namespace nwii::runtime::input;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void expect_remote(std::uint32_t wanted, const char* name) {
    InputManager::get().update();
    const auto state = InputManager::get().get_wiimote_state(0);
    if (state.buttons != wanted) {
        std::ostringstream message;
        message << name << ": expected remote buttons 0x" << std::hex << wanted
                << ", got 0x" << state.buttons;
        throw std::runtime_error(message.str());
    }
    require(state.err == 0, "keyboard/mouse must report a connected remote");
    require(state.ir_x == 0.25f && state.ir_y == 0.75f,
            "button mappings must preserve mouse pointer coordinates");
    for (int channel = 1; channel < 4; ++channel) {
        const auto other = InputManager::get().get_wiimote_state(channel);
        require(other.buttons == 0 && other.err == -1,
                "default keyboard/mouse controls must affect only player one");
    }
}

void clear_host() {
    host_keys.fill(0);
    host_mouse = 0;
}

// Replacing any requested binding, failing to rotate WASD for a left-held
// sideways remote, or restoring old conflicting aliases breaks these cases.
void requested_controls_produce_raw_sideways_remote_buttons() {
    struct KeyCase { SDL_Scancode key; std::uint32_t wanted; const char* name; };
    constexpr KeyCase cases[] = {
        {SDL_SCANCODE_W, 0x0002, "W: horizontal up"},
        {SDL_SCANCODE_A, 0x0004, "A: horizontal left"},
        {SDL_SCANCODE_S, 0x0001, "S: horizontal down"},
        {SDL_SCANCODE_D, 0x0008, "D: horizontal right"},
        {SDL_SCANCODE_ESCAPE, 0x0010, "Escape: Plus"},
        {SDL_SCANCODE_TAB, 0x1000, "Tab: Minus"},
        {SDL_SCANCODE_E, 0x0800, "E: remote A"}
    };
    clear_host();
    host_mouse = SDL_BUTTON(SDL_BUTTON_LEFT);
    expect_remote(0x0100, "left mouse: remote 2");
    clear_host();
    host_mouse = SDL_BUTTON(SDL_BUTTON_RIGHT);
    expect_remote(0x0200, "right mouse: remote 1");
    for (const auto& test : cases) {
        clear_host();
        host_keys[test.key] = 1;
        expect_remote(test.wanted, test.name);
    }
    for (auto key : {SDL_SCANCODE_RETURN, SDL_SCANCODE_BACKSPACE,
                     SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT,
                     SDL_SCANCODE_RIGHT, SDL_SCANCODE_SPACE}) {
        clear_host();
        host_keys[key] = 1;
        expect_remote(0, "unassigned Wii key");
    }
}

// The input source must OR simultaneous buttons into one sample, and the real
// InputManager must clear those held bits on the following released frame.
void simultaneous_controls_and_release_produce_current_frame_state() {
    clear_host();
    expect_remote(0, "no input");
    host_mouse = SDL_BUTTON(SDL_BUTTON_LEFT) | SDL_BUTTON(SDL_BUTTON_RIGHT);
    host_keys[SDL_SCANCODE_W] = host_keys[SDL_SCANCODE_D] = 1;
    host_keys[SDL_SCANCODE_ESCAPE] = host_keys[SDL_SCANCODE_TAB] = 1;
    host_keys[SDL_SCANCODE_E] = 1;
    expect_remote(0x1B1A, "simultaneous movement and remote buttons");
    expect_remote(0x1B1A, "held combination");
    host_keys[SDL_SCANCODE_E] = 0;
    host_mouse = SDL_BUTTON(SDL_BUTTON_RIGHT);
    expect_remote(0x121A, "partial release");
    clear_host();
    expect_remote(0, "all controls released");
}

// Altering the Wii bindings must not change the existing GC digital/stick
// bindings, including the keys that are no longer aliases for Wii buttons.
void gamecube_bindings_remain_unchanged() {
    struct KeyCase { SDL_Scancode key; std::uint16_t buttons; int x; int y; };
    constexpr KeyCase cases[] = {
        {SDL_SCANCODE_SPACE, 0x0100, 0, 0},
        {SDL_SCANCODE_RETURN, 0x1000, 0, 0},
        {SDL_SCANCODE_UP, 0x0008, 0, 0},
        {SDL_SCANCODE_DOWN, 0x0004, 0, 0},
        {SDL_SCANCODE_LEFT, 0x0001, 0, 0},
        {SDL_SCANCODE_RIGHT, 0x0002, 0, 0},
        {SDL_SCANCODE_W, 0, 0, 127},
        {SDL_SCANCODE_S, 0, 0, -128},
        {SDL_SCANCODE_D, 0, 127, 0},
        {SDL_SCANCODE_A, 0, -128, 0},
        {SDL_SCANCODE_ESCAPE, 0, 0, 0},
        {SDL_SCANCODE_TAB, 0, 0, 0},
        {SDL_SCANCODE_E, 0, 0, 0}
    };
    for (const auto& test : cases) {
        clear_host();
        host_keys[test.key] = 1;
        InputManager::get().update();
        const auto state = InputManager::get().get_gcpad_state(0);
        require(state.buttons == test.buttons && state.stick_x == test.x && state.stick_y == test.y,
                "GameCube keyboard bindings must remain unchanged");
        require(state.err == 0 && state.substick_x == 0 && state.substick_y == 0 &&
                state.trigger_l == 0 && state.trigger_r == 0,
                "GameCube state must retain its original connected/default axes");
    }
    clear_host();
    InputManager::get().update();
    const auto released = InputManager::get().get_gcpad_state(0);
    require(released.buttons == 0 && released.stick_x == 0 && released.stick_y == 0,
            "released GameCube keys must clear buttons and sticks");
}
}

extern "C" Uint32 SDL_GetMouseState(int* x, int* y) {
    *x = 160;
    *y = 360;
    return host_mouse;
}
extern "C" const Uint8* SDL_GetKeyboardState(int* count) {
    if (count) *count = static_cast<int>(host_keys.size());
    return host_keys.data();
}

int main() {
    try {
        nwii::runtime::Config::get().window_width = 640;
        nwii::runtime::Config::get().window_height = 480;
        InputManager::get().add_source(std::make_unique<MouseKeyboardSource>());
        requested_controls_produce_raw_sideways_remote_buttons();
        simultaneous_controls_and_release_produce_current_frame_state();
        gamecube_bindings_remain_unchanged();
        std::cout << "PASS: default sideways Wii controls and unchanged GameCube bindings\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
