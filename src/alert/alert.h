#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "cams/cams.h"
#include "gnss/gnss_types.h"

struct alert_state {
    const struct cams_camera_info *last_camera;
    float last_camera_warning_radius_m;
    float last_distance_squared_in_meters;
};

struct alert_actions {
    const struct cams_camera_info *camera;
    float distance_squared_in_meters;
    float warning_radius_m;
    bool announce;
    bool warn;
    bool speeding;
};

void alert_init(struct alert_state *state);
struct alert_actions alert_update(struct alert_state *state, const struct gnss_data *gnss_data, const struct cams_camera_info *cams, size_t cams_length);
