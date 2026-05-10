#include "input_demo.h"

#include "audio_demo.h"
#include "render_utils.h"
#include "storage_demo.h"

namespace
{

void ResetSensorSnapshot(SensorSnapshot *sensor)
{
    if (!sensor) {
        return;
    }

    SDL_zero(*sensor);
}

void ResetTrackedSensors(AppState *state)
{
    if (!state) {
        return;
    }

    ResetSensorSnapshot(&state->accel_sensor);
    ResetSensorSnapshot(&state->gyro_sensor);
    ResetSensorSnapshot(&state->accel_l_sensor);
}

void ResetRumblePreview(RumblePreview *rumble)
{
    if (!rumble) {
        return;
    }

    SDL_zero(*rumble);
}

void ResetActiveGamepadState(AppState *state)
{
    if (!state) {
        return;
    }

    state->active_gamepad_id = 0;
    state->battery_state = SDL_POWERSTATE_UNKNOWN;
    state->battery_percent = -1;
    state->touch_down = false;
    state->touch_x = 0.0f;
    state->touch_y = 0.0f;
    state->touch_pressure = 0.0f;
    ResetTrackedSensors(state);
}

void ResetActiveJoystickState(AppState *state)
{
    if (!state) {
        return;
    }

    state->active_joystick_id = 0;
}

const char *GetSensorName(SDL_SensorType type)
{
    switch (type) {
    case SDL_SENSOR_ACCEL:
        return "accel";
    case SDL_SENSOR_GYRO:
        return "gyro";
    case SDL_SENSOR_ACCEL_L:
        return "accel_l";
    default:
        return "sensor";
    }
}

void EnableSensorIfPresent(SDL_Gamepad *gamepad, SDL_SensorType type)
{
    const char *gamepad_name;

    if (!gamepad || !SDL_GamepadHasSensor(gamepad, type)) {
        return;
    }

    gamepad_name = SDL_GetGamepadName(gamepad);
    if (!SDL_GamepadSensorEnabled(gamepad, type) &&
        !SDL_SetGamepadSensorEnabled(gamepad, type, true)) {
        SDL_Log("Failed to enable %s on %s: %s",
                GetSensorName(type),
                gamepad_name ? gamepad_name : "unknown gamepad",
                SDL_GetError());
        return;
    }

    SDL_Log("Enabled %s on %s (%.1f Hz)",
            GetSensorName(type),
            gamepad_name ? gamepad_name : "unknown gamepad",
            SDL_GetGamepadSensorDataRate(gamepad, type));
}

void RefreshSensorSnapshot(SDL_Gamepad *gamepad, SDL_SensorType type, SensorSnapshot *sensor)
{
    ResetSensorSnapshot(sensor);

    if (!gamepad || !sensor || !SDL_GamepadHasSensor(gamepad, type)) {
        return;
    }

    sensor->available = true;
    sensor->enabled = SDL_GamepadSensorEnabled(gamepad, type);
    sensor->rate_hz = SDL_GetGamepadSensorDataRate(gamepad, type);

    if (sensor->enabled) {
        if (!SDL_GetGamepadSensorData(gamepad, type, sensor->data, (int)SDL_arraysize(sensor->data))) {
            sensor->enabled = false;
            SDL_zero(sensor->data);
        }
    }
}

int FindGamepadSlot(const AppState *state, SDL_JoystickID gamepad_id)
{
    if (!state || !gamepad_id) {
        return -1;
    }

    for (int i = 0; i < MAX_TRACKED_GAMEPADS; ++i) {
        if (state->gamepads[i].gamepad && state->gamepads[i].id == gamepad_id) {
            return i;
        }
    }

    return -1;
}

int FindFreeGamepadSlot(const AppState *state)
{
    if (!state) {
        return -1;
    }

    for (int i = 0; i < MAX_TRACKED_GAMEPADS; ++i) {
        if (!state->gamepads[i].gamepad) {
            return i;
        }
    }

    return -1;
}

int FindJoystickSlot(const AppState *state, SDL_JoystickID joystick_id)
{
    if (!state || !joystick_id) {
        return -1;
    }

    for (int i = 0; i < MAX_TRACKED_JOYSTICKS; ++i) {
        if (state->joysticks[i].joystick && state->joysticks[i].id == joystick_id) {
            return i;
        }
    }

    return -1;
}

int FindFreeJoystickSlot(const AppState *state)
{
    if (!state) {
        return -1;
    }

    for (int i = 0; i < MAX_TRACKED_JOYSTICKS; ++i) {
        if (!state->joysticks[i].joystick) {
            return i;
        }
    }

    return -1;
}

void SelectFallbackGamepad(AppState *state)
{
    if (!state) {
        return;
    }

    for (int i = 0; i < MAX_TRACKED_GAMEPADS; ++i) {
        if (state->gamepads[i].gamepad) {
            state->active_gamepad_id = state->gamepads[i].id;
            return;
        }
    }

    ResetActiveGamepadState(state);
}

void SelectFallbackJoystick(AppState *state)
{
    if (!state) {
        return;
    }

    for (int i = 0; i < MAX_TRACKED_JOYSTICKS; ++i) {
        if (state->joysticks[i].joystick) {
            state->active_joystick_id = state->joysticks[i].id;
            return;
        }
    }

    ResetActiveJoystickState(state);
}

SDL_Joystick *GetActiveJoystick(AppState *state)
{
    int slot;

    if (!state) {
        return nullptr;
    }

    slot = FindJoystickSlot(state, state->active_joystick_id);
    if (slot >= 0) {
        return state->joysticks[slot].joystick;
    }

    SelectFallbackJoystick(state);
    if (!state->active_joystick_id) {
        return nullptr;
    }

    slot = FindJoystickSlot(state, state->active_joystick_id);
    if (slot < 0) {
        ResetActiveJoystickState(state);
        return nullptr;
    }

    return state->joysticks[slot].joystick;
}

SDL_Gamepad *GetTrackedGamepad(AppState *state, SDL_JoystickID gamepad_id)
{
    const int slot = FindGamepadSlot(state, gamepad_id);
    return (slot >= 0) ? state->gamepads[slot].gamepad : nullptr;
}

SDL_Joystick *GetTrackedJoystick(AppState *state, SDL_JoystickID joystick_id)
{
    const int slot = FindJoystickSlot(state, joystick_id);
    return (slot >= 0) ? state->joysticks[slot].joystick : nullptr;
}

bool GamepadHasRumble(SDL_Gamepad *gamepad)
{
    return gamepad &&
           SDL_GetBooleanProperty(SDL_GetGamepadProperties(gamepad),
                                  SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN,
                                  false);
}

bool JoystickHasRumble(SDL_Joystick *joystick)
{
    return joystick &&
           SDL_GetBooleanProperty(SDL_GetJoystickProperties(joystick),
                                  SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN,
                                  false);
}

void UpdateRumblePreview(AppState *state, ActiveRumbleDeviceKind kind, SDL_JoystickID id,
                         bool result, Uint16 low, Uint16 high, Uint32 duration_ms)
{
    if (!state) {
        return;
    }

    state->rumble.kind = kind;
    state->rumble.id = id;
    state->rumble.last_result = result;
    state->rumble.low = low;
    state->rumble.high = high;
    state->rumble.active_until_ns = result ? (SDL_GetTicksNS() + (Uint64)duration_ms * SDL_NS_PER_MS) : 0;
}

void TriggerGamepadRumble(AppState *state, SDL_Gamepad *gamepad,
                          Uint16 low, Uint16 high, Uint32 duration_ms)
{
    const SDL_JoystickID id = gamepad ? SDL_GetGamepadID(gamepad) : 0;
    const bool result = gamepad && SDL_RumbleGamepad(gamepad, low, high, duration_ms);

    UpdateRumblePreview(state, ACTIVE_RUMBLE_DEVICE_GAMEPAD, id, result, low, high, duration_ms);
}

void TriggerJoystickRumble(AppState *state, SDL_Joystick *joystick,
                           Uint16 low, Uint16 high, Uint32 duration_ms)
{
    const SDL_JoystickID id = joystick ? SDL_GetJoystickID(joystick) : 0;
    const bool result = joystick && SDL_RumbleJoystick(joystick, low, high, duration_ms);

    UpdateRumblePreview(state, ACTIVE_RUMBLE_DEVICE_JOYSTICK, id, result, low, high, duration_ms);
}

void CloseGamepadSlot(AppState *state, int slot)
{
    if (!state || slot < 0 || slot >= MAX_TRACKED_GAMEPADS || !state->gamepads[slot].gamepad) {
        return;
    }

    SDL_CloseGamepad(state->gamepads[slot].gamepad);
    state->gamepads[slot].gamepad = nullptr;

    if (state->rumble.kind == ACTIVE_RUMBLE_DEVICE_GAMEPAD &&
        state->rumble.id == state->gamepads[slot].id) {
        ResetRumblePreview(&state->rumble);
    }
    if (state->active_gamepad_id == state->gamepads[slot].id) {
        state->active_gamepad_id = 0;
    }

    state->gamepads[slot].id = 0;
    SelectFallbackGamepad(state);
}

void CloseJoystickSlot(AppState *state, int slot)
{
    if (!state || slot < 0 || slot >= MAX_TRACKED_JOYSTICKS || !state->joysticks[slot].joystick) {
        return;
    }

    SDL_CloseJoystick(state->joysticks[slot].joystick);
    state->joysticks[slot].joystick = nullptr;

    if (state->rumble.kind == ACTIVE_RUMBLE_DEVICE_JOYSTICK &&
        state->rumble.id == state->joysticks[slot].id) {
        ResetRumblePreview(&state->rumble);
    }
    if (state->active_joystick_id == state->joysticks[slot].id) {
        state->active_joystick_id = 0;
    }

    state->joysticks[slot].id = 0;
    SelectFallbackJoystick(state);
}

void CloseAllGamepads(AppState *state)
{
    if (!state) {
        return;
    }

    for (int i = 0; i < MAX_TRACKED_GAMEPADS; ++i) {
        CloseGamepadSlot(state, i);
    }
}

void CloseAllJoysticks(AppState *state)
{
    if (!state) {
        return;
    }

    for (int i = 0; i < MAX_TRACKED_JOYSTICKS; ++i) {
        CloseJoystickSlot(state, i);
    }
}

void OpenGamepad(AppState *state, SDL_JoystickID gamepad_id)
{
    int slot;

    if (!state || !gamepad_id) {
        return;
    }

    slot = FindGamepadSlot(state, gamepad_id);
    if (slot >= 0) {
        state->active_gamepad_id = gamepad_id;
        return;
    }

    slot = FindFreeGamepadSlot(state);
    if (slot < 0) {
        SDL_Log("No free gamepad slots for %" SDL_PRIu32, gamepad_id);
        return;
    }

    state->gamepads[slot].gamepad = SDL_OpenGamepad(gamepad_id);
    if (!state->gamepads[slot].gamepad) {
        SDL_Log("SDL_OpenGamepad failed for %" SDL_PRIu32 ": %s", gamepad_id, SDL_GetError());
        return;
    }

    state->gamepads[slot].id = SDL_GetGamepadID(state->gamepads[slot].gamepad);
    state->active_gamepad_id = state->gamepads[slot].id;
    SDL_Log("Opened gamepad: %s", SDL_GetGamepadName(state->gamepads[slot].gamepad));
    EnableSensorIfPresent(state->gamepads[slot].gamepad, SDL_SENSOR_ACCEL);
    EnableSensorIfPresent(state->gamepads[slot].gamepad, SDL_SENSOR_GYRO);
    EnableSensorIfPresent(state->gamepads[slot].gamepad, SDL_SENSOR_ACCEL_L);
    if (state->rumble.kind == ACTIVE_RUMBLE_DEVICE_NONE) {
        state->rumble.kind = ACTIVE_RUMBLE_DEVICE_GAMEPAD;
        state->rumble.id = state->gamepads[slot].id;
    }
}

void OpenJoystick(AppState *state, SDL_JoystickID joystick_id)
{
    int slot;

    if (!state || !joystick_id || SDL_IsGamepad(joystick_id)) {
        return;
    }

    slot = FindJoystickSlot(state, joystick_id);
    if (slot >= 0) {
        state->active_joystick_id = joystick_id;
        return;
    }

    slot = FindFreeJoystickSlot(state);
    if (slot < 0) {
        SDL_Log("No free joystick slots for %" SDL_PRIu32, joystick_id);
        return;
    }

    state->joysticks[slot].joystick = SDL_OpenJoystick(joystick_id);
    if (!state->joysticks[slot].joystick) {
        SDL_Log("SDL_OpenJoystick failed for %" SDL_PRIu32 ": %s", joystick_id, SDL_GetError());
        return;
    }

    state->joysticks[slot].id = SDL_GetJoystickID(state->joysticks[slot].joystick);
    state->active_joystick_id = state->joysticks[slot].id;
    SDL_Log("Opened joystick: %s", SDL_GetJoystickName(state->joysticks[slot].joystick));
    if (state->rumble.kind == ACTIVE_RUMBLE_DEVICE_NONE) {
        state->rumble.kind = ACTIVE_RUMBLE_DEVICE_JOYSTICK;
        state->rumble.id = state->joysticks[slot].id;
    }
}

void PollGamepadState(AppState *state)
{
    SDL_Gamepad *gamepad;

    for (int i = 0; i < MAX_TRACKED_GAMEPADS; ++i) {
        if (state->gamepads[i].gamepad && !SDL_GamepadConnected(state->gamepads[i].gamepad)) {
            CloseGamepadSlot(state, i);
        }
    }

    gamepad = GetActiveGamepad(state);
    if (!gamepad || !SDL_GamepadConnected(gamepad)) {
        ResetActiveGamepadState(state);
        return;
    }

    state->battery_state = SDL_GetGamepadPowerInfo(gamepad, &state->battery_percent);
    state->touch_down = false;
    state->touch_x = 0.0f;
    state->touch_y = 0.0f;
    state->touch_pressure = 0.0f;

    if (SDL_GetNumGamepadTouchpads(gamepad) > 0 &&
        SDL_GetNumGamepadTouchpadFingers(gamepad, 0) > 0) {
        SDL_GetGamepadTouchpadFinger(gamepad, 0, 0,
                                     &state->touch_down,
                                     &state->touch_x,
                                     &state->touch_y,
                                     &state->touch_pressure);
    }

    RefreshSensorSnapshot(gamepad, SDL_SENSOR_ACCEL, &state->accel_sensor);
    RefreshSensorSnapshot(gamepad, SDL_SENSOR_GYRO, &state->gyro_sensor);
    RefreshSensorSnapshot(gamepad, SDL_SENSOR_ACCEL_L, &state->accel_l_sensor);
}

void PollJoystickState(AppState *state)
{
    for (int i = 0; i < MAX_TRACKED_JOYSTICKS; ++i) {
        if (state->joysticks[i].joystick && !SDL_JoystickConnected(state->joysticks[i].joystick)) {
            CloseJoystickSlot(state, i);
        }
    }

    if (state->rumble.active_until_ns && SDL_GetTicksNS() >= state->rumble.active_until_ns) {
        state->rumble.active_until_ns = 0;
        state->rumble.low = 0;
        state->rumble.high = 0;
    }
}

void OpenInitialGamepads(AppState *state)
{
    int count = 0;
    SDL_JoystickID *gamepads = SDL_GetGamepads(&count);

    if (gamepads) {
        for (int i = 0; i < count; ++i) {
            OpenGamepad(state, gamepads[i]);
        }
        SDL_free(gamepads);
    }
}

void OpenInitialJoysticks(AppState *state)
{
    int count = 0;
    SDL_JoystickID *joysticks = SDL_GetJoysticks(&count);

    if (joysticks) {
        for (int i = 0; i < count; ++i) {
            OpenJoystick(state, joysticks[i]);
        }
        SDL_free(joysticks);
    }
}

bool RenderRumbleWidget(AppState *state, const SDL_FRect &rect)
{
    ActiveRumbleDeviceKind kind = state->rumble.kind;
    SDL_JoystickID id = state->rumble.id;
    SDL_Gamepad *gamepad = nullptr;
    SDL_Joystick *joystick = nullptr;
    bool supported = false;
    bool active = false;
    bool failed = false;
    float low = 0.0f;
    float high = 0.0f;
    Uint8 accent_r = 80;
    Uint8 accent_g = 110;
    Uint8 accent_b = 180;
    const float padding = 10.0f;
    const float bar_h = 14.0f;
    const float bar_gap = 8.0f;
    const float pulse_radius = 18.0f;
    const SDL_FRect low_bar = { rect.x + padding, rect.y + rect.h - padding - bar_h * 2.0f - bar_gap, rect.w - padding * 2.0f, bar_h };
    const SDL_FRect high_bar = { low_bar.x, low_bar.y + bar_h + bar_gap, low_bar.w, bar_h };

    if (kind == ACTIVE_RUMBLE_DEVICE_GAMEPAD) {
        gamepad = GetTrackedGamepad(state, id);
    } else if (kind == ACTIVE_RUMBLE_DEVICE_JOYSTICK) {
        joystick = GetTrackedJoystick(state, id);
    }

    if (!gamepad && !joystick) {
        gamepad = GetActiveGamepad(state);
        if (gamepad) {
            kind = ACTIVE_RUMBLE_DEVICE_GAMEPAD;
            id = SDL_GetGamepadID(gamepad);
        }
    }
    if (!gamepad && !joystick) {
        joystick = GetActiveJoystick(state);
        if (joystick) {
            kind = ACTIVE_RUMBLE_DEVICE_JOYSTICK;
            id = SDL_GetJoystickID(joystick);
        }
    }

    if (gamepad) {
        supported = GamepadHasRumble(gamepad);
        accent_r = 60;
        accent_g = 210;
        accent_b = 255;
    } else if (joystick) {
        supported = JoystickHasRumble(joystick);
        accent_r = 255;
        accent_g = 150;
        accent_b = 110;
    }

    active = (state->rumble.kind == kind &&
              state->rumble.id == id &&
              state->rumble.last_result &&
              state->rumble.active_until_ns > SDL_GetTicksNS());
    failed = (state->rumble.kind == kind &&
              state->rumble.id == id &&
              !state->rumble.last_result);

    if (active) {
        low = (float)state->rumble.low / 65535.0f;
        high = (float)state->rumble.high / 65535.0f;
    }

    if (failed) {
        accent_r = 255;
        accent_g = 110;
        accent_b = 110;
    } else if (!supported) {
        accent_r = 80;
        accent_g = 90;
        accent_b = 120;
    }

    if (!SetColor(state->renderer, 8, 10, 24, 220) ||
        !SDL_RenderFillRect(state->renderer, &rect) ||
        !SetColor(state->renderer, accent_r, accent_g, accent_b, 255) ||
        !SDL_RenderRect(state->renderer, &rect)) {
        return false;
    }

    const SDL_FRect accent_strip = { rect.x + 1.0f, rect.y + 1.0f, rect.w - 2.0f, 4.0f };
    if (!SetColor(state->renderer, accent_r, accent_g, accent_b, 255) ||
        !SDL_RenderFillRect(state->renderer, &accent_strip)) {
        return false;
    }

    const SDL_FRect source_indicator = {
        rect.x + rect.w - 30.0f,
        rect.y + 10.0f,
        16.0f,
        16.0f
    };
    if (!SetColor(state->renderer,
                  kind == ACTIVE_RUMBLE_DEVICE_GAMEPAD ? accent_r : 24,
                  kind == ACTIVE_RUMBLE_DEVICE_JOYSTICK ? accent_g : 32,
                  accent_b,
                  255) ||
        !SDL_RenderFillRect(state->renderer, &source_indicator)) {
        return false;
    }

    const float pulse_x = rect.x + padding + pulse_radius;
    const float pulse_y = rect.y + rect.h * 0.5f - 6.0f;
    const float pulse_glow = active ? SDL_max(low, high) : (supported ? 0.2f : 0.0f);
    if (!SetColor(state->renderer,
                  (Uint8)(40 + accent_r * pulse_glow),
                  (Uint8)(40 + accent_g * pulse_glow),
                  (Uint8)(50 + accent_b * pulse_glow),
                  255) ||
        !FillCircle(state->renderer, pulse_x, pulse_y, pulse_radius + pulse_glow * 10.0f)) {
        return false;
    }

    const SDL_FRect pulse_core = { pulse_x - 6.0f, pulse_y - 6.0f, 12.0f, 12.0f };
    if (!SetColor(state->renderer, 12, 18, 36, 255) ||
        !SDL_RenderFillRect(state->renderer, &pulse_core)) {
        return false;
    }

    const auto draw_bar = [state](const SDL_FRect &bar, float amount, Uint8 r, Uint8 g, Uint8 b) {
        const SDL_FRect fill = { bar.x + 1.0f, bar.y + 1.0f, SDL_max(0.0f, (bar.w - 2.0f) * SDL_clamp(amount, 0.0f, 1.0f)), bar.h - 2.0f };
        return SetColor(state->renderer, 20, 28, 60, 255) &&
               SDL_RenderFillRect(state->renderer, &bar) &&
               SetColor(state->renderer, 60, 90, 150, 255) &&
               SDL_RenderRect(state->renderer, &bar) &&
               SetColor(state->renderer, r, g, b, 255) &&
               SDL_RenderFillRect(state->renderer, &fill);
    };

    if (!draw_bar(low_bar, low, 255, 170, 110) ||
        !draw_bar(high_bar, high, 120, 230, 255)) {
        return false;
    }

    if (!supported) {
        return SetColor(state->renderer, 80, 90, 120, 255) &&
               SDL_RenderLine(state->renderer, low_bar.x, low_bar.y, high_bar.x + high_bar.w, high_bar.y + high_bar.h) &&
               SDL_RenderLine(state->renderer, high_bar.x + high_bar.w, low_bar.y, low_bar.x, high_bar.y + high_bar.h);
    }

    return true;
}

bool RenderSensorWidget(AppState *state, const SDL_FRect &rect, const SensorSnapshot &sensor, float scale, Uint8 accent_r, Uint8 accent_g, Uint8 accent_b)
{
    const float padding = 10.0f;
    const float plot_size = SDL_min(rect.h - padding * 2.0f, 52.0f);
    const SDL_FRect plot_rect = { rect.x + padding, rect.y + padding, plot_size, plot_size };
    const float bar_x = plot_rect.x + plot_rect.w + 12.0f;
    const float bar_w = rect.x + rect.w - bar_x - padding;
    const float bar_h = 10.0f;
    const float bar_gap = 7.0f;
    const float bars_y = rect.y + padding + SDL_max(0.0f, (plot_rect.h - (bar_h * 3.0f + bar_gap * 2.0f)) * 0.5f);
    const SDL_FRect bars[3] = {
        { bar_x, bars_y, bar_w, bar_h },
        { bar_x, bars_y + bar_h + bar_gap, bar_w, bar_h },
        { bar_x, bars_y + (bar_h + bar_gap) * 2.0f, bar_w, bar_h }
    };
    const Uint8 axis_colors[3][3] = {
        { 255, 120, 120 },
        { 120, 255, 160 },
        { 120, 180, 255 }
    };

    if (!SetColor(state->renderer, 10, 16, 34, 220) ||
        !SDL_RenderFillRect(state->renderer, &rect) ||
        !SetColor(state->renderer,
                  sensor.available ? accent_r : 46,
                  sensor.available ? accent_g : 56,
                  sensor.available ? accent_b : 72,
                  255) ||
        !SDL_RenderRect(state->renderer, &rect)) {
        return false;
    }

    const SDL_FRect accent_strip = { rect.x + 1.0f, rect.y + 1.0f, rect.w - 2.0f, 4.0f };
    if (!SetColor(state->renderer,
                  sensor.enabled ? accent_r : (Uint8)(accent_r / 2),
                  sensor.enabled ? accent_g : (Uint8)(accent_g / 2),
                  sensor.enabled ? accent_b : (Uint8)(accent_b / 2),
                  255) ||
        !SDL_RenderFillRect(state->renderer, &accent_strip) ||
        !SetColor(state->renderer, 60, 90, 150, 255) ||
        !SDL_RenderRect(state->renderer, &plot_rect)) {
        return false;
    }

    if (sensor.available) {
        const int rate_pips = SDL_clamp((int)SDL_lroundf(sensor.rate_hz / 50.0f), 1, 5);

        for (int i = 0; i < 5; ++i) {
            const SDL_FRect pip_rect = {
                rect.x + rect.w - 42.0f + i * 7.0f,
                rect.y + 10.0f,
                5.0f,
                3.0f
            };

            if (!SetColor(state->renderer,
                          i < rate_pips ? accent_r : 36,
                          i < rate_pips ? accent_g : 44,
                          i < rate_pips ? accent_b : 60,
                          255) ||
                !SDL_RenderFillRect(state->renderer, &pip_rect)) {
                return false;
            }
        }
    }

    if (!SetColor(state->renderer, 34, 46, 84, 255) ||
        !SDL_RenderLine(state->renderer,
                        plot_rect.x + plot_rect.w * 0.5f, plot_rect.y,
                        plot_rect.x + plot_rect.w * 0.5f, plot_rect.y + plot_rect.h) ||
        !SDL_RenderLine(state->renderer,
                        plot_rect.x, plot_rect.y + plot_rect.h * 0.5f,
                        plot_rect.x + plot_rect.w, plot_rect.y + plot_rect.h * 0.5f)) {
        return false;
    }

    for (int i = 0; i < 3; ++i) {
        const SDL_FRect midpoint = { bars[i].x + bars[i].w * 0.5f - 1.0f, bars[i].y, 2.0f, bars[i].h };

        if (!SetColor(state->renderer, 18, 26, 56, 255) ||
            !SDL_RenderFillRect(state->renderer, &bars[i]) ||
            !SetColor(state->renderer, 60, 90, 150, 255) ||
            !SDL_RenderRect(state->renderer, &bars[i]) ||
            !SDL_RenderFillRect(state->renderer, &midpoint)) {
            return false;
        }
    }

    if (!sensor.available) {
        return SetColor(state->renderer, 80, 90, 120, 255) &&
               SDL_RenderLine(state->renderer, rect.x + 10.0f, rect.y + 16.0f, rect.x + rect.w - 10.0f, rect.y + rect.h - 16.0f) &&
               SDL_RenderLine(state->renderer, rect.x + rect.w - 10.0f, rect.y + 16.0f, rect.x + 10.0f, rect.y + rect.h - 16.0f);
    }

    if (!sensor.enabled) {
        return SetColor(state->renderer, 255, 200, 90, 255) &&
               SDL_RenderLine(state->renderer, rect.x + 12.0f, rect.y + rect.h - 12.0f, rect.x + rect.w - 12.0f, rect.y + 12.0f);
    }

    {
        const float normalized_x = SDL_clamp(sensor.data[0] / scale, -1.0f, 1.0f);
        const float normalized_y = SDL_clamp(sensor.data[1] / scale, -1.0f, 1.0f);
        const float normalized_z = SDL_clamp(sensor.data[2] / scale, -1.0f, 1.0f);
        const float dot_x = plot_rect.x + plot_rect.w * 0.5f + normalized_x * (plot_rect.w * 0.34f);
        const float dot_y = plot_rect.y + plot_rect.h * 0.5f - normalized_y * (plot_rect.h * 0.34f);
        const float dot_radius = 4.0f + SDL_fabsf(normalized_z) * 5.0f;
        const float values[3] = { normalized_x, normalized_y, normalized_z };

        if (!SetColor(state->renderer, accent_r, accent_g, accent_b, 255) ||
            !FillCircle(state->renderer, dot_x, dot_y, dot_radius)) {
            return false;
        }

        for (int i = 0; i < 3; ++i) {
            const float half_w = bars[i].w * 0.5f - 2.0f;
            const float magnitude = SDL_fabsf(values[i]) * half_w;
            SDL_FRect fill_rect;

            if (values[i] >= 0.0f) {
                fill_rect = { bars[i].x + bars[i].w * 0.5f, bars[i].y + 1.0f, magnitude, bars[i].h - 2.0f };
            } else {
                fill_rect = { bars[i].x + bars[i].w * 0.5f - magnitude, bars[i].y + 1.0f, magnitude, bars[i].h - 2.0f };
            }

            if (!SetColor(state->renderer, axis_colors[i][0], axis_colors[i][1], axis_colors[i][2], 255) ||
                !SDL_RenderFillRect(state->renderer, &fill_rect)) {
                return false;
            }
        }
    }

    return true;
}

} // namespace

