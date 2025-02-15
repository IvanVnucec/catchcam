#include <float.h>
#include <stddef.h>
#include <stdlib.h>

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


void cams_get_nearest_camera_inside_radius(const struct cams_camera_info **cam, const struct gnss_position *pos, float radius)
{
    assert(cam);
    assert(pos);

    float min_dist = FLT_MAX;

    int l = 0, r = cams_list_length - 1;


    while (l <= r) {
        int m = (l + r) / 2;
        float lat_dist = cams_list[i].pos.lat - pos->lat;

        if (lat_dist > radius) {
            r = m;
        } else if (lat_dist <= radius) {
            l = m + 1;
        }
    }

    int maxi = l;

    int l = 0, r = cams_list_length - 1;

    while (l <= r) {
        int m = (l + r) / 2;
        float lat_dist = cams_list[i].pos.lat - pos->lat;

        if (lat_dist <= -radius) {
            r = m - 1;
        } else if (lat_dist > -radius) {
            l = m;
        }
    }

    int mini = r;


    float min_dist = FLT_MAX;

    for (size_t i=mini; i<=maxi; i++) {
        float dist = GNSS_DISTANCE_SQUARED_IN_METERS(cams_list[i].pos.lat, pos->lat);
        if (dist < min_dist) {
            min_dist = dist;
            *cam = &cams_list[i];
        }
    }
}