#pragma once

#include <SDL3/SDL.h>

inline float AxisToFloat(Sint16 value)
{
    return (value >= 0) ? ((float)value / 32767.0f) : ((float)value / 32768.0f);
}

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
    SDL_AsyncIOQueue *async_queue;
    bool title_ready;
    bool user_ready;
    bool last_save_ok;
    bool render_logged;
    bool raw_base_ok;
    bool raw_pref_ok;
    bool raw_title_io_ok;
    bool raw_save_io_ok;
    bool space_remaining_known;
    bool async_started;
    bool async_complete;
    bool async_title_ok;
    bool async_title_close_ok;
    bool async_write_ok;
    bool async_write_close_ok;
    bool async_verify_ok;
    bool async_verify_close_ok;
    Uint32 save_count;
    Uint32 raw_base_entries;
    Uint32 raw_pref_entries;
    Uint64 title_bytes;
    Uint64 space_remaining;
    Uint32 title_signature;
    Uint32 async_title_hash;
    Uint64 last_write_ns;
    Uint64 async_title_bytes;
    Uint64 async_write_bytes;
    Uint64 async_verify_bytes;
    Uint64 async_title_expected_size;
    Uint64 async_verify_expected_size;
    char title_preview[64];
    char async_title_buffer[256];
    char async_verify_buffer[64];
    char base_path[48];
    char current_dir[48];
    char pref_path[72];
    char async_status[96];
    char async_error[96];
    char pref_error[96];
    char raw_io_status[96];
    char title_error[96];
    char user_error[96];
};

struct TimeDemoState
{
    SDL_TimerID timer_id;
    SDL_AtomicU32 timer_pulse_count;
    Uint64 last_instrumentation_log_ns;
    Uint32 observed_timer_pulse_count;
    Uint64 timer_interval_ns;
    Uint64 last_timer_observation_ns;
    Uint64 timer_observed_period_ns;
    Uint64 performance_frequency;
    Uint64 last_performance_counter;
    Uint64 frame_delta_ns;
    Uint64 delay_request_ns;
    Uint64 delay_elapsed_ns;
    SDL_Time current_time;
    SDL_DateTime utc_time;
    SDL_DateTime local_time;
    SDL_DateFormat date_format;
    SDL_TimeFormat time_format;
    Uint64 roundtrip_error_ns;
    bool locale_ready;
    bool current_time_ready;
    bool utc_ready;
    bool local_ready;
    bool roundtrip_ready;
    bool render_logged;
};

struct CameraDemoState
{
    SDL_Camera *camera;
    SDL_CameraID device_id;
    SDL_Texture *preview_texture;
    SDL_CameraSpec spec;
    Uint64 last_frame_timestamp_ns;
    SDL_CameraPermissionState permission;
    bool available;
    bool enabled;
    bool frame_ready;
    char status_text[96];
};

struct PlatformTextUiDemoState
{
    bool screen_keyboard_supported;
    bool awaiting_virtual_text;
    char keyboard_status[96];
    char entered_text[64];
    char last_text[96];
    char message_box_status[96];
    char url_status[96];
};

struct AppState
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *background;
    SDL_Texture *showcase_texture;
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
    TimeDemoState time_demo;
    CameraDemoState camera_demo;
    PlatformTextUiDemoState platform_ui;
};