void ResetInputDemoState(AppState *state)
{
    if (!state) {
        return;
    }

    SDL_zero(state->gamepads);
    SDL_zero(state->joysticks);
    ResetActiveGamepadState(state);
    ResetActiveJoystickState(state);
    ResetRumblePreview(&state->rumble);
}

void OpenInitialInputDevices(AppState *state)
{
    OpenInitialGamepads(state);
    OpenInitialJoysticks(state);
}

void ShutdownInputDemo(AppState *state)
{
    CloseAllGamepads(state);
    CloseAllJoysticks(state);
}

void PollInputDemo(AppState *state)
{
    if (!state) {
        return;
    }

    PollGamepadState(state);
    PollJoystickState(state);
}

bool HandleInputDemoEvent(AppState *state, const SDL_Event *event)
{
    if (!state || !event) {
        return false;
    }

    if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        OpenGamepad(state, event->gdevice.which);
        return true;
    }

    if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
        const int slot = FindGamepadSlot(state, event->gdevice.which);
        if (slot >= 0) {
            CloseGamepadSlot(state, slot);
        }
        return true;
    }

    if (event->type == SDL_EVENT_JOYSTICK_ADDED) {
        OpenJoystick(state, event->jdevice.which);
        return true;
    }

    if (event->type == SDL_EVENT_JOYSTICK_REMOVED) {
        const int slot = FindJoystickSlot(state, event->jdevice.which);
        if (slot >= 0) {
            CloseJoystickSlot(state, slot);
        }
        return true;
    }

    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        SDL_Gamepad *gamepad = GetActiveGamepad(state);
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH && gamepad &&
            state->active_gamepad_id == event->gbutton.which) {
            TriggerGamepadRumble(state, gamepad, 0x5000, 0x9000, 140);
            return true;
        }
    }

    if (event->type == SDL_EVENT_JOYSTICK_BUTTON_DOWN && !SDL_IsGamepad(event->jbutton.which)) {
        SDL_Joystick *joystick = GetTrackedJoystick(state, event->jbutton.which);
        if (joystick) {
            state->active_joystick_id = event->jbutton.which;
            TriggerJoystickRumble(state, joystick, 0x7000, 0x5000, 180);
            return true;
        }
    }

    return false;
}

