#define SDL_MAIN_USE_CALLBACKS 1
#include <cmath>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

struct AppState
{
    SDL_Window *window;
};

static bool FillRect(SDL_Surface *surface, const SDL_Rect *rect, Uint8 r, Uint8 g, Uint8 b)
{
    Uint32 color = SDL_MapSurfaceRGBA(surface, r, g, b, 0xFF);

    if (!SDL_FillSurfaceRect(surface, rect, color)) {
        SDL_Log("SDL_FillSurfaceRect failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

static bool UpdateWindowColor(SDL_Window *window)
{
    SDL_Surface *surface = SDL_GetWindowSurface(window);
    const double time = (double)SDL_GetTicks() / 1000.0;
    const double tau = 6.283185307179586;
    SDL_Rect rect;

    if (!surface) {
        SDL_Log("SDL_GetWindowSurface failed: %s", SDL_GetError());
        return false;
    }

    if (!FillRect(surface, NULL, 5, 8, 20)) {
        return false;
    }

    const int width = surface->w;
    const int height = surface->h;
    const int horizon = (height * 3) / 5;

    for (int i = 0; i < 16; ++i) {
        const int x0 = (i * width) / 16;
        const int x1 = ((i + 1) * width) / 16;
        const double wave = 0.5 + 0.5 * std::sin(time * 1.6 + i * 0.55);
        rect = { x0, 0, x1 - x0 + 1, horizon };
        if (!FillRect(surface, &rect,
                      (Uint8)(10 + i * 5),
                      (Uint8)(20 + 80 * wave),
                      (Uint8)(70 + 130 * (1.0 - wave)))) {
            return false;
        }
    }

    for (int i = 0; i < 10; ++i) {
        const int radius = 108 - i * 8;
        rect = {
            (width / 2) - radius,
            horizon - 100 + i * 12 + (int)(std::sin(time * 3.0 + i) * 3.0),
            radius * 2,
            6
        };
        if (!FillRect(surface, &rect,
                      (Uint8)(255 - i * 10),
                      (Uint8)(120 + i * 8),
                      (Uint8)(80 + i * 14))) {
            return false;
        }
    }

    for (int y = horizon; y < height; y += 14) {
        rect = { 0, y, width, 2 };
        if (!FillRect(surface, &rect, 40, 190, 255)) {
            return false;
        }
    }

    for (int i = -6; i <= 6; ++i) {
        const int x = (width / 2) + i * 46 + (int)(std::sin(time + i) * 6.0);
        rect = { x, horizon, 2, height - horizon };
        if (!FillRect(surface, &rect, 40, 190, 255)) {
            return false;
        }
    }

    for (int i = 0; i < 6; ++i) {
        const double angle = time * 1.2 + (tau * i) / 6.0;
        const int size = 12 + i * 4;
        rect = {
            (width / 2) + (int)(std::cos(angle * 1.1) * 180.0) - size / 2,
            horizon - 24 + (int)(std::sin(angle * 1.7) * 70.0) - size / 2,
            size,
            size
        };
        if (!FillRect(surface, &rect,
                      (Uint8)(255 - i * 12),
                      (Uint8)(220 - i * 20),
                      (Uint8)(120 + i * 18))) {
            return false;
        }
    }

    for (int i = 0; i < 24; ++i) {
        const int size = 2 + (i & 1);
        const int x = (i * 97) % width;
        const int y = 12 + ((i * 53) % (horizon - 24));
        const Uint8 glow = (Uint8)(150 + 105 * (0.5 + 0.5 * std::sin(time * 4.0 + i)));
        rect = { x, y, size, size };
        if (!FillRect(surface, &rect, glow, glow, 255)) {
            return false;
        }
    }

    for (int i = 0; i < 5; ++i) {
        const int streakWidth = 72 - i * 10;
        const int x = (int)std::fmod(time * 130.0 + i * 95.0, (double)(width - streakWidth));
        const int y = 40 + i * 24 + (int)(std::sin(time * 2.4 + i) * 12.0);
        rect = { x, y, streakWidth, 4 };
        if (!FillRect(surface, &rect,
                      (Uint8)(255 - i * 20),
                      (Uint8)(255 - i * 10),
                      180)) {
            return false;
        }
    }

    if (!SDL_UpdateWindowSurface(window)) {
        SDL_Log("SDL_UpdateWindowSurface failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    AppState *state;
    (void)argc;
    (void)argv;

    SDL_SetAppMetadata("SDL Wii U Minimal Demo", "1.0", "org.wiiu.sdl-demo");
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state = (AppState *)SDL_calloc(1, sizeof(*state));
    if (!state) {
        SDL_Log("Out of memory");
        return SDL_APP_FAILURE;
    }

    state->window = SDL_CreateWindow("SDL Wii U Minimal Demo", 640, 480, 0);
    if (!state->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *state = (AppState *)appstate;

    if (!state || !state->window) {
        return SDL_APP_FAILURE;
    }

    return UpdateWindowColor(state->window) ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    AppState *state = (AppState *)appstate;
    (void)result;

    if (state) {
        if (state->window) {
            SDL_DestroyWindow(state->window);
        }
        SDL_free(state);
    }
}
