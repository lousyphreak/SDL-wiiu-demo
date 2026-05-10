#include "audio_demo.h"

#include "input_demo.h"
#include "render_utils.h"

void ResetAudioDemoState(AudioDemoState *audio)
{
    if (!audio) {
        return;
    }

    SDL_zero(*audio);
    audio->tone_frequency = 330.0f;
    audio->tone_gain = 0.08f;
}

void ShutdownAudioDemo(AppState *state)
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

bool InitAudioDemo(AppState *state)
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

void UpdateAudioDemo(AppState *state)
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
    bool audio_active;
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
    audio_active = IsAnyInputButtonPressed(state);
    right_x = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_RIGHTX)) : 0.0f;
    right_trigger = has_gamepad ? SDL_max(0.0f, AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))) : 0.0f;
    monitoring_enabled = audio_active &&
                         ((keyboard && keyboard[SDL_SCANCODE_M]) ||
                          (has_gamepad && SDL_GetGamepadButton(active_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)));
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
    if (!audio_active) {
        if (queued_bytes > 0 && !SDL_ClearAudioStream(state->audio.playback_stream)) {
            SDL_Log("SDL_ClearAudioStream(playback) failed: %s", SDL_GetError());
        }
        return;
    }

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

bool RenderAudioWidget(AppState *state, const SDL_FRect &rect)
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
