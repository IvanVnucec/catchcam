#include <float.h>
#include <stddef.h>

#include "cams.h"
#include "cams_list.h"
#include "gnss/gnss.h"

void cams_get_nearest_camera(const struct cams_camera_info **cam, const struct gnss_position *pos)
{
    assert(cam);
    assert(pos);

    float min_dist = FLT_MAX;

    for (size_t i=0; i<cams_list_length; i++) {
        float dist = GNSS_DISTANCE_SQUARED_IN_METERS(cams_list[i].pos, *pos);
        if (dist < min_dist) {
            min_dist = dist;
            *cam = &cams_list[i];
        }
    }
}
