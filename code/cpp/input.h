#pragma once
#include "main.h"
#include <SDL2/SDL.h>
#include "linear_algebra.h"

namespace input {
  extern bool firstPersonMode;
  
  void handleKeyPress(SDL_Keycode code);
  void handleKeyRelease(SDL_Keycode code);
  void handleMouseClick(SDL_Window *window);
  void handleMouseMotion(int x, int y);
  vec2 getViewAngleInput();
  vec2 getMovementVector();
}