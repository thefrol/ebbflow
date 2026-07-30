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
