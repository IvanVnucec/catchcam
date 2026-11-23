#pragma once

#include <stdbool.h>

struct gnss_position {
    float lat;
    float lon;
};

struct gnss_data {
    bool valid;
    float speed_knots;
    float course_deg;
    struct gnss_position pos;
    float lon_scale_squared;
};

#define KNOTS_TO_KMPH(knots) ((knots) * 1.852f)

// Equirectangular approximation, accurate for the short distances we care about.
#define GNSS_DISTANCE_SQUARED_IN_METERS(pos1, pos2, lon_scale_squared) \
    ((((pos1).lat - (pos2).lat) * ((pos1).lat - (pos2).lat) + \
    ((pos1).lon - (pos2).lon) * ((pos1).lon - (pos2).lon) * (lon_scale_squared)) * \
    (111317.099692198f * 111317.099692198f))
