#include "time_demo.h"

#include "render_utils.h"

namespace
{

Uint64 CounterDeltaToNanoseconds(Uint64 delta, Uint64 frequency)
{
    if (frequency == 0) {
        return 0;
    }

    return ((delta / frequency) * SDL_NS_PER_SECOND) +
           (((delta % frequency) * SDL_NS_PER_SECOND) / frequency);
}

Uint64 AbsoluteTimeDelta(SDL_Time a, SDL_Time b)
{
    return (a >= b) ? (Uint64)(a - b) : (Uint64)(b - a);
}

void FormatDateTimeLine(char *destination, size_t destination_size, const char *label, const SDL_DateTime &dt, bool ready)
{
    int offset_hours;
    int offset_minutes;
    const char sign = (dt.utc_offset < 0) ? '-' : '+';
    const int absolute_offset = (dt.utc_offset < 0) ? -dt.utc_offset : dt.utc_offset;

    if (!destination || destination_size == 0) {
        return;
    }

    if (!ready) {
        SDL_strlcpy(destination, "time unavailable", destination_size);
        return;
    }

    offset_hours = absolute_offset / 3600;
    offset_minutes = (absolute_offset % 3600) / 60;
    SDL_snprintf(destination, destination_size,
                 "%s %04d-%02d-%02d %02d:%02d:%02d.%03d %c%02d:%02d",
                 label,
                 dt.year, dt.month, dt.day,
                 dt.hour, dt.minute, dt.second,
                 dt.nanosecond / SDL_NS_PER_MS,
                 sign, offset_hours, offset_minutes);
}

const char *GetDateFormatLabel(SDL_DateFormat format)
{
    switch (format) {
    case SDL_DATE_FORMAT_DDMMYYYY:
        return "DMY";
    case SDL_DATE_FORMAT_MMDDYYYY:
        return "MDY";
    case SDL_DATE_FORMAT_YYYYMMDD:
    default:
        return "YMD";
    }
}

const char *GetTimeFormatLabel(SDL_TimeFormat format)
{
    return (format == SDL_TIME_FORMAT_12HR) ? "12h" : "24h";
}

Uint64 SDLCALL TimeDemoTimerCallback(void *userdata, SDL_TimerID timer_id, Uint64 interval)
{
    TimeDemoState *time_demo = static_cast<TimeDemoState *>(userdata);
    (void)timer_id;

    if (time_demo) {
        SDL_AddAtomicU32(&time_demo->timer_pulse_count, 1);
    }

    return interval;
}

} // namespace

void ResetTimeDemoState(TimeDemoState *time_demo)
{
    if (!time_demo) {
        return;
    }

    SDL_zero(*time_demo);
    time_demo->timer_interval_ns = SDL_MS_TO_NS(250);
    time_demo->delay_request_ns = SDL_MS_TO_NS(2);
}

bool InitTimeDemo(AppState *state)
{
    TimeDemoState *time_demo;
    Uint64 start_counter;
    Uint64 end_counter;

    if (!state) {
        return false;
    }

    time_demo = &state->time_demo;
    time_demo->performance_frequency = SDL_GetPerformanceFrequency();
    time_demo->locale_ready = SDL_GetDateTimeLocalePreferences(&time_demo->date_format, &time_demo->time_format);

    start_counter = SDL_GetPerformanceCounter();
    SDL_DelayPrecise(time_demo->delay_request_ns);
    end_counter = SDL_GetPerformanceCounter();
    time_demo->delay_elapsed_ns = CounterDeltaToNanoseconds(end_counter - start_counter, time_demo->performance_frequency);

    time_demo->timer_id = SDL_AddTimerNS(time_demo->timer_interval_ns, TimeDemoTimerCallback, time_demo);
    if (!time_demo->timer_id) {
        SDL_Log("SDL_AddTimerNS failed: %s", SDL_GetError());
        return false;
    }

    SDL_Log("[time] init freq=%.1fMHz locale=%s/%s timer=%.2fms delay=%.2fms",
            (double)time_demo->performance_frequency / 1000000.0,
            time_demo->locale_ready ? GetDateFormatLabel(time_demo->date_format) : "---",
            time_demo->locale_ready ? GetTimeFormatLabel(time_demo->time_format) : "---",
            (double)time_demo->timer_interval_ns / (double)SDL_NS_PER_MS,
            (double)time_demo->delay_elapsed_ns / (double)SDL_NS_PER_MS);

    UpdateTimeDemo(state);
    return true;
}

void ShutdownTimeDemo(AppState *state)
{
    if (!state) {
        return;
    }

    if (state->time_demo.timer_id) {
        SDL_RemoveTimer(state->time_demo.timer_id);
        state->time_demo.timer_id = 0;
    }
}

