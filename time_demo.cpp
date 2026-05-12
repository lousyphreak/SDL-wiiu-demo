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

void ResetTimingMetricStats(TimingMetricStats *stats)
{
    if (!stats) {
        return;
    }

    SDL_zero(*stats);
}

void AddTimingMetricSample(TimingMetricStats *stats, Uint64 ns)
{
    if (!stats) {
        return;
    }

    if (stats->sample_count == 0 || ns < stats->min_ns) {
        stats->min_ns = ns;
    }
    if (stats->sample_count == 0 || ns > stats->max_ns) {
        stats->max_ns = ns;
    }

    stats->total_ns += ns;
    ++stats->sample_count;
}

double TimingMetricAverageMs(const TimingMetricStats &stats)
{
    if (stats.sample_count == 0) {
        return 0.0;
    }

    return ((double)stats.total_ns / (double)stats.sample_count) / (double)SDL_NS_PER_MS;
}

double TimingMetricMinMs(const TimingMetricStats &stats)
{
    return (stats.sample_count > 0) ? ((double)stats.min_ns / (double)SDL_NS_PER_MS) : 0.0;
}

double TimingMetricMaxMs(const TimingMetricStats &stats)
{
    return (stats.sample_count > 0) ? ((double)stats.max_ns / (double)SDL_NS_PER_MS) : 0.0;
}

