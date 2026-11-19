#include <float.h>
#include <stddef.h>

#include "cams.h"
#include "cams_list.h"
#include "gnss/gnss.h"

float cams_get_nearest_camera(const struct cams_camera_info **cam, const struct gnss_position *pos, float lon_scale_squared)
{
    assert(cam);
    assert(pos);

    *cam = NULL;
    float min_dist = FLT_MAX;

    for (size_t i=0; i<cams_list_length; i++) {
        float dist = GNSS_DISTANCE_SQUARED_IN_METERS(cams_list[i].pos, *pos, lon_scale_squared);
        if (dist < min_dist) {
            min_dist = dist;
            *cam = &cams_list[i];
        }
    }

    return min_dist;
}
