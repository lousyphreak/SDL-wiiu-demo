#pragma once

#include "demo_state.h"

void ResetAudioDemoState(AudioDemoState *audio);
bool InitAudioDemo(AppState *state);
void ShutdownAudioDemo(AppState *state);
void UpdateAudioDemo(AppState *state);
bool RenderAudioWidget(AppState *state, const SDL_FRect &rect);
