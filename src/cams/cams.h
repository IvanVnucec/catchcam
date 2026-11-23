#pragma once

#include <stddef.h>
#include <stdint.h>
#include <pico/platform/compiler.h>

#include "gnss/gnss_types.h"

struct __packed cams_camera_info {
    struct gnss_position pos;
    uint8_t limit;
};

// Finds the camera in list closest to pos and returns its squared distance in meters.
// Sets *cam to NULL and returns FLT_MAX if the list is empty.
float cams_get_nearest_camera(const struct cams_camera_info *list, size_t list_length, const struct cams_camera_info **cam, const struct gnss_position *pos, float lon_scale_squared);
