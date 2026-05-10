#pragma once

#include <SDL3/SDL.h>

bool SetColor(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a);
bool FillCircle(SDL_Renderer *renderer, float center_x, float center_y, float radius);