SDL_Gamepad *GetActiveGamepad(AppState *state)
{
    int slot;

    if (!state) {
        return nullptr;
    }

    slot = FindGamepadSlot(state, state->active_gamepad_id);
    if (slot >= 0) {
        return state->gamepads[slot].gamepad;
    }

    SelectFallbackGamepad(state);
    if (!state->active_gamepad_id) {
        return nullptr;
    }

    slot = FindGamepadSlot(state, state->active_gamepad_id);
    if (slot < 0) {
        ResetActiveGamepadState(state);
        return nullptr;
    }

    return state->gamepads[slot].gamepad;
}

bool IsAnyInputButtonPressed(AppState *state)
{
    const bool *keyboard = SDL_GetKeyboardState(nullptr);
    SDL_Gamepad *gamepad;
    SDL_Joystick *joystick;
    const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(nullptr, nullptr);

    if (keyboard) {
        for (int scancode = 0; scancode < SDL_SCANCODE_COUNT; ++scancode) {
            if (keyboard[scancode]) {
                return true;
            }
        }
    }

    if (mouse_buttons != 0) {
        return true;
    }

    gamepad = GetActiveGamepad(state);
    if (gamepad && SDL_GamepadConnected(gamepad)) {
        for (int button = 0; button < SDL_GAMEPAD_BUTTON_COUNT; ++button) {
            if (SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(button))) {
                return true;
            }
        }
    }

    joystick = GetActiveJoystick(state);
    if (joystick && SDL_JoystickConnected(joystick)) {
        const int button_count = SDL_GetNumJoystickButtons(joystick);
        for (int button = 0; button < button_count; ++button) {
            if (SDL_GetJoystickButton(joystick, button)) {
                return true;
            }
        }
    }

    return false;
}

