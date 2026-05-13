#include "audio_demo.h"

#include "input_demo.h"
#include "render_utils.h"
#include <SDL3_mixer/SDL_mixer.h>

namespace
{

constexpr int kMaxDemoChannels = 6;
constexpr float kPanDeadzone = 0.18f;

struct SpatialSpeaker
{
    int channel;
    float x;
    float y;
};

float ApplyPanDeadzone(float raw_x, float raw_y, float *pan_x, float *pan_y)
{
    const float length = SDL_sqrtf((raw_x * raw_x) + (raw_y * raw_y));

    if (!pan_x || !pan_y) {
        return 0.0f;
    }

    if (length <= kPanDeadzone) {
        *pan_x = 0.0f;
        *pan_y = 0.0f;
        return 0.0f;
    }

    if (length >= 1.0f) {
        const float inv_length = (length > 0.0f) ? (1.0f / length) : 0.0f;
        *pan_x = raw_x * inv_length;
        *pan_y = raw_y * inv_length;
        return 1.0f;
    }

    {
        const float scaled_length = (length - kPanDeadzone) / (1.0f - kPanDeadzone);
        const float scale = scaled_length / length;
        *pan_x = raw_x * scale;
        *pan_y = raw_y * scale;
        return scaled_length;
    }
}

const char *GetPlaybackModeLabel(int channels)
{
    switch (channels) {
    case 1:
        return "mono";
    case 2:
        return "stereo";
    case 4:
        return "quad";
    case 5:
        return "4.1";
    case 6:
        return "5.1";
    default:
        return "audio";
    }
}

void BuildSpatialMix(int playback_channels, float pan_x, float pan_y, float *gains)
{
    float amount;

    if (!gains) {
        return;
    }

    for (int i = 0; i < kMaxDemoChannels; ++i) {
        gains[i] = 0.0f;
    }

    if (playback_channels <= 1) {
        gains[0] = 1.0f;
        return;
    }

    if (playback_channels == 2) {
        const float left_weight = SDL_clamp((1.0f - pan_x) * 0.5f, 0.0f, 1.0f);
        const float right_weight = SDL_clamp((1.0f + pan_x) * 0.5f, 0.0f, 1.0f);
        gains[0] = SDL_sqrtf(left_weight);
        gains[1] = SDL_sqrtf(right_weight);
        return;
    }

    amount = SDL_min(SDL_sqrtf((pan_x * pan_x) + (pan_y * pan_y)), 1.0f);

    {
        SpatialSpeaker speakers[5];
        float neutral[kMaxDemoChannels];
        float directional[kMaxDemoChannels];
        int directional_count = 0;
        int lfe_channel = -1;
        int front_left = 0;
        int front_right = 1;
        int front_center = -1;
        int back_left = -1;
        int back_right = -1;

        for (int i = 0; i < kMaxDemoChannels; ++i) {
            neutral[i] = 0.0f;
            directional[i] = 0.0f;
        }

        switch (playback_channels) {
        case 3:
            lfe_channel = 2;
            break;
        case 4:
            back_left = 2;
            back_right = 3;
            break;
        case 5:
            lfe_channel = 2;
            back_left = 3;
            back_right = 4;
            break;
        default:
            front_center = 2;
            lfe_channel = 3;
            back_left = 4;
            back_right = 5;
            break;
        }

        speakers[directional_count++] = { front_left, -0.70710678f, 0.70710678f };
        speakers[directional_count++] = { front_right, 0.70710678f, 0.70710678f };

        if (front_center >= 0) {
            speakers[directional_count++] = { front_center, 0.0f, 1.0f };
        }

        if (back_left >= 0 && back_right >= 0) {
            speakers[directional_count++] = { back_left, -0.70710678f, -0.70710678f };
            speakers[directional_count++] = { back_right, 0.70710678f, -0.70710678f };
        }

        for (int i = 0; i < directional_count; ++i) {
            neutral[speakers[i].channel] = 1.0f / (float)directional_count;
        }

        if (amount > 0.0f) {
            float sum = 0.0f;

            for (int i = 0; i < directional_count; ++i) {
                const float dot = SDL_max(0.0f, (pan_x * speakers[i].x) + (pan_y * speakers[i].y));
                const float weight = dot * dot;
                directional[speakers[i].channel] = weight;
                sum += weight;
            }

            if (sum > 0.0f) {
                for (int i = 0; i < directional_count; ++i) {
                    directional[speakers[i].channel] /= sum;
                }
            } else {
                for (int i = 0; i < directional_count; ++i) {
                    directional[speakers[i].channel] = neutral[speakers[i].channel];
                }
            }
        } else {
            for (int i = 0; i < directional_count; ++i) {
                directional[speakers[i].channel] = neutral[speakers[i].channel];
            }
        }

        for (int i = 0; i < directional_count; ++i) {
            const int channel = speakers[i].channel;
            const float weight = (neutral[channel] * (1.0f - amount)) + (directional[channel] * amount);
            gains[channel] = SDL_sqrtf(SDL_max(weight, 0.0f));
        }

        if (lfe_channel >= 0) {
            gains[lfe_channel] = 0.16f;
        }
    }
}

} // namespace

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

    if (state->audio.mixer_available) {
        MIX_Quit();
        state->audio.mixer_available = false;
        state->audio.mixer_decoder_count = 0;
    }
}

