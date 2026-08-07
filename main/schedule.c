#include "schedule.h"

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
