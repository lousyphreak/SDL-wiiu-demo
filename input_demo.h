#pragma once

#include "demo_state.h"

void ResetInputDemoState(AppState *state);
void OpenInitialInputDevices(AppState *state);
void ShutdownInputDemo(AppState *state);
void PollInputDemo(AppState *state);
bool HandleInputDemoEvent(AppState *state, const SDL_Event *event);
SDL_Gamepad *GetActiveGamepad(AppState *state);
bool IsAnyInputButtonPressed(AppState *state);
bool RenderInputOverlay(AppState *state, int width, int height);
