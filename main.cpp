#define SDL_MAIN_USE_CALLBACKS 1
#include <cmath>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

static constexpr int MAX_TRACKED_GAMEPADS = 8;
static constexpr int MAX_TRACKED_JOYSTICKS = 8;

struct GamepadSlot
{
    SDL_Gamepad *gamepad;
    SDL_JoystickID id;
};

struct JoystickSlot
{
    SDL_Joystick *joystick;
    SDL_JoystickID id;
};

struct SensorSnapshot
{
    bool available;
    bool enabled;
    float rate_hz;
    float data[3];
};

enum ActiveRumbleDeviceKind
{
    ACTIVE_RUMBLE_DEVICE_NONE,
    ACTIVE_RUMBLE_DEVICE_GAMEPAD,
    ACTIVE_RUMBLE_DEVICE_JOYSTICK,
};

struct RumblePreview
{
    ActiveRumbleDeviceKind kind;
    SDL_JoystickID id;
    bool last_result;
    Uint16 low;
    Uint16 high;
    Uint64 active_until_ns;
};

struct AudioDemoState
{
    SDL_AudioStream *playback_stream;
    SDL_AudioStream *capture_stream;
    SDL_AudioSpec mix_spec;
    float tone_phase;
    float tone_frequency;
    float tone_gain;
    float playback_level;
    float capture_level;
    float loopback_level;
    bool capture_available;
    bool monitoring_enabled;
};

struct StorageDemoState
{
    bool title_ready;
    bool user_ready;
    bool last_save_ok;
    Uint32 save_count;
    Uint64 title_bytes;
    Uint64 space_remaining;
    Uint32 title_signature;
    Uint64 last_write_ns;
    char title_preview[64];
    char base_path[48];
    char pref_path[72];
    char title_error[96];
    char user_error[96];
};

struct AppState
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *background;
    int background_width;
    int background_height;
    bool first_frame_complete;
    GamepadSlot gamepads[MAX_TRACKED_GAMEPADS];
    JoystickSlot joysticks[MAX_TRACKED_JOYSTICKS];
    SDL_JoystickID active_gamepad_id;
    SDL_JoystickID active_joystick_id;
    SDL_PowerState battery_state;
    int battery_percent;
    bool touch_down;
    float touch_x;
    float touch_y;
    float touch_pressure;
    SensorSnapshot accel_sensor;
    SensorSnapshot gyro_sensor;
    SensorSnapshot accel_l_sensor;
    RumblePreview rumble;
    AudioDemoState audio;
    StorageDemoState storage;
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

static bool RenderMouseCursor(AppState *state, int width, int height)
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

static void ResetSensorSnapshot(SensorSnapshot *sensor)
{
    if (!sensor) {
        return;
    }

    SDL_zero(*sensor);
}

static void ResetTrackedSensors(AppState *state)
{
    if (!state) {
        return;
    }

    ResetSensorSnapshot(&state->accel_sensor);
    ResetSensorSnapshot(&state->gyro_sensor);
    ResetSensorSnapshot(&state->accel_l_sensor);
}

static void ResetRumblePreview(RumblePreview *rumble)
{
    if (!rumble) {
        return;
    }

    SDL_zero(*rumble);
}

static void ResetAudioDemoState(AudioDemoState *audio)
{
    if (!audio) {
        return;
    }

    SDL_zero(*audio);
    audio->tone_frequency = 330.0f;
    audio->tone_gain = 0.08f;
}

static void ResetStorageDemoState(StorageDemoState *storage)
{
    if (!storage) {
        return;
    }

    SDL_zero(*storage);
}

static void CopyStorageDisplayString(char *destination, size_t destination_size, const char *source)
{
    if (!destination || destination_size == 0) {
        return;
    }

    destination[0] = '\0';
    if (!source || !*source) {
        return;
    }

    if (SDL_strlen(source) < destination_size) {
        SDL_strlcpy(destination, source, destination_size);
        return;
    }

    if (destination_size <= 4) {
        SDL_strlcpy(destination, source, destination_size);
        return;
    }

    SDL_memcpy(destination, source, destination_size - 4);
    destination[destination_size - 4] = '.';
    destination[destination_size - 3] = '.';
    destination[destination_size - 2] = '.';
    destination[destination_size - 1] = '\0';
}

static void CopyStorageErrorString(char *destination, size_t destination_size)
{
    const char *error = SDL_GetError();
    CopyStorageDisplayString(destination, destination_size, (error && *error) ? error : "unknown error");
}

