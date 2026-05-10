#include "scene_demo.h"

#include <cmath>

#include "input_demo.h"
#include "render_utils.h"

namespace
{

bool RenderMouseCursor(AppState *state, int width, int height)
{
    float mouse_x;
    float mouse_y;
    SDL_MouseButtonFlags buttons;

    if (!state || !SDL_HasMouse() || SDL_GetMouseFocus() != state->window || !SDL_CursorVisible()) {
        return true;
    }

    buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    mouse_x = SDL_clamp(mouse_x, 0.0f, (float)(width - 1));
    mouse_y = SDL_clamp(mouse_y, 0.0f, (float)(height - 1));

    if (!SetColor(state->renderer, 0, 0, 0, 200) ||
        !SDL_RenderLine(state->renderer, mouse_x + 1.0f, mouse_y - 10.0f, mouse_x + 1.0f, mouse_y + 10.0f) ||
        !SDL_RenderLine(state->renderer, mouse_x - 10.0f, mouse_y + 1.0f, mouse_x + 10.0f, mouse_y + 1.0f)) {
        return false;
    }

    if ((buttons & SDL_BUTTON_LMASK) != 0) {
        if (!SetColor(state->renderer, 255, 200, 80, 255)) {
            return false;
        }
    } else if ((buttons & SDL_BUTTON_RMASK) != 0) {
        if (!SetColor(state->renderer, 255, 120, 200, 255)) {
            return false;
        }
    } else if ((buttons & SDL_BUTTON_MMASK) != 0) {
        if (!SetColor(state->renderer, 120, 255, 180, 255)) {
            return false;
        }
    } else {
        if (!SetColor(state->renderer, 255, 255, 255, 255)) {
            return false;
        }
    }

    if (!SDL_RenderLine(state->renderer, mouse_x, mouse_y - 10.0f, mouse_x, mouse_y + 10.0f) ||
        !SDL_RenderLine(state->renderer, mouse_x - 10.0f, mouse_y, mouse_x + 10.0f, mouse_y) ||
        !FillCircle(state->renderer, mouse_x, mouse_y, 2.5f)) {
        return false;
    }

    if ((buttons & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK | SDL_BUTTON_MMASK)) != 0) {
        if (!SetColor(state->renderer, 255, 255, 255, 180) ||
            !FillCircle(state->renderer, mouse_x, mouse_y, 6.0f)) {
            return false;
        }
    }

    return true;
}

bool RebuildBackgroundTexture(AppState *state)
{
    int width = 0;
    int height = 0;
    const int horizon_ratio_numerator = 3;
    const int horizon_ratio_denominator = 5;

    if (!SDL_GetRenderOutputSize(state->renderer, &width, &height)) {
        SDL_Log("SDL_GetRenderOutputSize failed: %s", SDL_GetError());
        return false;
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

    state->background_width = width;
    state->background_height = height;

    if (!SDL_SetRenderTarget(state->renderer, state->background)) {
        SDL_Log("SDL_SetRenderTarget(background) failed: %s", SDL_GetError());
        return false;
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

    if (!SDL_SetRenderTarget(state->renderer, nullptr)) {
        SDL_Log("SDL_SetRenderTarget(reset) failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

} // namespace

void DestroySceneResources(AppState *state)
{
    if (!state || !state->background) {
        return;
    }

    SDL_DestroyTexture(state->background);
    state->background = nullptr;
    state->background_width = 0;
    state->background_height = 0;
}

bool RenderFrame(AppState *state)
{
    SDL_Gamepad *active_gamepad = GetActiveGamepad(state);
    const double time = (double)SDL_GetTicks() / 1000.0;
    const double tau = 6.283185307179586;
    const bool has_gamepad = active_gamepad && SDL_GamepadConnected(active_gamepad);
    const float left_x = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_LEFTX)) : 0.0f;
    const float left_y = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_LEFTY)) : 0.0f;
    const float right_x = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_RIGHTX)) : 0.0f;
    const float right_y = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_RIGHTY)) : 0.0f;
    const float left_trigger = has_gamepad ? SDL_max(0.0f, AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER))) : 0.0f;
    const float right_trigger = has_gamepad ? SDL_max(0.0f, AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))) : 0.0f;
    int width;
    int height;
    int horizon;
    int scene_center_x;
    int scene_center_y;
    float scene_energy;

    if (!RebuildBackgroundTexture(state)) {
        return false;
    }

    width = state->background_width;
    height = state->background_height;
    horizon = (height * 3) / 5;
    scene_center_x = (width / 2) + (int)(left_x * 110.0f);
    scene_center_y = horizon - 24 + (int)(left_y * 70.0f);
    scene_energy = 1.0f + left_trigger + right_trigger;

    if (!SDL_SetRenderDrawColor(state->renderer, 128, 0, 128, 255) ||
        !SDL_RenderClear(state->renderer) ||
        !SDL_RenderTexture(state->renderer, state->background, nullptr, nullptr)) {
        SDL_Log("Failed to start frame: %s", SDL_GetError());
        return false;
    }

    for (int i = 0; i < 10; ++i) {
        const int radius = (int)((112 - i * 8) * scene_energy);
        const SDL_FRect rect = {
            (float)(scene_center_x - radius),
            (float)(scene_center_y - 80 + i * 12 + (int)(std::sin(time * 3.0 + i + right_y) * 3.0)),
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
        rays[i + 6].x = (float)(scene_center_x + i * 46 + (int)(std::sin(time + i + right_x * 2.0f) * 6.0 * scene_energy));
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
            (float)(scene_center_x + (int)(std::cos(angle * 1.1 + right_x) * 180.0) - size / 2),
            (float)(scene_center_y + (int)(std::sin(angle * 1.7 + right_y) * 70.0) - size / 2),
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
        const int x = (int)std::fmod(time * (130.0 + right_trigger * 90.0) + i * 95.0, (double)SDL_max(width - streak_width, 1));
        const int y = 40 + i * 24 + (int)(std::sin(time * 2.4 + i + left_x) * 12.0);
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

    if (!RenderInputOverlay(state, width, height)) {
        SDL_Log("Failed to render input overlay: %s", SDL_GetError());
        return false;
    }

    if (!RenderMouseCursor(state, width, height)) {
        SDL_Log("Failed to render mouse cursor: %s", SDL_GetError());
        return false;
    }

    if (!SDL_RenderPresent(state->renderer)) {
        SDL_Log("SDL_RenderPresent failed: %s", SDL_GetError());
        return false;
    }

    if (!state->first_frame_complete) {
        state->first_frame_complete = true;
    }

    return true;
}
