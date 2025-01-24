#pragma once

#include <stdint.h>
#include <pico/platform/compiler.h>

#include "gnss/gnss.h"

struct __packed cams_camera_info {
    struct gnss_position pos;
    uint8_t limit;
};

void cams_get_nearest_camera(const struct cams_camera_info **cam, const struct gnss_position *pos);