static void SanitizeStoragePreview(char *text)
{
    if (!text) {
        return;
    }

    for (char *cursor = text; *cursor; ++cursor) {
        if (*cursor == '\r' || *cursor == '\n' || *cursor == '\t') {
            *cursor = ' ';
        }
    }
}

static Uint32 HashStorageBytes(const Uint8 *bytes, size_t length)
{
    Uint32 hash = 2166136261u;

    for (size_t i = 0; i < length; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }

    return hash;
}

static void RefreshStoragePaths(StorageDemoState *storage)
{
    const char *base_path;
    char *pref_path;

    if (!storage) {
        return;
    }

    base_path = SDL_GetBasePath();
    if (base_path) {
        CopyStorageDisplayString(storage->base_path, sizeof(storage->base_path), base_path);
    }

    pref_path = SDL_GetPrefPath("wiiu", "sdl-demo");
    if (pref_path) {
        CopyStorageDisplayString(storage->pref_path, sizeof(storage->pref_path), pref_path);
        SDL_free(pref_path);
    }
}

static bool ReadStoredCounter(SDL_Storage *storage, const char *path, Uint32 *count)
{
    SDL_PathInfo info;
    char buffer[32];

    if (!storage || !path || !count) {
        return false;
    }

    *count = 0;
    if (!SDL_GetStoragePathInfo(storage, path, &info)) {
        SDL_ClearError();
        return true;
    }

    if (info.type != SDL_PATHTYPE_FILE || info.size == 0 || info.size >= sizeof(buffer)) {
        return true;
    }

    if (!SDL_ReadStorageFile(storage, path, buffer, info.size)) {
        return false;
    }

    buffer[(size_t)info.size] = '\0';
    *count = (Uint32)SDL_strtoul(buffer, nullptr, 10);
    return true;
}

static bool PersistStorageCounter(StorageDemoState *storage, Uint32 new_count)
{
    static constexpr const char *save_dir = "showcase";
    static constexpr const char *save_file = "showcase/save-count.txt";
    SDL_Storage *user_storage;
    char buffer[32];
    int length;
    bool close_result;

    if (!storage) {
        return false;
    }

    storage->last_save_ok = false;
    user_storage = SDL_OpenUserStorage("wiiu", "sdl-demo", 0);
    if (!user_storage) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        return false;
    }

    if (!SDL_StorageReady(user_storage)) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_CloseStorage(user_storage);
        return false;
    }

    if (!SDL_CreateStorageDirectory(user_storage, save_dir)) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_CloseStorage(user_storage);
        return false;
    }

    length = SDL_snprintf(buffer, sizeof(buffer), "%u", (unsigned int)new_count);
    if (length <= 0 || length >= (int)sizeof(buffer) ||
        !SDL_WriteStorageFile(user_storage, save_file, buffer, (Uint64)length)) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_CloseStorage(user_storage);
        return false;
    }

    storage->space_remaining = SDL_GetStorageSpaceRemaining(user_storage);
    close_result = SDL_CloseStorage(user_storage);
    if (!close_result) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        return false;
    }

    storage->user_ready = true;
    storage->last_save_ok = true;
    storage->save_count = new_count;
    storage->last_write_ns = SDL_GetTicksNS();
    storage->user_error[0] = '\0';
    RefreshStoragePaths(storage);
    return true;
}

