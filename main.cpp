#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "audio_demo.h"
#include "camera_demo.h"
#include "demo_state.h"
#include "input_demo.h"
#include "scene_demo.h"
#include "storage_demo.h"
#include "time_demo.h"

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    AppState *state;
    (void)argc;
    (void)argv;

    SDL_SetAppMetadata("SDL Wii U Render Demo", "1.0", "org.wiiu.sdl-demo");
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_CAMERA)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state = static_cast<AppState *>(SDL_calloc(1, sizeof(*state)));
    if (!state) {
        SDL_Log("Out of memory");
        return SDL_APP_FAILURE;
    }

    ResetInputDemoState(state);
    ResetAudioDemoState(&state->audio);
    ResetCameraDemoState(&state->camera_demo);
    ResetStorageDemoState(&state->storage);
    ResetTimeDemoState(&state->time_demo);

    state->window = SDL_CreateWindow("SDL Wii U Render Demo", 640, 480, 0);
    if (!state->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    state->renderer = SDL_CreateRenderer(state->window, nullptr);
    if (!state->renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(state->window);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    if (!SDL_SetRenderDrawBlendMode(state->renderer, SDL_BLENDMODE_BLEND)) {
        SDL_Log("SDL_SetRenderDrawBlendMode failed: %s", SDL_GetError());
        SDL_DestroyRenderer(state->renderer);
        SDL_DestroyWindow(state->window);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    if (!InitAudioDemo(state)) {
        SDL_DestroyRenderer(state->renderer);
        SDL_DestroyWindow(state->window);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    if (!InitStorageDemo(state)) {
        ShutdownAudioDemo(state);
        SDL_DestroyRenderer(state->renderer);
        SDL_DestroyWindow(state->window);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    if (!InitTimeDemo(state)) {
        ShutdownTimeDemo(state);
        ShutdownAudioDemo(state);
        SDL_DestroyRenderer(state->renderer);
        SDL_DestroyWindow(state->window);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    if (!InitCameraDemo(state)) {
        ShutdownTimeDemo(state);
        ShutdownAudioDemo(state);
        SDL_DestroyRenderer(state->renderer);
        SDL_DestroyWindow(state->window);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    OpenInitialInputDevices(state);
    SDL_Log("AppInit complete");
    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    AppState *state = static_cast<AppState *>(appstate);

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.key == SDLK_ESCAPE) {
            return SDL_APP_SUCCESS;
        }

        if (event->key.scancode == SDL_SCANCODE_P) {
            (void)PersistStorageCounter(&state->storage, state->storage.save_count + 1);
            return SDL_APP_CONTINUE;
        }

        if (event->key.scancode == SDL_SCANCODE_R) {
            (void)PersistStorageCounter(&state->storage, 0);
            return SDL_APP_CONTINUE;
        }

        if (event->key.scancode == SDL_SCANCODE_C) {
            (void)ToggleCameraDemo(state);
            return SDL_APP_CONTINUE;
        }
    }

    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        SDL_Gamepad *gamepad = GetActiveGamepad(state);
        if (gamepad && state->active_gamepad_id == event->gbutton.which) {
            if (event->gbutton.button == SDL_GAMEPAD_BUTTON_NORTH) {
                (void)PersistStorageCounter(&state->storage, state->storage.save_count + 1);
                return SDL_APP_CONTINUE;
            }

            if (event->gbutton.button == SDL_GAMEPAD_BUTTON_WEST) {
                (void)PersistStorageCounter(&state->storage, 0);
                return SDL_APP_CONTINUE;
            }

            if (event->gbutton.button == SDL_GAMEPAD_BUTTON_EAST) {
                (void)ToggleCameraDemo(state);
                return SDL_APP_CONTINUE;
            }
        }
    }

    if (HandleCameraDemoEvent(state, event)) {
        return SDL_APP_CONTINUE;
    }

    if (HandleInputDemoEvent(state, event)) {
        return SDL_APP_CONTINUE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *state = static_cast<AppState *>(appstate);

    if (!state || !state->window || !state->renderer) {
        return SDL_APP_FAILURE;
    }

    PollInputDemo(state);
    UpdateAudioDemo(state);
    UpdateCameraDemo(state);
    UpdateTimeDemo(state);
    return RenderFrame(state) ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    AppState *state = static_cast<AppState *>(appstate);
    (void)result;

    if (state) {
        ShutdownInputDemo(state);
        ShutdownCameraDemo(state);
        ShutdownTimeDemo(state);
        ShutdownAudioDemo(state);
        DestroySceneResources(state);
        if (state->renderer) {
            SDL_DestroyRenderer(state->renderer);
        }
        if (state->window) {
            SDL_DestroyWindow(state->window);
        }
        SDL_free(state);
    }
}
