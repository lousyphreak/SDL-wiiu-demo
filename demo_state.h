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

struct TimeDemoState
{
    SDL_TimerID timer_id;
    SDL_AtomicU32 timer_pulse_count;
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
    TimeDemoState time_demo;
};
