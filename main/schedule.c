#include "schedule.h"

#include <stddef.h>

bool lamp_schedule_active(int on_min, int off_min, int now_min)
{
    if (on_min == off_min) {
        return false; // вырожденное расписание — свет выключен
    }
    if (on_min < off_min) {
        return now_min >= on_min && now_min < off_min;
    }
    // расписание идёт через полночь
    return now_min >= on_min || now_min < off_min;
}

bool lamp_pulse_active(int interval_min, int duration_sec, int now_sec)
{
    if (interval_min <= 0 || duration_sec <= 0) {
        return false;
    }
    int interval_sec = interval_min * 60;
    if (duration_sec >= interval_sec) {
        return false; // вырождено — не даём вечно включённый пин
    }
    return (now_sec % interval_sec) < duration_sec;
}

int lamp_pulse_next_start(int interval_min, int duration_sec, int now_sec)
{
    if (interval_min <= 0 || duration_sec <= 0) {
        return -1;
    }
    int interval_sec = interval_min * 60;
    if (duration_sec >= interval_sec) {
        return -1;
    }
    int offset = now_sec % interval_sec;
    return now_sec - offset + interval_sec;
}

// Округление вниз для целочисленного деления секунд на сутки.
static int day_floor(int sec)
{
    if (sec >= 0) {
        return sec / 86400;
    }
    return -((-sec + 86399) / 86400);
}

int lamp_pulse_nearest_starts(int interval_min, int duration_sec, int now_sec,
                              int past_count, int future_count,
                              int *out_starts, int *out_day_offsets, int max_count)
{
    if (interval_min <= 0 || duration_sec <= 0 ||
        now_sec < 0 || now_sec >= 24 * 3600 ||
        past_count < 0 || future_count < 0 ||
        out_starts == NULL || out_day_offsets == NULL ||
        max_count <= 0 || past_count + future_count > max_count) {
        return 0;
    }

    int interval_sec = interval_min * 60;
    if (duration_sec >= interval_sec) {
        return 0;
    }

    int current_start = now_sec - (now_sec % interval_sec);
    bool in_pulse = (now_sec - current_start) < duration_sec;
    // Последний импульс, который уже гарантированно закончился.
    int latest_finished = in_pulse ? (current_start - interval_sec) : current_start;

    int written = 0;

    for (int i = 0; i < past_count; i++) {
        int start = latest_finished - i * interval_sec;
        int day = day_floor(start);
        out_starts[written] = start - day * 86400;
        out_day_offsets[written] = day;
        written++;
    }

    int next_start = lamp_pulse_next_start(interval_min, duration_sec, now_sec);
    for (int i = 0; i < future_count; i++) {
        int start = next_start + i * interval_sec;
        int day = day_floor(start);
        out_starts[written] = start - day * 86400;
        out_day_offsets[written] = day;
        written++;
    }

    return written;
}