static bool InitStorageDemo(AppState *state)
{
    static constexpr const char *title_file = "storage-demo-title.txt";
    static constexpr const char *save_file = "showcase/save-count.txt";
    StorageDemoState *storage;
    SDL_Storage *title_storage;
    SDL_Storage *user_storage;
    Uint32 initial_count = 0;

    if (!state) {
        return false;
    }

    storage = &state->storage;
    ResetStorageDemoState(storage);
    RefreshStoragePaths(storage);

    title_storage = SDL_OpenTitleStorage(nullptr, 0);
    if (!title_storage) {
        CopyStorageErrorString(storage->title_error, sizeof(storage->title_error));
    } else if (!SDL_StorageReady(title_storage)) {
        CopyStorageErrorString(storage->title_error, sizeof(storage->title_error));
        SDL_CloseStorage(title_storage);
        title_storage = nullptr;
    } else {
        Uint64 title_size = 0;

        if (SDL_GetStorageFileSize(title_storage, title_file, &title_size) &&
            title_size > 0 && title_size < 4096) {
            Uint8 *bytes = (Uint8 *)SDL_malloc((size_t)title_size + 1);

            if (bytes && SDL_ReadStorageFile(title_storage, title_file, bytes, title_size)) {
                bytes[(size_t)title_size] = '\0';
                storage->title_ready = true;
                storage->title_bytes = title_size;
                storage->title_signature = HashStorageBytes(bytes, (size_t)title_size);
                CopyStorageDisplayString(storage->title_preview, sizeof(storage->title_preview), (const char *)bytes);
                SanitizeStoragePreview(storage->title_preview);
            } else {
                CopyStorageErrorString(storage->title_error, sizeof(storage->title_error));
            }
            SDL_free(bytes);
        } else {
            CopyStorageErrorString(storage->title_error, sizeof(storage->title_error));
        }

        if (!SDL_CloseStorage(title_storage) && storage->title_ready) {
            storage->title_ready = false;
            CopyStorageErrorString(storage->title_error, sizeof(storage->title_error));
        }
    }

    user_storage = SDL_OpenUserStorage("wiiu", "sdl-demo", 0);
    if (!user_storage) {
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        return true;
    }

    if (!SDL_StorageReady(user_storage)) {
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
        SDL_CloseStorage(user_storage);
        return true;
    }

    storage->user_ready = true;
    storage->space_remaining = SDL_GetStorageSpaceRemaining(user_storage);
    if (!ReadStoredCounter(user_storage, save_file, &initial_count)) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
    }

    if (!SDL_CloseStorage(user_storage) && storage->user_ready) {
        storage->user_ready = false;
        CopyStorageErrorString(storage->user_error, sizeof(storage->user_error));
    }

    if (storage->user_ready) {
        (void)PersistStorageCounter(storage, initial_count + 1);
    }

    return true;
}

static const char *GetSensorName(SDL_SensorType type)
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

static void EnableSensorIfPresent(SDL_Gamepad *gamepad, SDL_SensorType type)
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

static void RefreshSensorSnapshot(SDL_Gamepad *gamepad, SDL_SensorType type, SensorSnapshot *sensor)
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

static void ResetActiveGamepadState(AppState *state)
{
    state->active_gamepad_id = 0;
    state->battery_state = SDL_POWERSTATE_UNKNOWN;
    state->battery_percent = -1;
    state->touch_down = false;
    state->touch_x = 0.0f;
    state->touch_y = 0.0f;
    state->touch_pressure = 0.0f;
    ResetTrackedSensors(state);
}

static void ResetActiveJoystickState(AppState *state)
{
    if (!state) {
        return;
    }

    state->active_joystick_id = 0;
}

static int FindGamepadSlot(const AppState *state, SDL_JoystickID gamepad_id)
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

static int FindFreeGamepadSlot(const AppState *state)
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

static int FindJoystickSlot(const AppState *state, SDL_JoystickID joystick_id)
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

static int FindFreeJoystickSlot(const AppState *state)
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

static void SelectFallbackGamepad(AppState *state)
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

static void SelectFallbackJoystick(AppState *state)
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

