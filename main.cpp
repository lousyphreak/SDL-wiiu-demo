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

namespace
{

SDL_DisplayID FindSecondaryDisplay(SDL_Window *window)
{
    SDL_DisplayID *displays;
    SDL_DisplayID current_display;
    SDL_DisplayID fallback = 0;
    SDL_DisplayID preferred = 0;
    int count = 0;

    if (!window) {
        SDL_SetError("Primary demo window is unavailable");
        return 0;
    }

    displays = SDL_GetDisplays(&count);
    if (!displays) {
        return 0;
    }

    current_display = SDL_GetDisplayForWindow(window);
    for (int i = 0; i < count; ++i) {
        const SDL_DisplayID display = displays[i];
        const char *name;

        if (!display || display == current_display) {
            continue;
        }

        if (!fallback) {
            fallback = display;
        }

        name = SDL_GetDisplayName(display);
        if (name && (SDL_strstr(name, "GamePad") || SDL_strstr(name, "DRC"))) {
            preferred = display;
            break;
        }
    }

    SDL_free(displays);
    if (preferred) {
        return preferred;
    }
    if (fallback) {
        return fallback;
    }

    SDL_SetError("No secondary display is available");
    return 0;
}

void DestroyDrcWindow(AppState *state)
{
    if (!state) {
        return;
    }

    if (state->drc_renderer) {
        SDL_DestroyRenderer(state->drc_renderer);
        state->drc_renderer = nullptr;
    }
    if (state->drc_window) {
        SDL_DestroyWindow(state->drc_window);
        state->drc_window = nullptr;
    }
    state->drc_window_id = 0;

    if (state->window) {
        (void)SDL_RaiseWindow(state->window);
    }
}

bool CreateDrcWindow(AppState *state)
{
    SDL_PropertiesID props;
    SDL_DisplayID display;
    SDL_Window *window;
    SDL_Renderer *renderer;
    const char *display_name;

    if (!state) {
        return SDL_SetError("Application state is unavailable");
    }
    if (state->drc_window || state->drc_renderer) {
        return true;
    }

    display = FindSecondaryDisplay(state->window);
    if (!display) {
        return false;
    }

    props = SDL_CreateProperties();
    if (!props) {
        return false;
    }

    (void)SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "SDL Wii U DRC Demo");
    (void)SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 854);
    (void)SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 480);
    (void)SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(display));
    (void)SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(display));

    window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!window) {
        return false;
    }

    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_DestroyWindow(window);
        return false;
    }

    if (!SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND)) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return false;
    }

    state->drc_window = window;
    state->drc_renderer = renderer;
    state->drc_window_id = SDL_GetWindowID(window);
    display_name = SDL_GetDisplayName(display);
    SDL_Log("DRC demo window active on %s", display_name ? display_name : "secondary display");
    return true;
}

bool ToggleDrcWindow(AppState *state)
{
    if (!state) {
        return false;
    }

    if (state->drc_window || state->drc_renderer) {
        DestroyDrcWindow(state);
        SDL_Log("DRC demo window disabled; shared-window mirroring restored");
        return true;
    }

    return CreateDrcWindow(state);
}

} // namespace

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

    SDL_Log("[startup] InitAudioDemo begin");
    if (!InitAudioDemo(state)) {
        SDL_DestroyRenderer(state->renderer);
        SDL_DestroyWindow(state->window);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }
    SDL_Log("[startup] InitAudioDemo done");

    SDL_Log("[startup] InitStorageDemo begin");
    if (!InitStorageDemo(state)) {
        ShutdownAudioDemo(state);
        SDL_DestroyRenderer(state->renderer);
        SDL_DestroyWindow(state->window);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }
    SDL_Log("[startup] InitStorageDemo done");

    SDL_Log("[startup] InitTimeDemo begin");
    if (!InitTimeDemo(state)) {
        ShutdownStorageDemo(state);
        ShutdownTimeDemo(state);
        ShutdownAudioDemo(state);
        SDL_DestroyRenderer(state->renderer);
        SDL_DestroyWindow(state->window);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }
    SDL_Log("[startup] InitTimeDemo done");

    SDL_Log("[startup] InitCameraDemo begin");
    if (!InitCameraDemo(state)) {
        ShutdownStorageDemo(state);
        ShutdownTimeDemo(state);
        ShutdownAudioDemo(state);
        SDL_DestroyRenderer(state->renderer);
        SDL_DestroyWindow(state->window);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }
    SDL_Log("[startup] InitCameraDemo done");

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

    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
        state->drc_window_id != 0 &&
        event->window.windowID == state->drc_window_id) {
        DestroyDrcWindow(state);
        return SDL_APP_CONTINUE;
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
            if (event->gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK) {
                if (!ToggleDrcWindow(state)) {
                    SDL_Log("Failed to toggle DRC demo window: %s", SDL_GetError());
                }
                return SDL_APP_CONTINUE;
            }

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
    UpdateStorageDemo(state);
    UpdateTimeDemo(state);
    if (!RenderFrame(state)) {
        return SDL_APP_FAILURE;
    }
    if (!RenderDrcFrame(state)) {
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    AppState *state = static_cast<AppState *>(appstate);
    (void)result;

    if (state) {
        ShutdownInputDemo(state);
        ShutdownCameraDemo(state);
        ShutdownTimeDemo(state);
        ShutdownStorageDemo(state);
        ShutdownAudioDemo(state);
        DestroySceneResources(state);
        DestroyDrcWindow(state);
        if (state->renderer) {
            SDL_DestroyRenderer(state->renderer);
        }
        if (state->window) {
            SDL_DestroyWindow(state->window);
        }
        SDL_free(state);
    }
}
