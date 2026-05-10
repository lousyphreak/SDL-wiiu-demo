#include "time_demo.h"

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
}
