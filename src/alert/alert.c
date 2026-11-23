#include <assert.h>
#include <float.h>
#include <math.h>

#include "alert/alert.h"

#define KMPH_TO_MPS(kmph) ((kmph) / 3.6f)

// Camera warning distance
#define CAM_WRN_MIN_DIST_M              350.0f
#define CAM_WRN_LOOKAHEAD_S             20.0f
#define CAM_WRN_HYSTERESIS_M            50.0f

void alert_init(struct alert_state *state)
{
    assert(state);

    state->last_camera = NULL;
    state->last_camera_warning_radius_m = 0.0f;
    state->last_distance_squared_in_meters = FLT_MAX;
}

struct alert_actions alert_update(struct alert_state *state, const struct gnss_data *gnss_data, const struct cams_camera_info *cams, size_t cams_length)
{
    assert(state);
    assert(gnss_data);

    struct alert_actions actions = {0};

    if (gnss_data->valid == false) {
        // Clear camera detection warning
        state->last_camera = NULL;
        state->last_distance_squared_in_meters = FLT_MAX;
        return actions;
    }

    float current_speed_kmph = KNOTS_TO_KMPH(gnss_data->speed_knots);

    // Get nearest camera
    const struct cams_camera_info *camera;
    float distance_squared_in_meters = cams_get_nearest_camera(cams, cams_length, &camera, &gnss_data->pos, gnss_data->lon_scale_squared);
    if (camera == NULL) {
        return actions;
    }

    // Keep the radius the alert fired with
    float warning_radius_m = camera == state->last_camera
        ? state->last_camera_warning_radius_m
        : fmaxf(CAM_WRN_MIN_DIST_M, KMPH_TO_MPS(current_speed_kmph) * CAM_WRN_LOOKAHEAD_S);

    actions.camera = camera;
    actions.distance_squared_in_meters = distance_squared_in_meters;
    actions.warning_radius_m = warning_radius_m;

    // Check if we are close to a camera
    if (distance_squared_in_meters <= warning_radius_m * warning_radius_m) {
        bool approaching = distance_squared_in_meters < state->last_distance_squared_in_meters;

        // Alert only if this is a new camera and we're getting closer to it
        if (state->last_camera != camera && approaching) {
            actions.announce = true;
            state->last_camera = camera;
            state->last_camera_warning_radius_m = warning_radius_m + CAM_WRN_HYSTERESIS_M;
        }

        // Trigger camera detection warnings only if we're getting closer to the camera and the speed is above certain threshold
        if (approaching && current_speed_kmph > 10.0f) {
            actions.warn = true;
            // TODO: handle different camera speed limit units
            actions.speeding = current_speed_kmph > camera->limit;
        }
    } else {
        // Clear camera detection warning
        state->last_camera = NULL;
    }

    state->last_distance_squared_in_meters = distance_squared_in_meters;
    return actions;
}
