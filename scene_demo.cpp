#include "scene_demo.h"

#include <cmath>

#include "camera_demo.h"
#include "input_demo.h"
#include "render_utils.h"

namespace
{

constexpr int SHOWCASE_TEXTURE_SIZE = 128;

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

bool EnsureShowcaseTexture(AppState *state)
{
    SDL_Texture *texture;

    if (state->showcase_texture) {
        return true;
    }

    texture = SDL_CreateTexture(state->renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                SHOWCASE_TEXTURE_SIZE, SHOWCASE_TEXTURE_SIZE);
    if (!texture) {
        SDL_Log("SDL_CreateTexture(showcase) failed: %s", SDL_GetError());
        return false;
    }

    if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND)) {
        SDL_Log("SDL_SetTextureBlendMode(showcase) failed: %s", SDL_GetError());
        SDL_DestroyTexture(texture);
        return false;
    }

    if (!SDL_SetRenderTarget(state->renderer, texture)) {
        SDL_Log("SDL_SetRenderTarget(showcase) failed: %s", SDL_GetError());
        SDL_DestroyTexture(texture);
        return false;
    }

    if (!SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 0) ||
        !SDL_RenderClear(state->renderer)) {
        SDL_Log("Failed to clear showcase texture: %s", SDL_GetError());
        (void)SDL_SetRenderTarget(state->renderer, nullptr);
        SDL_DestroyTexture(texture);
        return false;
    }

    const SDL_FRect left_cells[] = {
        { 8.0f, 8.0f, 50.0f, 50.0f },
        { 8.0f, 70.0f, 50.0f, 50.0f },
    };
    const SDL_FRect right_cells[] = {
        { 70.0f, 8.0f, 50.0f, 50.0f },
        { 70.0f, 70.0f, 50.0f, 50.0f },
    };
    const SDL_FRect frame = { 0.5f, 0.5f, (float)SHOWCASE_TEXTURE_SIZE - 1.0f, (float)SHOWCASE_TEXTURE_SIZE - 1.0f };
    SDL_FRect scanlines[6];

    for (int i = 0; i < SDL_arraysize(scanlines); ++i) {
        scanlines[i].x = 12.0f;
        scanlines[i].y = 16.0f + (float)(i * 18);
        scanlines[i].w = (float)(SHOWCASE_TEXTURE_SIZE - 24);
        scanlines[i].h = 3.0f;
    }

    if (!SetColor(state->renderer, 24, 52, 108, 255) ||
        !SDL_RenderFillRect(state->renderer, nullptr) ||
        !SetColor(state->renderer, 50, 220, 255, 220) ||
        !SDL_RenderFillRects(state->renderer, left_cells, SDL_arraysize(left_cells)) ||
        !SetColor(state->renderer, 255, 96, 190, 220) ||
        !SDL_RenderFillRects(state->renderer, right_cells, SDL_arraysize(right_cells)) ||
        !SetColor(state->renderer, 255, 255, 255, 36) ||
        !SDL_RenderFillRects(state->renderer, scanlines, SDL_arraysize(scanlines)) ||
        !SetColor(state->renderer, 255, 190, 110, 255) ||
        !FillCircle(state->renderer, SHOWCASE_TEXTURE_SIZE * 0.5f, SHOWCASE_TEXTURE_SIZE * 0.5f, 19.0f) ||
        !SetColor(state->renderer, 255, 255, 255, 255) ||
        !FillCircle(state->renderer, SHOWCASE_TEXTURE_SIZE * 0.5f, SHOWCASE_TEXTURE_SIZE * 0.5f, 7.0f) ||
        !SetColor(state->renderer, 255, 255, 255, 255) ||
        !SDL_RenderRect(state->renderer, &frame) ||
        !SDL_RenderLine(state->renderer, SHOWCASE_TEXTURE_SIZE * 0.5f, 10.0f,
                        SHOWCASE_TEXTURE_SIZE * 0.5f, SHOWCASE_TEXTURE_SIZE - 10.0f) ||
        !SDL_RenderLine(state->renderer, 10.0f, SHOWCASE_TEXTURE_SIZE * 0.5f,
                        SHOWCASE_TEXTURE_SIZE - 10.0f, SHOWCASE_TEXTURE_SIZE * 0.5f)) {
        SDL_Log("Failed to build showcase texture: %s", SDL_GetError());
        (void)SDL_SetRenderTarget(state->renderer, nullptr);
        SDL_DestroyTexture(texture);
        return false;
    }

    if (!SDL_SetRenderTarget(state->renderer, nullptr)) {
        SDL_Log("SDL_SetRenderTarget(showcase reset) failed: %s", SDL_GetError());
        SDL_DestroyTexture(texture);
        return false;
    }

    state->showcase_texture = texture;
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

