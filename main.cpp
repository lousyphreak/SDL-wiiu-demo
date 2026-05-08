#define SDL_MAIN_USE_CALLBACKS 1
#include <cmath>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

struct AppState
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *background;
    int background_width;
    int background_height;
    bool first_frame_complete;
    unsigned int frame_count;
};

static void LogRendererCenterPixel(AppState *state, unsigned int frame_number, const char *stage)
{
    SDL_Rect rect;
    SDL_Surface *surface;
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 0;

    if (!state || !state->renderer || state->background_width <= 0 || state->background_height <= 0) {
        return;
    }

    rect.x = state->background_width / 2;
    rect.y = state->background_height / 2;
    rect.w = 1;
    rect.h = 1;

    surface = SDL_RenderReadPixels(state->renderer, &rect);
    if (!surface) {
        SDL_Log("Frame %u: %s readback failed: %s", frame_number, stage, SDL_GetError());
        return;
    }

    if (SDL_ReadSurfacePixel(surface, 0, 0, &r, &g, &b, &a)) {
        SDL_Log("Frame %u: %s center pixel is rgba(%u,%u,%u,%u) format=%s",
                frame_number, stage, r, g, b, a, SDL_GetPixelFormatName(surface->format));
    } else {
        SDL_Log("Frame %u: %s readback pixel decode failed: %s", frame_number, stage, SDL_GetError());
    }

    SDL_DestroySurface(surface);
}

