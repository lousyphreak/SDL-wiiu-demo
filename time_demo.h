#pragma once

#include "demo_state.h"

void ResetTimeDemoState(TimeDemoState *time_demo);
bool InitTimeDemo(AppState *state);
void ShutdownTimeDemo(AppState *state);
void UpdateTimeDemo(AppState *state);
