#pragma once

#include "demo_state.h"

void ResetStorageDemoState(StorageDemoState *storage);
bool InitStorageDemo(AppState *state);
bool PersistStorageCounter(StorageDemoState *storage, Uint32 new_count);
bool RenderStorageWidget(AppState *state, const SDL_FRect &rect);