void UpdateTimeDemo(AppState *state)
{
    TimeDemoState *time_demo;
    Uint64 counter;
    Uint32 pulse_count;

    if (!state) {
        return;
    }

    time_demo = &state->time_demo;
    time_demo->performance_frequency = SDL_GetPerformanceFrequency();

    counter = SDL_GetPerformanceCounter();
    if (time_demo->last_performance_counter != 0) {
        time_demo->frame_delta_ns = CounterDeltaToNanoseconds(counter - time_demo->last_performance_counter,
                                                              time_demo->performance_frequency);
    }
    time_demo->last_performance_counter = counter;

    pulse_count = SDL_GetAtomicU32(&time_demo->timer_pulse_count);
    if (pulse_count != time_demo->observed_timer_pulse_count) {
        const Uint64 now = SDL_GetTicksNS();
        const Uint32 pulse_delta = pulse_count - time_demo->observed_timer_pulse_count;

        if (time_demo->observed_timer_pulse_count != 0 && time_demo->last_timer_observation_ns != 0 && pulse_delta > 0) {
            time_demo->timer_observed_period_ns = (now - time_demo->last_timer_observation_ns) / pulse_delta;
        }

        time_demo->last_timer_observation_ns = now;
        time_demo->observed_timer_pulse_count = pulse_count;
    }

    time_demo->current_time_ready = SDL_GetCurrentTime(&time_demo->current_time);
    time_demo->utc_ready = false;
    time_demo->local_ready = false;
    time_demo->roundtrip_ready = false;
    time_demo->roundtrip_error_ns = 0;

    if (!time_demo->current_time_ready) {
        return;
    }

    time_demo->utc_ready = SDL_TimeToDateTime(time_demo->current_time, &time_demo->utc_time, false);
    time_demo->local_ready = SDL_TimeToDateTime(time_demo->current_time, &time_demo->local_time, true);

    if (time_demo->local_ready) {
        SDL_Time roundtrip_time;

        if (SDL_DateTimeToTime(&time_demo->local_time, &roundtrip_time)) {
            time_demo->roundtrip_ready = true;
            time_demo->roundtrip_error_ns = AbsoluteTimeDelta(roundtrip_time, time_demo->current_time);
        }
    }

    {
        const Uint64 now = SDL_GetTicksNS();
        if (time_demo->last_instrumentation_log_ns == 0 ||
            (now - time_demo->last_instrumentation_log_ns) >= SDL_SECONDS_TO_NS(5)) {
            SDL_Log("[time] pulse=%u frame=%.2fms roundtrip=%.3fms current=%s",
                    (unsigned int)time_demo->observed_timer_pulse_count,
                    (double)time_demo->frame_delta_ns / (double)SDL_NS_PER_MS,
                    (double)time_demo->roundtrip_error_ns / (double)SDL_NS_PER_MS,
                    time_demo->current_time_ready ? "ready" : "unavailable");
            time_demo->last_instrumentation_log_ns = now;
        }
    }
}

bool RenderTimeWidget(AppState *state, const SDL_FRect &rect)
{
    TimeDemoState *time_demo;
    const float padding = 10.0f;
    const float line_height = (float)SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE + 2.0f;
    const float text_x = rect.x + padding;
    const float text_y = rect.y + 10.0f;
    if (!state) {
        return false;
    }

    time_demo = &state->time_demo;

    const Uint8 accent_r = time_demo->current_time_ready ? 120 : 255;
    const Uint8 accent_g = time_demo->local_ready ? 220 : 120;
    const Uint8 accent_b = time_demo->roundtrip_ready ? 255 : 160;
    char utc_line[96];
    char local_line[96];

    FormatDateTimeLine(utc_line, sizeof(utc_line), "utc", time_demo->utc_time, time_demo->utc_ready);
    FormatDateTimeLine(local_line, sizeof(local_line), "loc", time_demo->local_time, time_demo->local_ready);

    if (!time_demo->render_logged) {
        SDL_Log("[time] render-window rect=(%.0f,%.0f %.0fx%.0f)",
                rect.x, rect.y, rect.w, rect.h);
        time_demo->render_logged = true;
    }

    if (!SetColor(state->renderer, 10, 12, 34, 228) ||
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

    if (!SetColor(state->renderer, 230, 240, 255, 255) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y, "time window") ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height, utc_line) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 2.0f, local_line) ||
        !SDL_RenderDebugTextFormat(state->renderer, text_x, text_y + line_height * 3.0f,
                                   "fmt %s %s  rt %.3fms",
                                   time_demo->locale_ready ? GetDateFormatLabel(time_demo->date_format) : "---",
                                   time_demo->locale_ready ? GetTimeFormatLabel(time_demo->time_format) : "---",
                                   (double)time_demo->roundtrip_error_ns / (double)SDL_NS_PER_MS) ||
        !SDL_RenderDebugTextFormat(state->renderer, text_x, text_y + line_height * 4.0f,
                                   "perf %.1fMHz",
                                   (double)time_demo->performance_frequency / 1000000.0) ||
        !SDL_RenderDebugTextFormat(state->renderer, text_x, text_y + line_height * 5.0f,
                                   "up %.1fs  frm %.2fms",
                                   (double)SDL_GetTicksNS() / (double)SDL_NS_PER_SECOND,
                                   (double)time_demo->frame_delta_ns / (double)SDL_NS_PER_MS) ||
        !SDL_RenderDebugTextFormat(state->renderer, text_x, text_y + line_height * 6.0f,
                                   "dly %.2fms tmr %u @ %.2fms",
                                   (double)time_demo->delay_elapsed_ns / (double)SDL_NS_PER_MS,
                                   (unsigned int)time_demo->observed_timer_pulse_count,
                                   (double)(time_demo->timer_observed_period_ns ? time_demo->timer_observed_period_ns : time_demo->timer_interval_ns) / (double)SDL_NS_PER_MS)) {
        return false;
    }

    return true;
}