bool RenderRendererShowcase(AppState *state, int width, int height, double time, int horizon,
                            float left_x, float left_y, float right_x, float right_y,
                            float left_trigger, float right_trigger)
{
    const float bars_x = ((float)width * 0.5f) - 114.0f;
    const float bars_baseline = (float)SDL_min(height - 128, horizon + 52);
    const float bar_width = 14.0f;
    const float bar_step = 18.0f;
    const float sprite_y = bars_baseline - 136.0f;
    const float orbit_center_x = ((float)width * 0.5f) + right_x * 18.0f;
    const float orbit_center_y = sprite_y + 34.0f + right_y * 10.0f;
    const float orbit_radius = 42.0f + (left_trigger + right_trigger) * 30.0f;
    const SDL_FRect full_src = { 0.0f, 0.0f, (float)SHOWCASE_TEXTURE_SIZE, (float)SHOWCASE_TEXTURE_SIZE };
    const SDL_FRect left_src = { 0.0f, 0.0f, SHOWCASE_TEXTURE_SIZE * 0.5f, (float)SHOWCASE_TEXTURE_SIZE };
    const SDL_FRect right_src = { SHOWCASE_TEXTURE_SIZE * 0.5f, 0.0f, SHOWCASE_TEXTURE_SIZE * 0.5f, (float)SHOWCASE_TEXTURE_SIZE };
    SDL_FRect bars[12];
    SDL_FRect caps[12];

    for (int i = 0; i < SDL_arraysize(bars); ++i) {
        const float phase = (float)(time * 4.0 + i * 0.65 + left_x * 1.7 + right_y);
        const float energy = 0.35f + 0.65f * (0.5f + 0.5f * (float)std::sin(phase));
        const float bar_height = 18.0f + energy * (44.0f + (left_trigger + right_trigger) * 36.0f);
        bars[i].x = bars_x + bar_step * (float)i;
        bars[i].y = bars_baseline - bar_height;
        bars[i].w = bar_width;
        bars[i].h = bar_height;
        caps[i].x = bars[i].x;
        caps[i].y = bars[i].y - 4.0f;
        caps[i].w = bar_width;
        caps[i].h = 2.0f;
    }

    const SDL_FRect left_copy = {
        bars_x - 124.0f + left_x * 18.0f,
        sprite_y + left_y * 10.0f,
        86.0f,
        86.0f
    };
    const SDL_FRect right_copy = {
        bars_x + bar_step * (float)SDL_arraysize(bars) + 18.0f - left_x * 18.0f,
        sprite_y - left_y * 10.0f,
        86.0f,
        86.0f
    };
    const SDL_FRect center_spin = {
        orbit_center_x - 52.0f,
        orbit_center_y - 52.0f,
        104.0f,
        104.0f
    };
    const SDL_FRect orbit_spin = {
        orbit_center_x + orbit_radius * (float)std::cos(time * 1.6 + right_x) - 34.0f,
        orbit_center_y + orbit_radius * (float)std::sin(time * 1.6 + right_y) - 34.0f,
        68.0f,
        68.0f
    };
    const SDL_FRect counter_spin = {
        orbit_center_x + orbit_radius * 0.72f * (float)std::cos(-time * 1.1 + left_y) - 28.0f,
        orbit_center_y + orbit_radius * 0.72f * (float)std::sin(-time * 1.1 + left_x) - 28.0f,
        56.0f,
        56.0f
    };

    if (!SetColor(state->renderer, 64, 240, 255, 180) ||
        !SDL_RenderFillRects(state->renderer, bars, SDL_arraysize(bars)) ||
        !SetColor(state->renderer, 255, 255, 255, 220) ||
        !SDL_RenderFillRects(state->renderer, caps, SDL_arraysize(caps)) ||
        !SDL_SetTextureColorMod(state->showcase_texture, 255, 255, 255) ||
        !SDL_SetTextureAlphaMod(state->showcase_texture, 224) ||
        !SDL_RenderTexture(state->renderer, state->showcase_texture, &left_src, &left_copy) ||
        !SDL_SetTextureColorMod(state->showcase_texture, 255, 170, 220) ||
        !SDL_SetTextureAlphaMod(state->showcase_texture, 208) ||
        !SDL_RenderTexture(state->renderer, state->showcase_texture, &right_src, &right_copy) ||
        !SDL_SetTextureColorMod(state->showcase_texture, 120, 235, 255) ||
        !SDL_SetTextureAlphaMod(state->showcase_texture, 210) ||
        !SDL_RenderTextureRotated(state->renderer, state->showcase_texture, &full_src, &center_spin,
                                  time * 45.0, nullptr, SDL_FLIP_NONE) ||
        !SDL_SetTextureColorMod(state->showcase_texture, 255, 220, 120) ||
        !SDL_SetTextureAlphaMod(state->showcase_texture, 196) ||
        !SDL_RenderTextureRotated(state->renderer, state->showcase_texture, &full_src, &orbit_spin,
                                  -time * 110.0, nullptr, SDL_FLIP_HORIZONTAL) ||
        !SDL_SetTextureColorMod(state->showcase_texture, 180, 255, 150) ||
        !SDL_SetTextureAlphaMod(state->showcase_texture, 188) ||
        !SDL_RenderTextureRotated(state->renderer, state->showcase_texture, &full_src, &counter_spin,
                                  time * 150.0, nullptr, SDL_FLIP_VERTICAL) ||
        !SDL_SetTextureColorMod(state->showcase_texture, 255, 255, 255) ||
        !SDL_SetTextureAlphaMod(state->showcase_texture, 255)) {
        SDL_Log("Failed to render renderer showcase: %s", SDL_GetError());
        return false;
    }

    if (!SetColor(state->renderer, 220, 240, 255, 255) ||
        !SDL_RenderDebugText(state->renderer, bars_x - 18.0f, sprite_y - 16.0f,
                             "copy / copyex / fillrects") ||
        !SDL_RenderDebugText(state->renderer, bars_x + 18.0f, bars_baseline + 10.0f,
                             "batched fill rects")) {
        SDL_Log("Failed to label renderer showcase: %s", SDL_GetError());
        return false;
    }

    return true;
}

} // namespace

void DestroySceneResources(AppState *state)
{
    if (!state) {
        return;
    }

    SDL_DestroyTexture(state->background);
    state->background = nullptr;
    state->background_width = 0;
    state->background_height = 0;

    SDL_DestroyTexture(state->showcase_texture);
    state->showcase_texture = nullptr;
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

    if (!EnsureShowcaseTexture(state)) {
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

    if (!RenderRendererShowcase(state, width, height, time, horizon, left_x, left_y,
                                right_x, right_y, left_trigger, right_trigger)) {
        SDL_Log("Failed to render renderer showcase: %s", SDL_GetError());
        return false;
    }

    if (!RenderCameraDemo(state, width, height, horizon)) {
        SDL_Log("Failed to render camera showcase: %s", SDL_GetError());
        return false;
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
