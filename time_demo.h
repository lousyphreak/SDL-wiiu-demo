#pragma once

#include "demo_state.h"

void ResetTimeDemoState(TimeDemoState *time_demo);
bool InitTimeDemo(AppState *state);
void ShutdownTimeDemo(AppState *state);
void BeginTimeDemoFrame(AppState *state);
void UpdateTimeDemo(AppState *state);
void MarkTimeDemoUpdatesComplete(AppState *state);
void MarkTimeDemoMainRenderComplete(AppState *state);
void EndTimeDemoFrame(AppState *state);
bool RenderTimeWidget(AppState *state, const SDL_FRect &rect);