static SDL_Gamepad *GetActiveGamepad(AppState *state)
{
    int slot = FindGamepadSlot(state, state->active_gamepad_id);

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

static SDL_Joystick *GetActiveJoystick(AppState *state)
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

static SDL_Gamepad *GetTrackedGamepad(AppState *state, SDL_JoystickID gamepad_id)
{
    const int slot = FindGamepadSlot(state, gamepad_id);
    return (slot >= 0) ? state->gamepads[slot].gamepad : nullptr;
}

static SDL_Joystick *GetTrackedJoystick(AppState *state, SDL_JoystickID joystick_id)
{
    const int slot = FindJoystickSlot(state, joystick_id);
    return (slot >= 0) ? state->joysticks[slot].joystick : nullptr;
}

static bool GamepadHasRumble(SDL_Gamepad *gamepad)
{
    return gamepad &&
           SDL_GetBooleanProperty(SDL_GetGamepadProperties(gamepad),
                                  SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN,
                                  false);
}

static bool JoystickHasRumble(SDL_Joystick *joystick)
{
    return joystick &&
           SDL_GetBooleanProperty(SDL_GetJoystickProperties(joystick),
                                  SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN,
                                  false);
}

static void UpdateRumblePreview(AppState *state, ActiveRumbleDeviceKind kind, SDL_JoystickID id,
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

static void TriggerGamepadRumble(AppState *state, SDL_Gamepad *gamepad,
                                 Uint16 low, Uint16 high, Uint32 duration_ms)
{
    const SDL_JoystickID id = gamepad ? SDL_GetGamepadID(gamepad) : 0;
    const bool result = gamepad && SDL_RumbleGamepad(gamepad, low, high, duration_ms);

    UpdateRumblePreview(state, ACTIVE_RUMBLE_DEVICE_GAMEPAD, id, result, low, high, duration_ms);
}

static void TriggerJoystickRumble(AppState *state, SDL_Joystick *joystick,
                                  Uint16 low, Uint16 high, Uint32 duration_ms)
{
    const SDL_JoystickID id = joystick ? SDL_GetJoystickID(joystick) : 0;
    const bool result = joystick && SDL_RumbleJoystick(joystick, low, high, duration_ms);

    UpdateRumblePreview(state, ACTIVE_RUMBLE_DEVICE_JOYSTICK, id, result, low, high, duration_ms);
}

static void CloseGamepadSlot(AppState *state, int slot)
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

static void CloseJoystickSlot(AppState *state, int slot)
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

static void CloseAllGamepads(AppState *state)
{
    if (!state) {
        return;
    }

    for (int i = 0; i < MAX_TRACKED_GAMEPADS; ++i) {
        CloseGamepadSlot(state, i);
    }
}

static void CloseAllJoysticks(AppState *state)
{
    if (!state) {
        return;
    }

    for (int i = 0; i < MAX_TRACKED_JOYSTICKS; ++i) {
        CloseJoystickSlot(state, i);
    }
}

static void OpenGamepad(AppState *state, SDL_JoystickID gamepad_id)
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

static void OpenJoystick(AppState *state, SDL_JoystickID joystick_id)
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

static void OpenInitialGamepads(AppState *state)
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

static void OpenInitialJoysticks(AppState *state)
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

static void PollGamepadState(AppState *state)
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

static void PollJoystickState(AppState *state)
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

static void ShutdownAudioDemo(AppState *state)
{
    if (!state) {
        return;
    }

    if (state->audio.capture_stream) {
        const SDL_AudioDeviceID capture_device = SDL_GetAudioStreamDevice(state->audio.capture_stream);
        SDL_CloseAudioDevice(capture_device);
        SDL_DestroyAudioStream(state->audio.capture_stream);
        state->audio.capture_stream = nullptr;
    }

    if (state->audio.playback_stream) {
        const SDL_AudioDeviceID playback_device = SDL_GetAudioStreamDevice(state->audio.playback_stream);
        SDL_CloseAudioDevice(playback_device);
        SDL_DestroyAudioStream(state->audio.playback_stream);
        state->audio.playback_stream = nullptr;
    }
}

static bool InitAudioDemo(AppState *state)
{
    SDL_AudioDeviceID playback_device;
    SDL_AudioSpec playback_device_spec;

    if (!state) {
        return false;
    }

    playback_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!playback_device) {
        SDL_Log("SDL_OpenAudioDevice(default playback) failed: %s", SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(playback_device);
    SDL_GetAudioDeviceFormat(playback_device, &playback_device_spec, nullptr);

    state->audio.mix_spec.format = SDL_AUDIO_F32;
    state->audio.mix_spec.channels = 2;
    state->audio.mix_spec.freq = playback_device_spec.freq;
    state->audio.playback_stream = SDL_CreateAudioStream(&state->audio.mix_spec, &playback_device_spec);
    if (!state->audio.playback_stream) {
        SDL_Log("SDL_CreateAudioStream(playback) failed: %s", SDL_GetError());
        SDL_CloseAudioDevice(playback_device);
        return false;
    }

    if (!SDL_BindAudioStream(playback_device, state->audio.playback_stream)) {
        SDL_Log("SDL_BindAudioStream(playback) failed: %s", SDL_GetError());
        ShutdownAudioDemo(state);
        return false;
    }

    SDL_ResumeAudioStreamDevice(state->audio.playback_stream);

    {
        const SDL_AudioDeviceID capture_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, nullptr);
        if (!capture_device) {
            SDL_Log("Recording device unavailable: %s", SDL_GetError());
        } else {
            SDL_AudioSpec capture_device_spec;

            SDL_PauseAudioDevice(capture_device);
            SDL_GetAudioDeviceFormat(capture_device, &capture_device_spec, nullptr);

            state->audio.capture_stream = SDL_CreateAudioStream(&capture_device_spec, &capture_device_spec);
            if (!state->audio.capture_stream) {
                SDL_Log("SDL_CreateAudioStream(capture) failed: %s", SDL_GetError());
                SDL_CloseAudioDevice(capture_device);
            } else if (!SDL_BindAudioStream(capture_device, state->audio.capture_stream)) {
                SDL_Log("SDL_BindAudioStream(capture) failed: %s", SDL_GetError());
                SDL_CloseAudioDevice(capture_device);
                SDL_DestroyAudioStream(state->audio.capture_stream);
                state->audio.capture_stream = nullptr;
            } else if (!SDL_SetAudioStreamFormat(state->audio.capture_stream, nullptr, &state->audio.mix_spec)) {
                SDL_Log("SDL_SetAudioStreamFormat(capture) failed: %s", SDL_GetError());
                SDL_CloseAudioDevice(capture_device);
                SDL_DestroyAudioStream(state->audio.capture_stream);
                state->audio.capture_stream = nullptr;
            } else {
                state->audio.capture_available = true;
                SDL_ResumeAudioStreamDevice(state->audio.capture_stream);
            }
        }
    }

    return true;
}

static void UpdateAudioDemo(AppState *state)
{
    constexpr float tau = 6.28318530717958647692f;
    constexpr int tone_chunk_frames = 512;
    constexpr int capture_chunk_frames = 256;

    float tone_buffer[tone_chunk_frames * 2];
    float capture_buffer[capture_chunk_frames * 2];
    float loopback_buffer[capture_chunk_frames * 2];
    SDL_Gamepad *active_gamepad;
    const bool *keyboard;
    bool has_gamepad;
    bool monitoring_enabled;
    float right_x;
    float right_trigger;
    int queued_bytes;
    const int target_queue_bytes = tone_chunk_frames * 6 * (int)sizeof(float) * 2;

    if (!state || !state->audio.playback_stream) {
        return;
    }

    active_gamepad = GetActiveGamepad(state);
    keyboard = SDL_GetKeyboardState(nullptr);
    has_gamepad = active_gamepad && SDL_GamepadConnected(active_gamepad);
    right_x = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_RIGHTX)) : 0.0f;
    right_trigger = has_gamepad ? SDL_max(0.0f, AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))) : 0.0f;
    monitoring_enabled = (keyboard && keyboard[SDL_SCANCODE_M]) ||
                         (has_gamepad && SDL_GetGamepadButton(active_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER));
    state->audio.monitoring_enabled = monitoring_enabled;

    state->audio.tone_frequency = 220.0f + (right_x + 1.0f) * 220.0f;
    state->audio.tone_gain += ((0.05f + right_trigger * 0.18f) - state->audio.tone_gain) * 0.18f;
    state->audio.capture_level *= 0.86f;
    state->audio.playback_level *= 0.90f;
    state->audio.loopback_level *= 0.82f;

    if (state->audio.capture_stream) {
        const int minimum_capture_bytes = capture_chunk_frames * (int)sizeof(float) * 2;

        while (SDL_GetAudioStreamAvailable(state->audio.capture_stream) >= minimum_capture_bytes) {
            const int br = SDL_GetAudioStreamData(state->audio.capture_stream, capture_buffer, minimum_capture_bytes);
            if (br <= 0) {
                break;
            }

            const int sample_count = br / (int)sizeof(float);
            float capture_peak = 0.0f;

            for (int i = 0; i < sample_count; ++i) {
                const float value = capture_buffer[i];
                capture_peak = SDL_max(capture_peak, SDL_fabsf(value));
                loopback_buffer[i] = value * 0.18f;
            }

            state->audio.capture_level = SDL_max(state->audio.capture_level, capture_peak);

            if (monitoring_enabled) {
                state->audio.loopback_level = SDL_max(state->audio.loopback_level, capture_peak * 0.18f);
                if (!SDL_PutAudioStreamData(state->audio.playback_stream, loopback_buffer, br)) {
                    SDL_Log("SDL_PutAudioStreamData(loopback) failed: %s", SDL_GetError());
                    break;
                }
            }
        }
    }

    queued_bytes = SDL_GetAudioStreamQueued(state->audio.playback_stream);
    while (queued_bytes >= 0 && queued_bytes < target_queue_bytes) {
        float phase = state->audio.tone_phase;
        const float phase_step = tau * state->audio.tone_frequency / (float)state->audio.mix_spec.freq;
        float tone_peak = 0.0f;

        for (int i = 0; i < tone_chunk_frames; ++i) {
            const float base = SDL_sinf(phase);
            const float overtone = SDL_sinf(phase * 2.0f + 0.6f) * 0.25f;
            const float sample = (base + overtone) * state->audio.tone_gain;
            const float left = sample * 0.9f;
            const float right = sample * 0.9f;

            tone_buffer[i * 2] = left;
            tone_buffer[i * 2 + 1] = right;
            tone_peak = SDL_max(tone_peak, SDL_max(SDL_fabsf(left), SDL_fabsf(right)));

            phase += phase_step;
            if (phase >= tau) {
                phase -= tau;
            }
        }

        state->audio.tone_phase = phase;
        state->audio.playback_level = SDL_max(state->audio.playback_level, tone_peak);

        if (!SDL_PutAudioStreamData(state->audio.playback_stream, tone_buffer, (int)sizeof(tone_buffer))) {
            SDL_Log("SDL_PutAudioStreamData(tone) failed: %s", SDL_GetError());
            break;
        }

        queued_bytes += (int)sizeof(tone_buffer);
    }
}

