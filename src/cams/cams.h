#pragma once

#include <stdint.h>
#include <pico/platform/compiler.h>

#include "gnss/gnss.h"

struct __packed cams_camera_info {
    struct gnss_position pos;
    uint8_t limit;
};

// Finds the camera closest to pos and returns its squared distance in meters.
// Sets *cam to NULL and returns FLT_MAX if there are no cameras.
float cams_get_nearest_camera(const struct cams_camera_info **cam, const struct gnss_position *pos, float lon_scale_squared);