void ResetTimingWindow(TimeDemoState *time_demo)
{
    if (!time_demo) {
        return;
    }

    ResetTimingMetricStats(&time_demo->callback_stats);
    ResetTimingMetricStats(&time_demo->idle_stats);
    ResetTimingMetricStats(&time_demo->update_stats);
    ResetTimingMetricStats(&time_demo->main_render_stats);
    ResetTimingMetricStats(&time_demo->drc_render_stats);
    ResetTimingMetricStats(&time_demo->frame_total_stats);
    ResetTimingMetricStats(&time_demo->main_cache_stats);
    ResetTimingMetricStats(&time_demo->main_scene_stats);
    ResetTimingMetricStats(&time_demo->main_showcase_stats);
    ResetTimingMetricStats(&time_demo->main_camera_stats);
    ResetTimingMetricStats(&time_demo->main_overlay_stats);
    ResetTimingMetricStats(&time_demo->main_cursor_stats);
    ResetTimingMetricStats(&time_demo->main_present_stats);
    ResetTimingMetricStats(&time_demo->drc_scene_stats);
    ResetTimingMetricStats(&time_demo->drc_present_stats);
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

void RefreshTimeDemoPacingState(AppState *state)
{
    TimeDemoState *time_demo;
    const char *callback_rate_hint;

    if (!state) {
        return;
    }

    time_demo = &state->time_demo;
    callback_rate_hint = SDL_GetHint(SDL_HINT_MAIN_CALLBACK_RATE);
    time_demo->main_callback_wait_event = callback_rate_hint && (SDL_strcmp(callback_rate_hint, "waitevent") == 0);
    time_demo->main_callback_rate_hz = 0.0;

    if (!time_demo->main_callback_wait_event && callback_rate_hint && *callback_rate_hint) {
        time_demo->main_callback_rate_hz = SDL_atof(callback_rate_hint);
        if (time_demo->main_callback_rate_hz < 0.0) {
            time_demo->main_callback_rate_hz = 0.0;
        }
    }

    time_demo->renderer_vsync = 0;
    time_demo->renderer_vsync_known = state->renderer && SDL_GetRenderVSync(state->renderer, &time_demo->renderer_vsync);
}

void FormatLoopModeLabel(char *destination, size_t destination_size, const TimeDemoState &time_demo)
{
    if (!destination || destination_size == 0) {
        return;
    }

    if (time_demo.main_callback_wait_event) {
        SDL_strlcpy(destination, "waitevent", destination_size);
    } else if (time_demo.main_callback_rate_hz > 0.0) {
        SDL_snprintf(destination, destination_size, "%.2fHz", time_demo.main_callback_rate_hz);
    } else {
        SDL_strlcpy(destination, "video-paced", destination_size);
    }
}

void LogTimingWindow(TimeDemoState *time_demo)
{
    char loop_mode[32];
    double callback_avg_ms;
    double fps;

    if (!time_demo) {
        return;
    }

    FormatLoopModeLabel(loop_mode, sizeof(loop_mode), *time_demo);
    callback_avg_ms = TimingMetricAverageMs(time_demo->callback_stats);
    fps = (callback_avg_ms > 0.0) ? (1000.0 / callback_avg_ms) : 0.0;

    SDL_Log("[time] pace=%s vsync=%s fps=%.2f cb(avg/min/max)=%.2f/%.2f/%.2fms idle(avg/max)=%.2f/%.2fms update(avg/max)=%.2f/%.2fms main(avg/max)=%.2f/%.2fms drc(avg/max)=%.2f/%.2fms frame(avg/max)=%.2f/%.2fms pulse=%u timer=%.2fms rt=%.3fms current=%s",
            loop_mode,
            time_demo->renderer_vsync_known ? (time_demo->renderer_vsync ? "on" : "off") : "?",
            fps,
            callback_avg_ms,
            TimingMetricMinMs(time_demo->callback_stats),
            TimingMetricMaxMs(time_demo->callback_stats),
            TimingMetricAverageMs(time_demo->idle_stats),
            TimingMetricMaxMs(time_demo->idle_stats),
            TimingMetricAverageMs(time_demo->update_stats),
            TimingMetricMaxMs(time_demo->update_stats),
            TimingMetricAverageMs(time_demo->main_render_stats),
            TimingMetricMaxMs(time_demo->main_render_stats),
            TimingMetricAverageMs(time_demo->drc_render_stats),
            TimingMetricMaxMs(time_demo->drc_render_stats),
            TimingMetricAverageMs(time_demo->frame_total_stats),
            TimingMetricMaxMs(time_demo->frame_total_stats),
            (unsigned int)time_demo->observed_timer_pulse_count,
            (double)(time_demo->timer_observed_period_ns ? time_demo->timer_observed_period_ns : time_demo->timer_interval_ns) / (double)SDL_NS_PER_MS,
            (double)time_demo->roundtrip_error_ns / (double)SDL_NS_PER_MS,
            time_demo->current_time_ready ? "ready" : "unavailable");
    SDL_Log("[time] stages cache(avg/max)=%.2f/%.2fms scene(avg/max)=%.2f/%.2fms show(avg/max)=%.2f/%.2fms camera(avg/max)=%.2f/%.2fms overlay(avg/max)=%.2f/%.2fms cursor(avg/max)=%.2f/%.2fms present(avg/max)=%.2f/%.2fms drc-draw(avg/max)=%.2f/%.2fms drc-present(avg/max)=%.2f/%.2fms",
            TimingMetricAverageMs(time_demo->main_cache_stats),
            TimingMetricMaxMs(time_demo->main_cache_stats),
            TimingMetricAverageMs(time_demo->main_scene_stats),
            TimingMetricMaxMs(time_demo->main_scene_stats),
            TimingMetricAverageMs(time_demo->main_showcase_stats),
            TimingMetricMaxMs(time_demo->main_showcase_stats),
            TimingMetricAverageMs(time_demo->main_camera_stats),
            TimingMetricMaxMs(time_demo->main_camera_stats),
            TimingMetricAverageMs(time_demo->main_overlay_stats),
            TimingMetricMaxMs(time_demo->main_overlay_stats),
            TimingMetricAverageMs(time_demo->main_cursor_stats),
            TimingMetricMaxMs(time_demo->main_cursor_stats),
            TimingMetricAverageMs(time_demo->main_present_stats),
            TimingMetricMaxMs(time_demo->main_present_stats),
            TimingMetricAverageMs(time_demo->drc_scene_stats),
            TimingMetricMaxMs(time_demo->drc_scene_stats),
            TimingMetricAverageMs(time_demo->drc_present_stats),
            TimingMetricMaxMs(time_demo->drc_present_stats));

    ResetTimingWindow(time_demo);
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
    ResetTimingWindow(time_demo);
}

bool InitTimeDemo(AppState *state)
{
    TimeDemoState *time_demo;
    Uint64 start_counter;
    Uint64 end_counter;
    char loop_mode[32];

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

    RefreshTimeDemoPacingState(state);
    FormatLoopModeLabel(loop_mode, sizeof(loop_mode), *time_demo);
    SDL_Log("[time] init freq=%.1fMHz locale=%s/%s timer=%.2fms delay=%.2fms pace=%s vsync=%s",
            (double)time_demo->performance_frequency / 1000000.0,
            time_demo->locale_ready ? GetDateFormatLabel(time_demo->date_format) : "---",
            time_demo->locale_ready ? GetTimeFormatLabel(time_demo->time_format) : "---",
            (double)time_demo->timer_interval_ns / (double)SDL_NS_PER_MS,
            (double)time_demo->delay_elapsed_ns / (double)SDL_NS_PER_MS,
            loop_mode,
            time_demo->renderer_vsync_known ? (time_demo->renderer_vsync ? "on" : "off") : "?");

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

void BeginTimeDemoFrame(AppState *state)
{
    TimeDemoState *time_demo;
    Uint64 counter;

    if (!state) {
        return;
    }

    time_demo = &state->time_demo;
    RefreshTimeDemoPacingState(state);
    time_demo->performance_frequency = SDL_GetPerformanceFrequency();

    counter = SDL_GetPerformanceCounter();
    time_demo->frame_start_counter = counter;
    time_demo->update_complete_counter = 0;
    time_demo->main_render_complete_counter = 0;
    time_demo->update_duration_ns = 0;
    time_demo->main_render_duration_ns = 0;
    time_demo->drc_render_duration_ns = 0;
    time_demo->frame_total_ns = 0;
    time_demo->main_cache_duration_ns = 0;
    time_demo->main_scene_duration_ns = 0;
    time_demo->main_showcase_duration_ns = 0;
    time_demo->main_camera_duration_ns = 0;
    time_demo->main_overlay_duration_ns = 0;
    time_demo->main_cursor_duration_ns = 0;
    time_demo->main_present_duration_ns = 0;
    time_demo->drc_scene_duration_ns = 0;
    time_demo->drc_present_duration_ns = 0;

    if (time_demo->last_frame_start_counter != 0) {
        time_demo->frame_delta_ns = CounterDeltaToNanoseconds(counter - time_demo->last_frame_start_counter,
                                                              time_demo->performance_frequency);
        time_demo->interframe_idle_ns = (time_demo->frame_delta_ns > time_demo->last_completed_frame_total_ns)
                                            ? (time_demo->frame_delta_ns - time_demo->last_completed_frame_total_ns)
                                            : 0;
        AddTimingMetricSample(&time_demo->callback_stats, time_demo->frame_delta_ns);
        AddTimingMetricSample(&time_demo->idle_stats, time_demo->interframe_idle_ns);
    } else {
        time_demo->frame_delta_ns = 0;
        time_demo->interframe_idle_ns = 0;
    }

    time_demo->last_frame_start_counter = counter;
}

void UpdateTimeDemo(AppState *state)
{
    TimeDemoState *time_demo;
    Uint32 pulse_count;

    if (!state) {
        return;
    }

    time_demo = &state->time_demo;
    time_demo->performance_frequency = SDL_GetPerformanceFrequency();

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
}

void MarkTimeDemoUpdatesComplete(AppState *state)
{
    TimeDemoState *time_demo;
    Uint64 counter;

    if (!state) {
        return;
    }

    time_demo = &state->time_demo;
    if (time_demo->frame_start_counter == 0) {
        return;
    }

    counter = SDL_GetPerformanceCounter();
    time_demo->update_complete_counter = counter;
    time_demo->update_duration_ns = CounterDeltaToNanoseconds(counter - time_demo->frame_start_counter,
                                                              time_demo->performance_frequency);
}

void MarkTimeDemoMainRenderComplete(AppState *state)
{
    TimeDemoState *time_demo;
    Uint64 counter;
    Uint64 start_counter;

    if (!state) {
        return;
    }

    time_demo = &state->time_demo;
    if (time_demo->frame_start_counter == 0) {
        return;
    }

    counter = SDL_GetPerformanceCounter();
    start_counter = time_demo->update_complete_counter ? time_demo->update_complete_counter : time_demo->frame_start_counter;
    time_demo->main_render_complete_counter = counter;
    time_demo->main_render_duration_ns = CounterDeltaToNanoseconds(counter - start_counter,
                                                                   time_demo->performance_frequency);
}

void EndTimeDemoFrame(AppState *state)
{
    TimeDemoState *time_demo;
    Uint64 counter;
    Uint64 drc_start_counter;
    Uint64 now;

    if (!state) {
        return;
    }

    time_demo = &state->time_demo;
    if (time_demo->frame_start_counter == 0) {
        return;
    }

    counter = SDL_GetPerformanceCounter();
    drc_start_counter = time_demo->main_render_complete_counter
                            ? time_demo->main_render_complete_counter
                            : (time_demo->update_complete_counter ? time_demo->update_complete_counter : time_demo->frame_start_counter);
    time_demo->drc_render_duration_ns = CounterDeltaToNanoseconds(counter - drc_start_counter,
                                                                  time_demo->performance_frequency);
    time_demo->frame_total_ns = CounterDeltaToNanoseconds(counter - time_demo->frame_start_counter,
                                                          time_demo->performance_frequency);
    time_demo->last_completed_frame_total_ns = time_demo->frame_total_ns;

    AddTimingMetricSample(&time_demo->update_stats, time_demo->update_duration_ns);
    AddTimingMetricSample(&time_demo->main_render_stats, time_demo->main_render_duration_ns);
    AddTimingMetricSample(&time_demo->drc_render_stats, time_demo->drc_render_duration_ns);
    AddTimingMetricSample(&time_demo->frame_total_stats, time_demo->frame_total_ns);
    AddTimingMetricSample(&time_demo->main_cache_stats, time_demo->main_cache_duration_ns);
    AddTimingMetricSample(&time_demo->main_scene_stats, time_demo->main_scene_duration_ns);
    AddTimingMetricSample(&time_demo->main_showcase_stats, time_demo->main_showcase_duration_ns);
    AddTimingMetricSample(&time_demo->main_camera_stats, time_demo->main_camera_duration_ns);
    AddTimingMetricSample(&time_demo->main_overlay_stats, time_demo->main_overlay_duration_ns);
    AddTimingMetricSample(&time_demo->main_cursor_stats, time_demo->main_cursor_duration_ns);
    AddTimingMetricSample(&time_demo->main_present_stats, time_demo->main_present_duration_ns);
    AddTimingMetricSample(&time_demo->drc_scene_stats, time_demo->drc_scene_duration_ns);
    AddTimingMetricSample(&time_demo->drc_present_stats, time_demo->drc_present_duration_ns);

    now = SDL_GetTicksNS();
    if (time_demo->last_instrumentation_log_ns == 0 ||
        (now - time_demo->last_instrumentation_log_ns) >= SDL_SECONDS_TO_NS(5)) {
        LogTimingWindow(time_demo);
        time_demo->last_instrumentation_log_ns = now;
    }

    time_demo->frame_start_counter = 0;
    time_demo->update_complete_counter = 0;
    time_demo->main_render_complete_counter = 0;
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
    char loop_line[96];
    char callback_line[96];
    char render_line[96];
    char timer_line[96];
    char loop_mode[32];
    char delay_line[96];
    const double fps = (time_demo->frame_delta_ns > 0)
                           ? ((double)SDL_NS_PER_SECOND / (double)time_demo->frame_delta_ns)
                           : 0.0;

    FormatDateTimeLine(utc_line, sizeof(utc_line), "utc", time_demo->utc_time, time_demo->utc_ready);
    FormatDateTimeLine(local_line, sizeof(local_line), "loc", time_demo->local_time, time_demo->local_ready);
    FormatLoopModeLabel(loop_mode, sizeof(loop_mode), *time_demo);
    SDL_snprintf(loop_line, sizeof(loop_line),
                 "fmt %s %s loop %s vs %s",
                 time_demo->locale_ready ? GetDateFormatLabel(time_demo->date_format) : "---",
                 time_demo->locale_ready ? GetTimeFormatLabel(time_demo->time_format) : "---",
                 loop_mode,
                 time_demo->renderer_vsync_known ? (time_demo->renderer_vsync ? "on" : "off") : "?");
    SDL_snprintf(callback_line, sizeof(callback_line),
                 "cb %.2f idle %.2f fps %.1f",
                 (double)time_demo->frame_delta_ns / (double)SDL_NS_PER_MS,
                 (double)time_demo->interframe_idle_ns / (double)SDL_NS_PER_MS,
                 fps);
    SDL_snprintf(render_line, sizeof(render_line),
                 "scene %.2f ovl %.2f pre %.2f",
                 (double)time_demo->main_scene_duration_ns / (double)SDL_NS_PER_MS,
                 (double)time_demo->main_overlay_duration_ns / (double)SDL_NS_PER_MS,
                 (double)time_demo->main_present_duration_ns / (double)SDL_NS_PER_MS);
    SDL_snprintf(timer_line, sizeof(timer_line),
                 "show %.2f cam %.2f drc %.2f",
                 (double)time_demo->main_showcase_duration_ns / (double)SDL_NS_PER_MS,
                 (double)time_demo->main_camera_duration_ns / (double)SDL_NS_PER_MS,
                 (double)time_demo->drc_scene_duration_ns / (double)SDL_NS_PER_MS);
    SDL_snprintf(delay_line, sizeof(delay_line),
                 "frm %.2f rt %.3f tmr %.2f",
                 (double)time_demo->frame_total_ns / (double)SDL_NS_PER_MS,
                 (double)time_demo->roundtrip_error_ns / (double)SDL_NS_PER_MS,
                 (double)(time_demo->timer_observed_period_ns ? time_demo->timer_observed_period_ns : time_demo->timer_interval_ns) / (double)SDL_NS_PER_MS);

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
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 3.0f, loop_line) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 4.0f, callback_line) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 5.0f, render_line) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 6.0f, timer_line) ||
        !SDL_RenderDebugText(state->renderer, text_x, text_y + line_height * 7.0f, delay_line)) {
        return false;
    }

    return true;
}
