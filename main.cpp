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
    SDL_Gamepad *gamepad;
    SDL_JoystickID gamepad_id;
    SDL_PowerState battery_state;
    int battery_percent;
    bool touch_down;
    float touch_x;
    float touch_y;
    float touch_pressure;
};

static float AxisToFloat(Sint16 value)
{
    return (value >= 0) ? ((float)value / 32767.0f) : ((float)value / 32768.0f);
}

static bool SetColor(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    return SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

static bool FillCircle(SDL_Renderer *renderer, float center_x, float center_y, float radius)
{
    const int limit = (int)SDL_ceilf(radius);

    for (int y = -limit; y <= limit; ++y) {
        const float dy = (float)y;
        const float dx = SDL_sqrtf(SDL_max(0.0f, (radius * radius) - (dy * dy)));
        const SDL_FRect span = {
            center_x - dx,
            center_y + dy,
            (dx * 2.0f) + 1.0f,
            1.0f
        };

        if (!SDL_RenderFillRect(renderer, &span)) {
            return false;
        }
    }

    return true;
}

static void CloseGamepad(AppState *state)
{
    if (state->gamepad) {
        SDL_CloseGamepad(state->gamepad);
        state->gamepad = nullptr;
    }
    state->gamepad_id = 0;
    state->battery_state = SDL_POWERSTATE_UNKNOWN;
    state->battery_percent = -1;
    state->touch_down = false;
    state->touch_x = 0.0f;
    state->touch_y = 0.0f;
    state->touch_pressure = 0.0f;
}

static void OpenGamepad(AppState *state, SDL_JoystickID gamepad_id)
{
    if (!state || state->gamepad_id == gamepad_id) {
        return;
    }

    CloseGamepad(state);

    state->gamepad = SDL_OpenGamepad(gamepad_id);
    if (!state->gamepad) {
        SDL_Log("SDL_OpenGamepad failed for %" SDL_PRIu32 ": %s", gamepad_id, SDL_GetError());
        return;
    }

    state->gamepad_id = SDL_GetGamepadID(state->gamepad);
    SDL_Log("Opened gamepad: %s", SDL_GetGamepadName(state->gamepad));
}

static void OpenInitialGamepad(AppState *state)
{
    int count = 0;
    SDL_JoystickID *gamepads = SDL_GetGamepads(&count);

    if (gamepads) {
        if (count > 0) {
            OpenGamepad(state, gamepads[0]);
        }
        SDL_free(gamepads);
    }
}

static void PollGamepadState(AppState *state)
{
    if (!state->gamepad || !SDL_GamepadConnected(state->gamepad)) {
        CloseGamepad(state);
        return;
    }

    state->battery_state = SDL_GetGamepadPowerInfo(state->gamepad, &state->battery_percent);
    state->touch_down = false;
    state->touch_x = 0.0f;
    state->touch_y = 0.0f;
    state->touch_pressure = 0.0f;

    if (SDL_GetNumGamepadTouchpads(state->gamepad) > 0 &&
        SDL_GetNumGamepadTouchpadFingers(state->gamepad, 0) > 0) {
        SDL_GetGamepadTouchpadFinger(state->gamepad, 0, 0,
                                     &state->touch_down,
                                     &state->touch_x,
                                     &state->touch_y,
                                     &state->touch_pressure);
    }
}

static bool RenderInputOverlay(AppState *state, int width, int height)
{
    const float panel_x = 24.0f;
    const float panel_y = 24.0f;
    const float panel_w = 260.0f;
    const float panel_h = 220.0f;
    const float touch_w = 260.0f;
    const float touch_h = touch_w * (480.0f / 854.0f);
    const float touch_x = (float)width - touch_w - 24.0f;
    const float touch_y = 24.0f;
    const SDL_FRect panel = { panel_x, panel_y, panel_w, panel_h };
    const SDL_FRect touch_rect = { touch_x, touch_y, touch_w, touch_h };
    const bool has_gamepad = state->gamepad && SDL_GamepadConnected(state->gamepad);
    const float left_x = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(state->gamepad, SDL_GAMEPAD_AXIS_LEFTX)) : 0.0f;
    const float left_y = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(state->gamepad, SDL_GAMEPAD_AXIS_LEFTY)) : 0.0f;
    const float right_x = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(state->gamepad, SDL_GAMEPAD_AXIS_RIGHTX)) : 0.0f;
    const float right_y = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(state->gamepad, SDL_GAMEPAD_AXIS_RIGHTY)) : 0.0f;
    const float left_trigger = has_gamepad ? SDL_max(0.0f, AxisToFloat(SDL_GetGamepadAxis(state->gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER))) : 0.0f;
    const float right_trigger = has_gamepad ? SDL_max(0.0f, AxisToFloat(SDL_GetGamepadAxis(state->gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))) : 0.0f;
    const float stick_box = 64.0f;
    const float stick_radius = 6.0f;
    const float face_radius = 12.0f;
    const int battery_percent = (state->battery_percent >= 0) ? state->battery_percent : 0;
    const float battery_fill = SDL_clamp((float)battery_percent / 100.0f, 0.0f, 1.0f);
    const SDL_FRect battery_back = { panel_x + 16.0f, panel_y + 16.0f, panel_w - 32.0f, 12.0f };
    const SDL_FRect battery_fill_rect = { battery_back.x, battery_back.y, battery_back.w * battery_fill, battery_back.h };
    const SDL_FRect left_trigger_rect = { panel_x + 16.0f, panel_y + 40.0f, 96.0f, 16.0f };
    const SDL_FRect right_trigger_rect = { panel_x + panel_w - 112.0f, panel_y + 40.0f, 96.0f, 16.0f };
    const SDL_FRect left_trigger_fill = { left_trigger_rect.x, left_trigger_rect.y, left_trigger_rect.w * left_trigger, left_trigger_rect.h };
    const SDL_FRect right_trigger_fill = { right_trigger_rect.x, right_trigger_rect.y, right_trigger_rect.w * right_trigger, right_trigger_rect.h };
    const SDL_FRect left_shoulder_rect = { panel_x + 16.0f, panel_y + 64.0f, 96.0f, 12.0f };
    const SDL_FRect right_shoulder_rect = { panel_x + panel_w - 112.0f, panel_y + 64.0f, 96.0f, 12.0f };
    const SDL_FRect left_stick_rect = { panel_x + 24.0f, panel_y + 112.0f, stick_box, stick_box };
    const SDL_FRect right_stick_rect = { panel_x + panel_w - 24.0f - stick_box, panel_y + 112.0f, stick_box, stick_box };
    const SDL_FRect left_stick_click_rect = { left_stick_rect.x + 18.0f, left_stick_rect.y + left_stick_rect.h + 8.0f, 28.0f, 10.0f };
    const SDL_FRect right_stick_click_rect = { right_stick_rect.x + 18.0f, right_stick_rect.y + right_stick_rect.h + 8.0f, 28.0f, 10.0f };
    const SDL_FRect back_rect = { panel_x + 100.0f, panel_y + 88.0f, 20.0f, 14.0f };
    const SDL_FRect guide_rect = { panel_x + 120.0f, panel_y + 80.0f, 20.0f, 20.0f };
    const SDL_FRect start_rect = { panel_x + 144.0f, panel_y + 88.0f, 20.0f, 14.0f };
    const SDL_FRect misc_rect = { panel_x + 186.0f, panel_y + 88.0f, 20.0f, 14.0f };

    auto button = [state, has_gamepad](SDL_GamepadButton id) {
        return has_gamepad && SDL_GetGamepadButton(state->gamepad, id);
    };
    auto draw_outline = [state](const SDL_FRect &rect, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
        return SetColor(state->renderer, r, g, b, a) && SDL_RenderRect(state->renderer, &rect);
    };
    auto draw_fill = [state](const SDL_FRect &rect, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
        return SetColor(state->renderer, r, g, b, a) && SDL_RenderFillRect(state->renderer, &rect);
    };
    auto draw_face = [state, face_radius](float cx, float cy, Uint8 base_r, Uint8 base_g, Uint8 base_b, bool pressed) {
        const Uint8 r = pressed ? 255 : base_r;
        const Uint8 g = pressed ? 255 : base_g;
        const Uint8 b = pressed ? 255 : base_b;
        return SetColor(state->renderer, r, g, b, 255) &&
               FillCircle(state->renderer, cx, cy, face_radius);
    };
    const bool left_shoulder_down = button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    const bool right_shoulder_down = button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    const bool left_stick_down = button(SDL_GAMEPAD_BUTTON_LEFT_STICK);
    const bool right_stick_down = button(SDL_GAMEPAD_BUTTON_RIGHT_STICK);

    if (!draw_fill(panel, 8, 10, 24, 220) ||
        !draw_outline(panel, 40, 190, 255, 255) ||
        !draw_fill(touch_rect, 8, 10, 24, 220) ||
        !draw_outline(touch_rect, 40, 190, 255, 255)) {
        return false;
    }

    if (!draw_fill(battery_back, 24, 32, 68, 255)) {
        return false;
    }

    switch (state->battery_state) {
    case SDL_POWERSTATE_CHARGED:
        if (!draw_fill(battery_fill_rect, 90, 255, 170, 255)) {
            return false;
        }
        break;
    case SDL_POWERSTATE_CHARGING:
        if (!draw_fill(battery_fill_rect, 255, 220, 90, 255)) {
            return false;
        }
        break;
    case SDL_POWERSTATE_ON_BATTERY:
        if (!draw_fill(battery_fill_rect, 80, 240, 120, 255)) {
            return false;
        }
        break;
    default:
        break;
    }

    if (!draw_outline(battery_back, 80, 110, 180, 255) ||
        !draw_fill(left_trigger_rect, 24, 32, 68, 255) ||
        !draw_fill(right_trigger_rect, 24, 32, 68, 255) ||
        !draw_fill(left_shoulder_rect, left_shoulder_down ? 120 : 32, left_shoulder_down ? 220 : 56, 255, 255) ||
        !draw_fill(right_shoulder_rect, right_shoulder_down ? 120 : 32, right_shoulder_down ? 220 : 56, 255, 255) ||
        !draw_fill(left_trigger_fill, 255, 140, 90, 255) ||
        !draw_fill(right_trigger_fill, 255, 140, 90, 255) ||
        !draw_outline(left_trigger_rect, 80, 110, 180, 255) ||
        !draw_outline(right_trigger_rect, 80, 110, 180, 255) ||
        !draw_outline(left_shoulder_rect, 80, 110, 180, 255) ||
        !draw_outline(right_shoulder_rect, 80, 110, 180, 255)) {
        return false;
    }

    const SDL_FRect dpad_center = { panel_x + 54.0f, panel_y + 90.0f, 28.0f, 28.0f };
    const SDL_FRect dpad_up = { dpad_center.x, dpad_center.y - 22.0f, dpad_center.w, 20.0f };
    const SDL_FRect dpad_down = { dpad_center.x, dpad_center.y + dpad_center.h + 2.0f, dpad_center.w, 20.0f };
    const SDL_FRect dpad_left = { dpad_center.x - 22.0f, dpad_center.y, 20.0f, dpad_center.h };
    const SDL_FRect dpad_right = { dpad_center.x + dpad_center.w + 2.0f, dpad_center.y, 20.0f, dpad_center.h };
    const SDL_FRect dpad_rects[] = { dpad_center, dpad_up, dpad_down, dpad_left, dpad_right };
    const SDL_GamepadButton dpad_buttons[] = {
        SDL_GAMEPAD_BUTTON_INVALID,
        SDL_GAMEPAD_BUTTON_DPAD_UP,
        SDL_GAMEPAD_BUTTON_DPAD_DOWN,
        SDL_GAMEPAD_BUTTON_DPAD_LEFT,
        SDL_GAMEPAD_BUTTON_DPAD_RIGHT
    };

    for (int i = 0; i < SDL_arraysize(dpad_rects); ++i) {
        const bool pressed = (i == 0) ? false : button(dpad_buttons[i]);
        if (!draw_fill(dpad_rects[i], pressed ? 255 : 54, pressed ? 240 : 110, pressed ? 160 : 220, 255) ||
            !draw_outline(dpad_rects[i], 0, 0, 0, 255)) {
            return false;
        }
    }

    if (!draw_fill(back_rect, button(SDL_GAMEPAD_BUTTON_BACK) ? 255 : 70, 140, 240, 255) ||
        !draw_fill(guide_rect, button(SDL_GAMEPAD_BUTTON_GUIDE) ? 255 : 90, 220, 255, 255) ||
        !draw_fill(start_rect, button(SDL_GAMEPAD_BUTTON_START) ? 255 : 70, 140, 240, 255) ||
        !draw_fill(misc_rect, button(SDL_GAMEPAD_BUTTON_MISC1) ? 255 : 70, 220, 170, 255) ||
        !draw_outline(back_rect, 0, 0, 0, 255) ||
        !draw_outline(guide_rect, 0, 0, 0, 255) ||
        !draw_outline(start_rect, 0, 0, 0, 255) ||
        !draw_outline(misc_rect, 0, 0, 0, 255)) {
        return false;
    }

    if (!draw_outline(left_stick_rect, left_stick_down ? 255 : 80, left_stick_down ? 220 : 110, left_stick_down ? 120 : 180, 255) ||
        !draw_outline(right_stick_rect, right_stick_down ? 255 : 80, right_stick_down ? 220 : 110, right_stick_down ? 120 : 180, 255) ||
        !draw_fill(left_stick_click_rect, left_stick_down ? 255 : 24, left_stick_down ? 220 : 32, left_stick_down ? 120 : 68, 255) ||
        !draw_fill(right_stick_click_rect, right_stick_down ? 255 : 24, right_stick_down ? 220 : 32, right_stick_down ? 120 : 68, 255) ||
        !draw_outline(left_stick_click_rect, 80, 110, 180, 255) ||
        !draw_outline(right_stick_click_rect, 80, 110, 180, 255) ||
        !SetColor(state->renderer, 60, 110, 180, 255) ||
        !SDL_RenderLine(state->renderer,
                        left_stick_rect.x + left_stick_rect.w * 0.5f, left_stick_rect.y,
                        left_stick_rect.x + left_stick_rect.w * 0.5f, left_stick_rect.y + left_stick_rect.h) ||
        !SDL_RenderLine(state->renderer,
                        left_stick_rect.x, left_stick_rect.y + left_stick_rect.h * 0.5f,
                        left_stick_rect.x + left_stick_rect.w, left_stick_rect.y + left_stick_rect.h * 0.5f) ||
        !SDL_RenderLine(state->renderer,
                        right_stick_rect.x + right_stick_rect.w * 0.5f, right_stick_rect.y,
                        right_stick_rect.x + right_stick_rect.w * 0.5f, right_stick_rect.y + right_stick_rect.h) ||
        !SDL_RenderLine(state->renderer,
                        right_stick_rect.x, right_stick_rect.y + right_stick_rect.h * 0.5f,
                        right_stick_rect.x + right_stick_rect.w, right_stick_rect.y + right_stick_rect.h * 0.5f)) {
        return false;
    }

    if (!SetColor(state->renderer, left_stick_down ? 255 : 90, 240, 255, 255) ||
        !FillCircle(state->renderer,
                    left_stick_rect.x + left_stick_rect.w * 0.5f + left_x * (stick_box * 0.32f),
                    left_stick_rect.y + left_stick_rect.h * 0.5f + left_y * (stick_box * 0.32f),
                    stick_radius) ||
        !SetColor(state->renderer, right_stick_down ? 255 : 90, 240, 255, 255) ||
        !FillCircle(state->renderer,
                    right_stick_rect.x + right_stick_rect.w * 0.5f + right_x * (stick_box * 0.32f),
                    right_stick_rect.y + right_stick_rect.h * 0.5f + right_y * (stick_box * 0.32f),
                    stick_radius)) {
        return false;
    }

    if (!draw_face(panel_x + panel_w - 58.0f, panel_y + 122.0f, 120, 255, 160, button(SDL_GAMEPAD_BUTTON_SOUTH)) ||
        !draw_face(panel_x + panel_w - 92.0f, panel_y + 122.0f, 255, 230, 120, button(SDL_GAMEPAD_BUTTON_NORTH)) ||
        !draw_face(panel_x + panel_w - 75.0f, panel_y + 105.0f, 120, 200, 255, button(SDL_GAMEPAD_BUTTON_WEST)) ||
        !draw_face(panel_x + panel_w - 75.0f, panel_y + 139.0f, 255, 140, 120, button(SDL_GAMEPAD_BUTTON_EAST))) {
        return false;
    }

    if (!draw_fill(touch_rect, 10, 16, 34, 180) ||
        !draw_outline(touch_rect, 80, 110, 180, 255)) {
        return false;
    }

    if (state->touch_down) {
        const float px = touch_rect.x + state->touch_x * touch_rect.w;
        const float py = touch_rect.y + state->touch_y * touch_rect.h;
        const float radius = 6.0f + state->touch_pressure * 12.0f;

        if (!SetColor(state->renderer, 255, 170, 110, 255) ||
            !FillCircle(state->renderer, px, py, radius)) {
            return false;
        }
    }

    return true;
}

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
    const unsigned int frame_number = ++state->frame_count;
    const bool has_gamepad = state->gamepad && SDL_GamepadConnected(state->gamepad);
    const float left_x = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(state->gamepad, SDL_GAMEPAD_AXIS_LEFTX)) : 0.0f;
    const float left_y = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(state->gamepad, SDL_GAMEPAD_AXIS_LEFTY)) : 0.0f;
    const float right_x = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(state->gamepad, SDL_GAMEPAD_AXIS_RIGHTX)) : 0.0f;
    const float right_y = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(state->gamepad, SDL_GAMEPAD_AXIS_RIGHTY)) : 0.0f;
    const float left_trigger = has_gamepad ? SDL_max(0.0f, AxisToFloat(SDL_GetGamepadAxis(state->gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER))) : 0.0f;
    const float right_trigger = has_gamepad ? SDL_max(0.0f, AxisToFloat(SDL_GetGamepadAxis(state->gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))) : 0.0f;
    int width;
    int height;
    int horizon;
    int scene_center_x;
    int scene_center_y;
    float scene_energy;

    if (frame_number <= 3) {
        SDL_Log("Frame %u: entering RenderFrame", frame_number);
    }

    if (!RebuildBackgroundTexture(state)) {
        return false;
    }

    width = state->background_width;
    height = state->background_height;
    horizon = (height * 3) / 5;
    scene_center_x = (width / 2) + (int)(left_x * 110.0f);
    scene_center_y = horizon - 24 + (int)(left_y * 70.0f);
    scene_energy = 1.0f + left_trigger + right_trigger;

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

    if (frame_number <= 3) {
        SDL_Log("Frame %u: before present", frame_number);
    }

    if (!RenderInputOverlay(state, width, height)) {
        SDL_Log("Failed to render input overlay: %s", SDL_GetError());
        return false;
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

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
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

    OpenInitialGamepad(state);
    SDL_Log("AppInit complete");
    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    AppState *state = (AppState *)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        OpenGamepad(state, event->gdevice.which);
        return SDL_APP_CONTINUE;
    }

    if (event->type == SDL_EVENT_GAMEPAD_REMOVED && state->gamepad_id == event->gdevice.which) {
        CloseGamepad(state);
        return SDL_APP_CONTINUE;
    }

    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH && state->gamepad &&
            state->gamepad_id == event->gbutton.which) {
            SDL_RumbleGamepad(state->gamepad, 0x5000, 0x9000, 140);
        }
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *state = (AppState *)appstate;

    if (!state || !state->window || !state->renderer) {
        return SDL_APP_FAILURE;
    }

    PollGamepadState(state);
    return RenderFrame(state) ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    AppState *state = (AppState *)appstate;
    (void)result;

    if (state) {
        CloseGamepad(state);
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