bool InitAudioDemo(AppState *state)
{
    SDL_AudioDeviceID playback_device;
    SDL_AudioSpec playback_device_spec;
    SDL_AudioSpec requested_playback_spec;
    int playback_map_count = 0;
    int *playback_map = nullptr;

    if (!state) {
        return false;
    }

    SDL_zero(requested_playback_spec);
    requested_playback_spec.format = SDL_AUDIO_F32;
    requested_playback_spec.channels = 6;
    requested_playback_spec.freq = 48000;

    playback_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &requested_playback_spec);
    if (!playback_device) {
        SDL_Log("SDL_OpenAudioDevice(default playback) failed: %s", SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(playback_device);
    SDL_GetAudioDeviceFormat(playback_device, &playback_device_spec, nullptr);
    playback_map = SDL_GetAudioDeviceChannelMap(playback_device, &playback_map_count);
    if (playback_map) {
        SDL_Log("[audio_demo] playback requested fmt=0x%x ch=%u freq=%d -> opened fmt=0x%x ch=%u freq=%d map=[%d,%d,%d,%d,%d,%d]",
                (unsigned int)requested_playback_spec.format,
                (unsigned int)requested_playback_spec.channels,
                requested_playback_spec.freq,
                (unsigned int)playback_device_spec.format,
                (unsigned int)playback_device_spec.channels,
                playback_device_spec.freq,
                playback_map_count > 0 ? playback_map[0] : -1,
                playback_map_count > 1 ? playback_map[1] : -1,
                playback_map_count > 2 ? playback_map[2] : -1,
                playback_map_count > 3 ? playback_map[3] : -1,
                playback_map_count > 4 ? playback_map[4] : -1,
                playback_map_count > 5 ? playback_map[5] : -1);
        SDL_free(playback_map);
    } else {
        SDL_Log("[audio_demo] playback requested fmt=0x%x ch=%u freq=%d -> opened fmt=0x%x ch=%u freq=%d map=default",
                (unsigned int)requested_playback_spec.format,
                (unsigned int)requested_playback_spec.channels,
                requested_playback_spec.freq,
                (unsigned int)playback_device_spec.format,
                (unsigned int)playback_device_spec.channels,
                playback_device_spec.freq);
    }

    state->audio.mix_spec.format = SDL_AUDIO_F32;
    state->audio.mix_spec.channels = playback_device_spec.channels;
    state->audio.mix_spec.freq = playback_device_spec.freq;
    state->audio.playback_channels = (int)playback_device_spec.channels;
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

    if (!MIX_Init()) {
        SDL_Log("SDL_mixer initialization failed: %s", SDL_GetError());
        return false;
    }

    state->audio.mixer_available = true;
    state->audio.mixer_decoder_count = MIX_GetNumAudioDecoders();
    SDL_Log("[audio_demo] SDL_mixer ready with %d decoder(s)", state->audio.mixer_decoder_count);
    for (int i = 0; i < state->audio.mixer_decoder_count; ++i) {
        const char *name = MIX_GetAudioDecoder(i);
        SDL_Log("[audio_demo]   decoder[%d] = %s", i, name ? name : "(null)");
    }

    return true;
}

void UpdateAudioDemo(AppState *state)
{
    constexpr float tau = 6.28318530717958647692f;
    constexpr int tone_chunk_frames = 512;
    constexpr int capture_chunk_frames = 256;

    float tone_buffer[tone_chunk_frames * kMaxDemoChannels];
    float capture_buffer[capture_chunk_frames * kMaxDemoChannels];
    float loopback_buffer[capture_chunk_frames * kMaxDemoChannels];
    float channel_gains[kMaxDemoChannels];
    SDL_Gamepad *active_gamepad;
    const bool *keyboard;
    bool has_gamepad;
    bool audio_active;
    bool monitoring_enabled;
    int playback_channels;
    float left_x;
    float left_y;
    float pan_x;
    float pan_y;
    float right_x;
    float right_trigger;
    int queued_bytes;
    int target_queue_bytes;

    if (!state || !state->audio.playback_stream) {
        return;
    }

    active_gamepad = GetActiveGamepad(state);
    keyboard = SDL_GetKeyboardState(nullptr);
    has_gamepad = active_gamepad && SDL_GamepadConnected(active_gamepad);
    audio_active = IsAnyInputButtonPressed(state);
    left_x = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_LEFTX)) : 0.0f;
    left_y = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_LEFTY)) : 0.0f;
    right_x = has_gamepad ? AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_RIGHTX)) : 0.0f;
    right_trigger = has_gamepad ? SDL_max(0.0f, AxisToFloat(SDL_GetGamepadAxis(active_gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))) : 0.0f;
    monitoring_enabled = audio_active &&
                         ((keyboard && keyboard[SDL_SCANCODE_M]) ||
                          (has_gamepad && SDL_GetGamepadButton(active_gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)));
    state->audio.monitoring_enabled = monitoring_enabled;
    playback_channels = SDL_max((int)state->audio.mix_spec.channels, 1);
    state->audio.playback_channels = playback_channels;
    queued_bytes = SDL_GetAudioStreamQueued(state->audio.playback_stream);
    target_queue_bytes = tone_chunk_frames * 6 * (int)sizeof(float) * playback_channels;
    state->audio.pan_amount = ApplyPanDeadzone(left_x, -left_y, &pan_x, &pan_y);
    state->audio.pan_x = pan_x;
    state->audio.pan_y = pan_y;
    BuildSpatialMix(playback_channels, pan_x, pan_y, channel_gains);

    state->audio.tone_frequency = 220.0f + (right_x + 1.0f) * 220.0f;
    state->audio.tone_gain += ((0.05f + right_trigger * 0.18f) - state->audio.tone_gain) * 0.18f;
    state->audio.capture_level *= 0.86f;
    state->audio.playback_level *= 0.90f;
    state->audio.loopback_level *= 0.82f;

    if (state->audio.capture_stream) {
        const int minimum_capture_bytes = capture_chunk_frames * (int)sizeof(float) * playback_channels;

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

            for (int channel = 0; channel < playback_channels; ++channel) {
                const float value = sample * channel_gains[channel];
                tone_buffer[(i * playback_channels) + channel] = value;
                tone_peak = SDL_max(tone_peak, SDL_fabsf(value));
            }

            phase += phase_step;
            if (phase >= tau) {
                phase -= tau;
            }
        }

        state->audio.tone_phase = phase;
        state->audio.playback_level = SDL_max(state->audio.playback_level, tone_peak);

        if (!SDL_PutAudioStreamData(state->audio.playback_stream,
                                    tone_buffer,
                                    tone_chunk_frames * playback_channels * (int)sizeof(float))) {
            SDL_Log("SDL_PutAudioStreamData(tone) failed: %s", SDL_GetError());
            break;
        }

        queued_bytes += tone_chunk_frames * playback_channels * (int)sizeof(float);
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
    const float pan_x = state->audio.pan_x;
    const float pan_y = (state->audio.playback_channels >= 4) ? state->audio.pan_y : 0.0f;
    const float pan_amount = SDL_clamp(state->audio.pan_amount, 0.0f, 1.0f);
    const SDL_FRect tone_bar = { rect.x + padding, rect.y + 20.0f, meter_w, bar_h };
    const SDL_FRect playback_bar = { tone_bar.x, tone_bar.y + bar_h + bar_gap, meter_w, bar_h };
    const SDL_FRect capture_bar = { tone_bar.x, playback_bar.y + bar_h + bar_gap, meter_w, bar_h };
    const SDL_FRect loopback_bar = { tone_bar.x, capture_bar.y + bar_h + bar_gap, meter_w, bar_h };
    const SDL_FRect pan_rect = { rect.x + rect.w - 62.0f, rect.y + rect.h - 54.0f, 44.0f, 44.0f };
    const float pan_center_x = pan_rect.x + pan_rect.w * 0.5f;
    const float pan_center_y = pan_rect.y + pan_rect.h * 0.5f;
    const float pan_scale = (pan_rect.w - 10.0f) * 0.5f;
    char mode_label[64];

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

    if (state->audio.mixer_available) {
        SDL_snprintf(mode_label,
                     sizeof(mode_label),
                     "L pan %s mix %d dec",
                     GetPlaybackModeLabel(state->audio.playback_channels),
                     state->audio.mixer_decoder_count);
    } else {
        SDL_snprintf(mode_label, sizeof(mode_label), "L pan %s mix off", GetPlaybackModeLabel(state->audio.playback_channels));
    }

    if (!SetColor(state->renderer, 18, 26, 54, 255) ||
        !SDL_RenderFillRect(state->renderer, &pan_rect) ||
        !SetColor(state->renderer, 65, 96, 150, 255) ||
        !SDL_RenderRect(state->renderer, &pan_rect) ||
        !SetColor(state->renderer, 55, 84, 132, 255) ||
        !SDL_RenderLine(state->renderer, pan_center_x, pan_rect.y + 2.0f, pan_center_x, pan_rect.y + pan_rect.h - 2.0f) ||
        !SDL_RenderLine(state->renderer, pan_rect.x + 2.0f, pan_center_y, pan_rect.x + pan_rect.w - 2.0f, pan_center_y)) {
        return false;
    }

    {
        const SDL_FRect listener = { pan_center_x - 3.0f, pan_center_y - 3.0f, 6.0f, 6.0f };
        const SDL_FRect front_left = { pan_rect.x + 3.0f, pan_rect.y + 3.0f, 4.0f, 4.0f };
        const SDL_FRect front_right = { pan_rect.x + pan_rect.w - 7.0f, pan_rect.y + 3.0f, 4.0f, 4.0f };
        const SDL_FRect back_left = { pan_rect.x + 3.0f, pan_rect.y + pan_rect.h - 7.0f, 4.0f, 4.0f };
        const SDL_FRect back_right = { pan_rect.x + pan_rect.w - 7.0f, pan_rect.y + pan_rect.h - 7.0f, 4.0f, 4.0f };

        if (!SetColor(state->renderer, 80, 110, 180, 255) ||
            !SDL_RenderFillRect(state->renderer, &front_left) ||
            !SDL_RenderFillRect(state->renderer, &front_right) ||
            !SDL_RenderFillRect(state->renderer, &listener)) {
            return false;
        }

        if (state->audio.playback_channels >= 4 &&
            (!SDL_RenderFillRect(state->renderer, &back_left) ||
             !SDL_RenderFillRect(state->renderer, &back_right))) {
            return false;
        }

        if (state->audio.playback_channels >= 6) {
            const SDL_FRect front_center = { pan_center_x - 2.0f, pan_rect.y + 2.0f, 4.0f, 4.0f };
            if (!SDL_RenderFillRect(state->renderer, &front_center)) {
                return false;
            }
        }
    }

    if (pan_amount > 0.0f &&
        (!SetColor(state->renderer, 255, 196, 120, 255) ||
         !SDL_RenderLine(state->renderer,
                         pan_center_x,
                         pan_center_y,
                         pan_center_x + pan_x * pan_scale,
                         pan_center_y - pan_y * pan_scale))) {
        return false;
    }

    if (!SetColor(state->renderer,
                  (Uint8)(120 + playback_amount * 110.0f),
                  (Uint8)(140 + capture_amount * 60.0f),
                  (Uint8)(255 - loopback_amount * 70.0f),
                  255) ||
        !FillCircle(state->renderer,
                    pan_center_x + pan_x * pan_scale,
                    pan_center_y - pan_y * pan_scale,
                    4.0f + playback_amount * 3.0f) ||
        !SetColor(state->renderer, 215, 225, 255, 255) ||
        !SDL_RenderDebugText(state->renderer, rect.x + padding, rect.y + rect.h - 14.0f, mode_label)) {
        return false;
    }

    return true;
}