static bool RenderRumbleWidget(AppState *state, const SDL_FRect &rect)
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

static bool RenderSensorWidget(AppState *state, const SDL_FRect &rect, const SensorSnapshot &sensor, float scale, Uint8 accent_r, Uint8 accent_g, Uint8 accent_b)
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

static bool RenderAudioWidget(AppState *state, const SDL_FRect &rect)
{
    const float padding = 10.0f;
    const float bar_h = 12.0f;
    const float bar_gap = 8.0f;
    const float meter_w = rect.w - padding * 2.0f - 76.0f;
    const float status_x = rect.x + rect.w - padding - 26.0f;
    const float status_y = rect.y + 16.0f;
    const float tone_amount = SDL_clamp((state->audio.tone_frequency - 220.0f) / 440.0f, 0.0f, 1.0f);
    const float playback_amount = SDL_clamp(state->audio.playback_level, 0.0f, 1.0f);
    const float capture_amount = SDL_clamp(state->audio.capture_level, 0.0f, 1.0f);
    const float loopback_amount = SDL_clamp(state->audio.loopback_level, 0.0f, 1.0f);
    const SDL_FRect tone_bar = { rect.x + padding, rect.y + 20.0f, meter_w, bar_h };
    const SDL_FRect playback_bar = { tone_bar.x, tone_bar.y + bar_h + bar_gap, meter_w, bar_h };
    const SDL_FRect capture_bar = { tone_bar.x, playback_bar.y + bar_h + bar_gap, meter_w, bar_h };
    const SDL_FRect loopback_bar = { tone_bar.x, capture_bar.y + bar_h + bar_gap, meter_w, bar_h };
    const float orb_x = rect.x + rect.w - 40.0f;
    const float orb_y = rect.y + rect.h - 34.0f;
    const float orb_radius = 14.0f + playback_amount * 12.0f + capture_amount * 6.0f;

    if (!SetColor(state->renderer, 9, 14, 30, 228) ||
        !SDL_RenderFillRect(state->renderer, &rect) ||
        !SetColor(state->renderer, 90, 220, 255, 255) ||
        !SDL_RenderRect(state->renderer, &rect)) {
        return false;
    }

    {
        const SDL_FRect accent_strip = { rect.x + 1.0f, rect.y + 1.0f, rect.w - 2.0f, 4.0f };
        if (!SetColor(state->renderer, 90, 220, 255, 255) ||
            !SDL_RenderFillRect(state->renderer, &accent_strip)) {
            return false;
        }
    }

    {
        const auto draw_bar = [state](const SDL_FRect &bar, float amount, Uint8 r, Uint8 g, Uint8 b) {
            const SDL_FRect fill = { bar.x + 1.0f, bar.y + 1.0f, SDL_max(0.0f, (bar.w - 2.0f) * SDL_clamp(amount, 0.0f, 1.0f)), bar.h - 2.0f };
            return SetColor(state->renderer, 20, 28, 60, 255) &&
                   SDL_RenderFillRect(state->renderer, &bar) &&
                   SetColor(state->renderer, 60, 90, 150, 255) &&
                   SDL_RenderRect(state->renderer, &bar) &&
                   SetColor(state->renderer, r, g, b, 255) &&
                   SDL_RenderFillRect(state->renderer, &fill);
        };

        if (!draw_bar(tone_bar, tone_amount, 120, 180, 255) ||
            !draw_bar(playback_bar, playback_amount, 255, 175, 110) ||
            !draw_bar(capture_bar, capture_amount, state->audio.capture_available ? 120 : 70, state->audio.capture_available ? 255 : 80, state->audio.capture_available ? 160 : 90) ||
            !draw_bar(loopback_bar, loopback_amount, 180, 140, 255)) {
            return false;
        }
    }

    {
        const SDL_FRect capture_status = { status_x, status_y, 18.0f, 18.0f };
        const SDL_FRect monitor_status = { status_x, status_y + 26.0f, 18.0f, 18.0f };

        if (!SetColor(state->renderer,
                      state->audio.capture_available ? 120 : 70,
                      state->audio.capture_available ? 255 : 80,
                      state->audio.capture_available ? 160 : 90,
                      255) ||
            !SDL_RenderFillRect(state->renderer, &capture_status) ||
            !SetColor(state->renderer,
                      state->audio.monitoring_enabled ? 210 : 76,
                      state->audio.monitoring_enabled ? 160 : 84,
                      state->audio.monitoring_enabled ? 255 : 110,
                      255) ||
            !SDL_RenderFillRect(state->renderer, &monitor_status)) {
            return false;
        }

        if (!state->audio.capture_available) {
            if (!SetColor(state->renderer, 255, 120, 120, 255) ||
                !SDL_RenderLine(state->renderer, capture_status.x, capture_status.y, capture_status.x + capture_status.w, capture_status.y + capture_status.h) ||
                !SDL_RenderLine(state->renderer, capture_status.x + capture_status.w, capture_status.y, capture_status.x, capture_status.y + capture_status.h)) {
                return false;
            }
        }
    }

    if (!SetColor(state->renderer,
                  (Uint8)(50 + playback_amount * 150.0f),
                  (Uint8)(70 + capture_amount * 170.0f),
                  (Uint8)(120 + loopback_amount * 120.0f),
                  255) ||
        !FillCircle(state->renderer, orb_x, orb_y, orb_radius)) {
        return false;
    }

    {
        const SDL_FRect core = { orb_x - 6.0f, orb_y - 6.0f, 12.0f, 12.0f };
        if (!SetColor(state->renderer, 15, 18, 42, 255) ||
            !SDL_RenderFillRect(state->renderer, &core)) {
            return false;
        }
    }

    return true;
}

