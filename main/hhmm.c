#include "hhmm.h"

#include <stdio.h>

bool hhmm_to_min(const char *s, int *out_min)
{
    int h = 0, m = 0;
    if (sscanf(s, "%d:%d", &h, &m) == 2 && h >= 0 && h < 24 && m >= 0 && m < 60) {
        *out_min = h * 60 + m;
        return true;
    }
    return false;
}

void hhmm_min_to_str(int min, char *buf, size_t len)
{
    snprintf(buf, len, "%02d:%02d", min / 60, min % 60);
}
