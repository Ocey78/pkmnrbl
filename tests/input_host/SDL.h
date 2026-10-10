#pragma once

#include <cstdint>

// Only the SDL host-input boundary consumed by MouseKeyboardSource. Scancode
// values match SDL2; no input mapping or runtime state is implemented here.
using Uint8 = std::uint8_t;
using Uint32 = std::uint32_t;
enum SDL_Scancode {
    SDL_SCANCODE_A = 4,
    SDL_SCANCODE_D = 7,
    SDL_SCANCODE_E = 8,
    SDL_SCANCODE_S = 22,
    SDL_SCANCODE_W = 26,
    SDL_SCANCODE_RETURN = 40,
    SDL_SCANCODE_ESCAPE = 41,
    SDL_SCANCODE_BACKSPACE = 42,
    SDL_SCANCODE_TAB = 43,
    SDL_SCANCODE_SPACE = 44,
    SDL_SCANCODE_RIGHT = 79,
    SDL_SCANCODE_LEFT = 80,
    SDL_SCANCODE_DOWN = 81,
    SDL_SCANCODE_UP = 82,
    SDL_NUM_SCANCODES = 512
};
#define SDL_BUTTON_LEFT 1
#define SDL_BUTTON_RIGHT 3
#define SDL_BUTTON(X) (1u << ((X) - 1))

extern "C" Uint32 SDL_GetMouseState(int* x, int* y);
extern "C" const Uint8* SDL_GetKeyboardState(int* count);