static bool RenderStorageWidget(AppState *state, const SDL_FRect &rect)
{
    const StorageDemoState &storage = state->storage;
    const float padding = 10.0f;
    const float line_height = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2.0f;
    const float text_x = rect.x + padding;
    const float text_y = rect.y + 10.0f;
    const float pulse_amount = (storage.last_write_ns > 0)
                                   ? SDL_clamp(1.0f - (float)((SDL_GetTicksNS() - storage.last_write_ns) / (double)(SDL_NS_PER_SECOND * 2)), 0.0f, 1.0f)
                                   : 0.0f;
    const Uint8 accent_r = storage.user_ready ? 120 : 255;
    const Uint8 accent_g = storage.user_ready ? 220 : 120;
    const Uint8 accent_b = storage.title_ready ? 255 : 140;

    if (!SetColor(state->renderer, 8, 12, 28, 228) ||
        !SDL_RenderFillRect(state->renderer, &rect) ||
        !SetColor(state->renderer, accent_r, accent_g, accent_b, 255) ||
        !SDL_RenderRect(state->renderer, &rect)) {
        return false;
    }

    {
        const SDL_FRect accent_strip = { rect.x + 1.0f, rect.y + 1.0f, rect.w - 2.0f, 4.0f };
        if (!SetColor(state->renderer, accent_r, accent_g, accent_b, 255) ||
            !SDL_RenderFillRect(state->renderer, &accent_strip)) {
            return false;
        }
    }

    for (int i = 0; i < 8; ++i) {
        const bool lit = ((storage.title_signature >> (i * 4)) & 0xF) != 0;
        const SDL_FRect pip = {
            rect.x + rect.w - 86.0f + i * 9.0f,
            rect.y + 10.0f,
            6.0f,
            6.0f
        };

        if (!SetColor(state->renderer,
                      lit ? (Uint8)(80 + i * 18) : 28,
                      lit ? (Uint8)(120 + i * 12) : 32,
                      lit ? 255 : 44,
                      255) ||
            !SDL_RenderFillRect(state->renderer, &pip)) {
            return false;
        }
    }

    if (pulse_amount > 0.0f) {
        const float radius = 8.0f + pulse_amount * 12.0f;
        if (!SetColor(state->renderer,
                      (Uint8)(90 + pulse_amount * 140.0f),
                      (Uint8)(90 + pulse_amount * 100.0f),
                      255,
                      255) ||
            !FillCircle(state->renderer, rect.x + rect.w - 24.0f, rect.y + rect.h - 24.0f, radius)) {
            return false;
        }
    }

    if (!SetColor(state->renderer, 230, 240, 255, 255) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y, "storage") ||
        !SDL_RenderDebugTextFormat(state->renderer, text_x, text_y + line_height,
                                   "title %s  %llu bytes",
                                   storage.title_ready ? "ok" : "miss",
                                   (unsigned long long)storage.title_bytes) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 2.0f,
                             storage.title_ready ? storage.title_preview : storage.title_error) ||
        !SDL_RenderDebugTextFormat(state->renderer, text_x, text_y + line_height * 3.0f,
                                   "save  %s  count %u  free %.1f MB",
                                   storage.user_ready ? (storage.last_save_ok ? "flushed" : "open") : "fail",
                                   (unsigned int)storage.save_count,
                                   (double)storage.space_remaining / (1024.0 * 1024.0)) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 4.0f,
                             storage.user_ready ? storage.pref_path : storage.user_error) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 5.0f,
                             "NORTH/P +1   WEST/R reset   title via SDL_OpenTitleStorage")) {
        return false;
    }

    return true;
}

