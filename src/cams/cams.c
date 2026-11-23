#include <assert.h>
#include <float.h>
#include <stddef.h>

#include "cams.h"

float cams_get_nearest_camera(const struct cams_camera_info *list, size_t list_length, const struct cams_camera_info **cam, const struct gnss_position *pos, float lon_scale_squared)
{
    assert(cam);
    assert(pos);

    *cam = NULL;
    float min_dist = FLT_MAX;

    for (size_t i=0; i<list_length; i++) {
        float dist = GNSS_DISTANCE_SQUARED_IN_METERS(list[i].pos, *pos, lon_scale_squared);
        if (dist < min_dist) {
            min_dist = dist;
            *cam = &list[i];
        }
    }

    return min_dist;
}
