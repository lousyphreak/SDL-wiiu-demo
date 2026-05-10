#pragma once

#include "demo_state.h"

void ResetCameraDemoState(CameraDemoState *camera);
bool InitCameraDemo(AppState *state);
void ShutdownCameraDemo(AppState *state);
bool HandleCameraDemoEvent(AppState *state, const SDL_Event *event);
bool ToggleCameraDemo(AppState *state);
void UpdateCameraDemo(AppState *state);
bool RenderCameraDemo(AppState *state, int width, int height, int horizon);
