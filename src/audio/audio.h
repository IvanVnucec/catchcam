#pragma once

#include <stdbool.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>

enum audio_samples {
    AUDIO_SAMPLES_ONE_SECOND_PAUSE,
    AUDIO_SAMPLES_ONE_BEEP,
    AUDIO_SAMPLES_THREE_BEEPS,
    AUDIO_SAMPLES_STARTUP,
    AUDIO_SAMPLES_SEARCHING_FOR_A_GPS_SIGNAL,
    AUDIO_SAMPLES_WARNING_SPEED_CAMERA_AHEAD_LIMIT,
    AUDIO_SAMPLES_UNKNOWN,
    AUDIO_SAMPLES_FIVE,
    AUDIO_SAMPLES_TEN,
    AUDIO_SAMPLES_TWENTY,
    AUDIO_SAMPLES_THIRTY,
    AUDIO_SAMPLES_FORTY,
    AUDIO_SAMPLES_FIFTY,
    AUDIO_SAMPLES_SIXTY,
    AUDIO_SAMPLES_SEVENTY,
    AUDIO_SAMPLES_EIGHTY,
    AUDIO_SAMPLES_NINETY,
    AUDIO_SAMPLES_ONE_HUNDRED,
    AUDIO_SAMPLES_GPS_SIGNAL_ACQUIRED,
    AUDIO_SAMPLES_GPS_SIGNAL_LOST,
    AUDIO_SAMPLES_LENGTH
};

struct audio_sample_data {
    enum audio_samples sample;
    SemaphoreHandle_t done_playing;
};

struct audio_task_params {
    QueueHandle_t audio_sample_data_queue;
};

void audio_play_sample_async(QueueHandle_t samples, enum audio_samples sample);
void audio_play_sample_blocking(QueueHandle_t samples, enum audio_samples sample, SemaphoreHandle_t done_playing);
void audio_task(void *params);
