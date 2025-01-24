#pragma once

#include <stdbool.h>

#include <FreeRTOS.h>
#include <queue.h>

struct gnss_task_params {
    QueueHandle_t gnss_data_queue;
    QueueHandle_t audio_sample_data_queue;
};

struct gnss_position {
    float lat;
    float lon;
};

struct gnss_data {
    bool valid;
    float speed_knots;
    float course_deg;
    struct gnss_position pos;
};

void gnss_task(void *params);

#define GNSS_DISTANCE_SQUARED_IN_METERS(pos1, pos2) \
    (((pos1).lat - (pos2).lat) * ((pos1).lat - (pos2).lat) + \
    ((pos1).lon - (pos2).lon) * ((pos1).lon - (pos2).lon)) * \
    (111317.099692198f * 111317.099692198f)