static bool RebuildBackgroundTexture(AppState *state)
{
    int width = 0;
    int height = 0;
    const int horizon_ratio_numerator = 3;
    const int horizon_ratio_denominator = 5;

    if (!SDL_GetRenderOutputSize(state->renderer, &width, &height)) {
        SDL_Log("SDL_GetRenderOutputSize failed: %s", SDL_GetError());
        return false;
    }

    if (state->frame_count <= 3) {
        SDL_Log("Frame %u: output size is %dx%d", state->frame_count, width, height);
    }

    if (state->background && state->background_width == width && state->background_height == height) {
        return true;
    }

    if (state->background) {
        SDL_DestroyTexture(state->background);
        state->background = nullptr;
    }

    state->background = SDL_CreateTexture(state->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
    if (!state->background) {
        SDL_Log("SDL_CreateTexture(background) failed: %s", SDL_GetError());
        return false;
    }

    if (state->frame_count <= 3) {
        SDL_Log("Frame %u: created background target texture", state->frame_count);
    }

    state->background_width = width;
    state->background_height = height;

    if (!SDL_SetRenderTarget(state->renderer, state->background)) {
        SDL_Log("SDL_SetRenderTarget(background) failed: %s", SDL_GetError());
        return false;
    }

    if (state->frame_count <= 3) {
        SDL_Log("Frame %u: bound background target", state->frame_count);
    }

    if (!SDL_SetRenderDrawColor(state->renderer, 5, 8, 20, 255) ||
        !SDL_RenderClear(state->renderer)) {
        SDL_Log("Failed to clear cached background: %s", SDL_GetError());
        return false;
    }

    const int horizon = (height * horizon_ratio_numerator) / horizon_ratio_denominator;
    for (int i = 0; i < 16; ++i) {
        const int x0 = (i * width) / 16;
        const int x1 = ((i + 1) * width) / 16;
        const Uint8 r = (Uint8)(10 + i * 5);
        const Uint8 g = (Uint8)(24 + i * 4);
        const Uint8 b = (Uint8)(90 + i * 6);
        const SDL_FRect rect = {
            (float)x0,
            0.0f,
            (float)(x1 - x0 + 1),
            (float)horizon
        };

        if (!SDL_SetRenderDrawColor(state->renderer, r, g, b, 255) ||
            !SDL_RenderFillRect(state->renderer, &rect)) {
            SDL_Log("Failed to render sky band: %s", SDL_GetError());
            return false;
        }
    }

    if (!SDL_SetRenderDrawColor(state->renderer, 6, 5, 18, 255)) {
        SDL_Log("Failed to set floor color: %s", SDL_GetError());
        return false;
    }

    const SDL_FRect floor_rect = { 0.0f, (float)horizon, (float)width, (float)(height - horizon) };
    if (!SDL_RenderFillRect(state->renderer, &floor_rect)) {
        SDL_Log("Failed to render floor background: %s", SDL_GetError());
        return false;
    }

    if (!SDL_SetRenderDrawColor(state->renderer, 40, 190, 255, 110)) {
        SDL_Log("Failed to set grid color: %s", SDL_GetError());
        return false;
    }

    for (int y = horizon; y < height; y += 14) {
        const SDL_FRect rect = { 0.0f, (float)y, (float)width, 1.5f };
        if (!SDL_RenderFillRect(state->renderer, &rect)) {
            SDL_Log("Failed to render grid line: %s", SDL_GetError());
            return false;
        }
    }

    SDL_FPoint rays[14];
    for (int i = -6; i <= 6; ++i) {
        rays[i + 6].x = (float)((width / 2) + i * 56);
        rays[i + 6].y = (float)height;
    }

    if (!SDL_RenderLines(state->renderer, rays, 13)) {
        SDL_Log("Failed to render perspective grid: %s", SDL_GetError());
        return false;
    }

    if (state->frame_count <= 2) {
        LogRendererCenterPixel(state, state->frame_count, "background-target");
    }

    if (!SDL_SetRenderTarget(state->renderer, nullptr)) {
        SDL_Log("SDL_SetRenderTarget(reset) failed: %s", SDL_GetError());
        return false;
    }

    if (state->frame_count <= 3) {
        SDL_Log("Frame %u: restored main render target", state->frame_count);
    }

    return true;
}

static bool RenderFrame(AppState *state)
{
    const double time = (double)SDL_GetTicks() / 1000.0;
    const double tau = 6.283185307179586;
    const int width = state->background_width;
    const int height = state->background_height;
    const int horizon = (height * 3) / 5;
    const unsigned int frame_number = ++state->frame_count;

    if (frame_number <= 3) {
        SDL_Log("Frame %u: entering RenderFrame", frame_number);
    }

    if (!RebuildBackgroundTexture(state)) {
        return false;
    }

    if (frame_number <= 3) {
        SDL_Log("Frame %u: background texture ready", frame_number);
    }

    if (!SDL_SetRenderDrawColor(state->renderer, 128, 0, 128, 255) ||
        !SDL_RenderClear(state->renderer) ||
        !SDL_RenderTexture(state->renderer, state->background, nullptr, nullptr)) {
        SDL_Log("Failed to start frame: %s", SDL_GetError());
        return false;
    }

    if (frame_number <= 3) {
        SDL_Log("Frame %u: drew cached background", frame_number);
    }

    if (frame_number <= 2) {
        LogRendererCenterPixel(state, frame_number, "pre-present");
    }

    for (int i = 0; i < 10; ++i) {
        const int radius = 112 - i * 8;
        const SDL_FRect rect = {
            (float)((width / 2) - radius),
            (float)(horizon - 104 + i * 12 + (int)(std::sin(time * 3.0 + i) * 3.0)),
            (float)(radius * 2),
            6.0f
        };

        if (!SDL_SetRenderDrawColor(state->renderer,
                                    (Uint8)(255 - i * 10),
                                    (Uint8)(120 + i * 8),
                                    (Uint8)(80 + i * 14),
                                    255) ||
            !SDL_RenderFillRect(state->renderer, &rect)) {
            SDL_Log("Failed to render sun band: %s", SDL_GetError());
            return false;
        }
    }

    SDL_FPoint rays[13];
    for (int i = -6; i <= 6; ++i) {
        rays[i + 6].x = (float)((width / 2) + i * 46 + (int)(std::sin(time + i) * 6.0));
        rays[i + 6].y = (float)height;
    }

    if (!SDL_SetRenderDrawColor(state->renderer, 40, 190, 255, 220) ||
        !SDL_RenderLines(state->renderer, rays, 13)) {
        SDL_Log("Failed to render animated rays: %s", SDL_GetError());
        return false;
    }

    for (int i = 0; i < 6; ++i) {
        const double angle = time * 1.2 + (tau * i) / 6.0;
        const int size = 12 + i * 4;
        const SDL_FRect rect = {
            (float)((width / 2) + (int)(std::cos(angle * 1.1) * 180.0) - size / 2),
            (float)(horizon - 24 + (int)(std::sin(angle * 1.7) * 70.0) - size / 2),
            (float)size,
            (float)size
        };

        if (!SDL_SetRenderDrawColor(state->renderer,
                                    (Uint8)(255 - i * 12),
                                    (Uint8)(220 - i * 20),
                                    (Uint8)(120 + i * 18),
                                    255) ||
            !SDL_RenderFillRect(state->renderer, &rect)) {
            SDL_Log("Failed to render orbit block: %s", SDL_GetError());
            return false;
        }
    }

    for (int i = 0; i < 24; ++i) {
        const int size = 2 + (i & 1);
        const int x = (i * 97) % width;
        const int y = 12 + ((i * 53) % SDL_max(horizon - 24, 1));
        const Uint8 glow = (Uint8)(150 + 105 * (0.5 + 0.5 * std::sin(time * 4.0 + i)));
        const SDL_FRect rect = { (float)x, (float)y, (float)size, (float)size };

        if (!SDL_SetRenderDrawColor(state->renderer, glow, glow, 255, 255) ||
            !SDL_RenderFillRect(state->renderer, &rect)) {
            SDL_Log("Failed to render star: %s", SDL_GetError());
            return false;
        }
    }

    for (int i = 0; i < 5; ++i) {
        const int streak_width = 72 - i * 10;
        const int x = (int)std::fmod(time * 130.0 + i * 95.0, (double)SDL_max(width - streak_width, 1));
        const int y = 40 + i * 24 + (int)(std::sin(time * 2.4 + i) * 12.0);
        const SDL_FRect rect = { (float)x, (float)y, (float)streak_width, 4.0f };

        if (!SDL_SetRenderDrawColor(state->renderer,
                                    (Uint8)(255 - i * 20),
                                    (Uint8)(255 - i * 10),
                                    180,
                                    255) ||
            !SDL_RenderFillRect(state->renderer, &rect)) {
            SDL_Log("Failed to render streak: %s", SDL_GetError());
            return false;
        }
    }

    if (frame_number <= 3) {
        SDL_Log("Frame %u: before present", frame_number);
    }

    if (!SDL_RenderPresent(state->renderer)) {
        SDL_Log("SDL_RenderPresent failed: %s", SDL_GetError());
        return false;
    }

    if (frame_number <= 3) {
        SDL_Log("Frame %u: present completed", frame_number);
    }

    if (!state->first_frame_complete) {
        state->first_frame_complete = true;
    }

    return true;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    AppState *state;
    (void)argc;
    (void)argv;

    SDL_SetAppMetadata("SDL Wii U Render Demo", "1.0", "org.wiiu.sdl-demo");
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

    SDL_Log("AppInit complete");
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

    if (!state || !state->window || !state->renderer) {
        return SDL_APP_FAILURE;
    }

    return RenderFrame(state) ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    AppState *state = (AppState *)appstate;
    (void)result;

    if (state) {
        if (state->background) {
            SDL_DestroyTexture(state->background);
        }
        if (state->renderer) {
            SDL_DestroyRenderer(state->renderer);
        }
        if (state->window) {
            SDL_DestroyWindow(state->window);
        }
        SDL_free(state);
    }
}
