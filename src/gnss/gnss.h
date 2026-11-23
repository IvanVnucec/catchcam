#pragma once

#include <FreeRTOS.h>
#include <queue.h>

#include "gnss/gnss_types.h"

struct gnss_task_params {
    QueueHandle_t gnss_data_queue;
    QueueHandle_t audio_sample_data_queue;
};

void gnss_task(void *params);