static bool RenderInputOverlay(AppState *state, int width, int height)
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

static bool RenderFrame(AppState *state)
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

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    AppState *state;
    (void)argc;
    (void)argv;

    SDL_SetAppMetadata("SDL Wii U Render Demo", "1.0", "org.wiiu.sdl-demo");
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state = (AppState *)SDL_calloc(1, sizeof(*state));
    if (!state) {
        SDL_Log("Out of memory");
        return SDL_APP_FAILURE;
    }

    ResetActiveGamepadState(state);
    ResetActiveJoystickState(state);
    ResetRumblePreview(&state->rumble);
    ResetAudioDemoState(&state->audio);
    ResetStorageDemoState(&state->storage);

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

    OpenInitialGamepads(state);
    OpenInitialJoysticks(state);
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
    }

    if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        OpenGamepad(state, event->gdevice.which);
        return SDL_APP_CONTINUE;
    }

    if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
        const int slot = FindGamepadSlot(state, event->gdevice.which);
        if (slot >= 0) {
            CloseGamepadSlot(state, slot);
        }
        return SDL_APP_CONTINUE;
    }

    if (event->type == SDL_EVENT_JOYSTICK_ADDED) {
        OpenJoystick(state, event->jdevice.which);
        return SDL_APP_CONTINUE;
    }

    if (event->type == SDL_EVENT_JOYSTICK_REMOVED) {
        const int slot = FindJoystickSlot(state, event->jdevice.which);
        if (slot >= 0) {
            CloseJoystickSlot(state, slot);
        }
        return SDL_APP_CONTINUE;
    }

    if (event->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        SDL_Gamepad *gamepad = GetActiveGamepad(state);
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_NORTH && gamepad &&
            state->active_gamepad_id == event->gbutton.which) {
            (void)PersistStorageCounter(&state->storage, state->storage.save_count + 1);
            return SDL_APP_CONTINUE;
        }
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_WEST && gamepad &&
            state->active_gamepad_id == event->gbutton.which) {
            (void)PersistStorageCounter(&state->storage, 0);
            return SDL_APP_CONTINUE;
        }
        if (event->gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH && gamepad &&
            state->active_gamepad_id == event->gbutton.which) {
            TriggerGamepadRumble(state, gamepad, 0x5000, 0x9000, 140);
        }
    }

    if (event->type == SDL_EVENT_JOYSTICK_BUTTON_DOWN && !SDL_IsGamepad(event->jbutton.which)) {
        SDL_Joystick *joystick = GetTrackedJoystick(state, event->jbutton.which);
        if (joystick) {
            state->active_joystick_id = event->jbutton.which;
            TriggerJoystickRumble(state, joystick, 0x7000, 0x5000, 180);
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
    PollJoystickState(state);
    UpdateAudioDemo(state);
    return RenderFrame(state) ? SDL_APP_CONTINUE : SDL_APP_FAILURE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    AppState *state = (AppState *)appstate;
    (void)result;

    if (state) {
        CloseAllGamepads(state);
        CloseAllJoysticks(state);
        ShutdownAudioDemo(state);
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