bool RenderInputOverlay(AppState *state, int width, int height)
{
    SDL_Gamepad *active_gamepad = GetActiveGamepad(state);
    const float panel_x = 24.0f;
    const float panel_y = 24.0f;
    const float panel_w = 260.0f;
    const float panel_h = 220.0f;
    const float rumble_h = 72.0f;
    const float audio_h = 108.0f;
    const float touch_w = 260.0f;
    const float touch_h = touch_w * (480.0f / 854.0f);
    const float touch_x = (float)width - touch_w - 24.0f;
    const float touch_y = 24.0f;
    const float sensor_y = touch_y + touch_h + 18.0f;
    const float sensor_h = SDL_max(0.0f, (float)height - sensor_y - 24.0f);
    const float storage_w = 360.0f;
    const float storage_h = 92.0f;
    const SDL_FRect storage_rect = {
        SDL_max(24.0f, ((float)width - storage_w) * 0.5f),
        (float)height - storage_h - 18.0f,
        storage_w,
        storage_h
    };
    const SDL_FRect panel = { panel_x, panel_y, panel_w, panel_h };
    const SDL_FRect rumble_rect = { panel_x, panel_y + panel_h + 16.0f, panel_w, rumble_h };
    const SDL_FRect audio_rect = { panel_x, rumble_rect.y + rumble_rect.h + 16.0f, panel_w, audio_h };
    const SDL_FRect touch_rect = { touch_x, touch_y, touch_w, touch_h };
    const SDL_FRect sensor_rect = { touch_x, sensor_y, touch_w, sensor_h };
    const bool has_gamepad = active_gamepad && SDL_GamepadConnected(active_gamepad);
    const float left_x = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_LEFTX)) : 0.0f;
    const float left_y = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_LEFTY)) : 0.0f;
    const float right_x = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_RIGHTX)) : 0.0f;
    const float right_y = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_RIGHTY)) : 0.0f;
    const float left_trigger = has_gamepad ? SDL_max(0.0f, AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER))) : 0.0f;
    const float right_trigger = has_gamepad ? SDL_max(0.0f, AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))) : 0.0f;
    const float stick_box = 64.0f;
    const float stick_radius = 6.0f;
    const float face_radius = 12.0f;
    const int battery_percent = (state->battery_percent >= 0) ? state->battery_percent : 0;
    const float battery_fill = SDL_clamp((float)battery_percent / 100.0f, 0.0f, 1.0f);
    const SDL_FRect battery_back = { panel_x + 16.0f, panel_y + 16.0f, 120.0f, 12.0f };
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

    auto button = [active_gamepad, has_gamepad](SDL_GamepadButton id) {
        return has_gamepad && SDL_GetGamepadButton(active_gamepad, id);
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

    for (int i = 0; i < MAX_TRACKED_GAMEPADS; ++i) {
        const int row = i / 4;
        const int column = i % 4;
        const bool occupied = state->gamepads[i].gamepad != nullptr;
        const bool selected = occupied && state->gamepads[i].id == state->active_gamepad_id;
        const SDL_FRect slot_rect = {
            panel_x + 156.0f + column * 22.0f,
            panel_y + 12.0f + row * 14.0f,
            16.0f,
            10.0f
        };

        if (!draw_fill(slot_rect,
                       selected ? 255 : (occupied ? 70 : 20),
                       selected ? 220 : (occupied ? 180 : 28),
                       selected ? 120 : (occupied ? 255 : 52),
                       255) ||
            !draw_outline(slot_rect, 0, 0, 0, 255)) {
            return false;
        }
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

    if (!RenderRumbleWidget(state, rumble_rect)) {
        return false;
    }

    if (!RenderAudioWidget(state, audio_rect)) {
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

    if (sensor_rect.h >= 90.0f) {
        const float widget_gap = 8.0f;
        const float widget_padding = 10.0f;
        const float widget_h = (sensor_rect.h - widget_padding * 2.0f - widget_gap * 2.0f) / 3.0f;
        const float widget_y = sensor_rect.y + widget_padding;
        const SDL_FRect accel_rect = { sensor_rect.x, widget_y, sensor_rect.w, widget_h };
        const SDL_FRect gyro_rect = { sensor_rect.x, widget_y + widget_h + widget_gap, sensor_rect.w, widget_h };
        const SDL_FRect accel_l_rect = { sensor_rect.x, widget_y + (widget_h + widget_gap) * 2.0f, sensor_rect.w, widget_h };

        if (!RenderSensorWidget(state, accel_rect, state->accel_sensor, SDL_STANDARD_GRAVITY * 2.0f, 255, 170, 90) ||
            !RenderSensorWidget(state, gyro_rect, state->gyro_sensor, 4.0f, 90, 220, 255) ||
            !RenderSensorWidget(state, accel_l_rect, state->accel_l_sensor, SDL_STANDARD_GRAVITY * 2.0f, 200, 110, 255)) {
            return false;
        }
    }

    if (!RenderStorageWidget(state, storage_rect)) {
        return false;
    }

    return true;
}
