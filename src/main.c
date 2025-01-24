#include <stdio.h>
#include <assert.h>
#include <float.h>
#include <math.h>

#include <pico/stdlib.h>
#include <pico/platform/compiler.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "leds/leds.h"
#include "gnss/gnss.h"
#include "cams/cams.h"
#include "audio/audio.h"

// Priorities of our threads - higher numbers are higher priority
#define MAIN_TASK_PRIORITY              (tskIDLE_PRIORITY        + 1)
#define GNSS_TASK_PRIORITY              (MAIN_TASK_PRIORITY      + 1)
#define AUDIO_TASK_PRIORITY             (GNSS_TASK_PRIORITY      + 1)
#define SYS_BLINK_TASK_PRIORITY         (AUDIO_TASK_PRIORITY     + 1)
#define CAM_DET_LED_WRN_TASK_PRIORITY   (SYS_BLINK_TASK_PRIORITY + 1)
#define CAM_DET_AUDIO_WRN_TASK_PRIORITY (CAM_DET_LED_WRN_TASK_PRIORITY + 1)

// Stack sizes of our threads in words (4 bytes)
#define MAIN_TASK_STACK_SIZE                configMINIMAL_STACK_SIZE
#define AUDIO_TASK_STACK_SIZE               configMINIMAL_STACK_SIZE
#define GNSS_TASK_STACK_SIZE                configMINIMAL_STACK_SIZE
#define SYS_BLINK_TASK_STACK_SIZE           configMINIMAL_STACK_SIZE
#define CAM_DET_LED_WRN_TASK_STACK_SIZE     configMINIMAL_STACK_SIZE
#define CAM_DET_AUDIO_WRN_TASK_STACK_SIZE   configMINIMAL_STACK_SIZE

#define KNOTS_TO_KMPH(knots) ((knots) * 1.852f)

// Camera detection LED warning data and task parameters
struct cam_det_led_wrn_data {
    size_t flash_count;
};

struct cam_det_led_wrn_task_params {
    QueueHandle_t cam_det_led_wrn_data_queue;
};

// Camera detection audio warning data and task parameters
struct cam_det_audio_wrn_data {
    enum audio_samples audio_sample;
};

struct cam_det_audio_wrn_task_params {
    QueueHandle_t cam_det_audio_wrn_data_queue;
    QueueHandle_t audio_sample_data_queue;
};

static void sys_blink_task(__unused void *params)
{
    // Test system LEDs
    leds_set_system_red_led(true);
    sleep_ms(1000);
    leds_set_system_red_led(false);

    while (true) {
        leds_set_system_green_led(true);
        sleep_ms(1000);
        leds_set_system_green_led(false);
        sleep_ms(1000);
    }
}

static void cam_det_led_wrn_task(void *params)
{
    assert(params);
    struct cam_det_led_wrn_task_params *task_params = (struct cam_det_led_wrn_task_params *)params;
    QueueHandle_t wrn_data_queue = task_params->cam_det_led_wrn_data_queue;
    assert(wrn_data_queue);

    // Test camera detected LED
    leds_set_camera_detected_led(true);
    sleep_ms(200);
    leds_set_camera_detected_led(false);

    while (true) {
        struct cam_det_led_wrn_data cam_det_led_wrn_data;
        xQueueReceive(wrn_data_queue, &cam_det_led_wrn_data, portMAX_DELAY);

        size_t flash_count = cam_det_led_wrn_data.flash_count;
        for (size_t i=0; i<flash_count; i++) {
            leds_set_camera_detected_led(true);
            sleep_ms(200);
            leds_set_camera_detected_led(false);
            sleep_ms(200);
        }

        sleep_ms(1000);
    }
}

static void cam_det_audio_wrn_task(void *params)
{
    assert(params);
    struct cam_det_audio_wrn_task_params *task_params = (struct cam_det_audio_wrn_task_params *)params;

    SemaphoreHandle_t done_playing_semaphore = xSemaphoreCreateBinary();
    assert(done_playing_semaphore);

    QueueHandle_t wrn_data_queue = task_params->cam_det_audio_wrn_data_queue;
    assert(wrn_data_queue);
    QueueHandle_t audio_sample_data_queue = task_params->audio_sample_data_queue;
    assert(audio_sample_data_queue);

    while (true) {
        // Wait for camera detection warning data
        struct cam_det_audio_wrn_data wrn_data;
        xQueueReceive(wrn_data_queue, &wrn_data, portMAX_DELAY);
        assert(wrn_data.audio_sample == AUDIO_SAMPLES_ONE_BEEP || wrn_data.audio_sample == AUDIO_SAMPLES_THREE_BEEPS);

        // Do it twice so the beep period is more periodical
        for (size_t i=0; i<2; i++) {
            // Play audio warning sample with blocking
            audio_play_sample_blocking(audio_sample_data_queue, wrn_data.audio_sample, done_playing_semaphore);
            if (wrn_data.audio_sample == AUDIO_SAMPLES_THREE_BEEPS) {
                sleep_ms(1000);
            } else {
                sleep_ms(2000);
            }
        }
    }
}

static void play_camera_detected_warning(QueueHandle_t queue, uint8_t limit) {
    static_assert(AUDIO_SAMPLES_LENGTH == 20 && "Add new audio sample handling code.");
    assert(queue);
    enum audio_samples audio_sample;

    audio_play_sample_async(queue, AUDIO_SAMPLES_WARNING_SPEED_CAMERA_AHEAD_LIMIT);

    if (limit == 0) {
        audio_play_sample_async(queue, AUDIO_SAMPLES_UNKNOWN);
        return;
    }

    // TODO: support 200 speed limit (if needed)
    int hundreds = limit / 100;
    if (hundreds == 1) {
        audio_play_sample_async(queue, AUDIO_SAMPLES_ONE_HUNDRED);
    }

    // TODO: support (100 + 15) (although it seems to be an unused speed limit)
    int tens = (limit % 100) / 10;
    switch (tens) {
        case 0:
            break;
        case 1:
            audio_play_sample_async(queue, AUDIO_SAMPLES_TEN);
            break;
        case 2:
            audio_play_sample_async(queue, AUDIO_SAMPLES_TWENTY);
            break;
        case 3:
            audio_play_sample_async(queue, AUDIO_SAMPLES_THIRTY);
            break;
        case 4:
            audio_play_sample_async(queue, AUDIO_SAMPLES_FORTY);
            break;
        case 5:
            audio_play_sample_async(queue, AUDIO_SAMPLES_FIFTY);
            break;
        case 6:
            audio_play_sample_async(queue, AUDIO_SAMPLES_SIXTY);
            break;
        case 7:
            audio_play_sample_async(queue, AUDIO_SAMPLES_SEVENTY);
            break;
        case 8:
            audio_play_sample_async(queue, AUDIO_SAMPLES_EIGHTY);
            break;
        case 9:
            audio_play_sample_async(queue, AUDIO_SAMPLES_NINETY);
            break;
        default:
            assert(0);
    }

    int ones = limit % 10;
    if (ones == 5) {
        audio_play_sample_async(queue, AUDIO_SAMPLES_FIVE);
    }
}

static void main_task(__unused void *params)
{
    BaseType_t ret;
    QueueHandle_t gnss_data_queue = xQueueCreate(1, sizeof(struct gnss_data));
    assert(gnss_data_queue);

    QueueHandle_t audio_sample_data_queue = xQueueCreate(16, sizeof(struct audio_sample_data));
    assert(audio_sample_data_queue);

    QueueHandle_t cam_det_led_wrn_data_queue = xQueueCreate(1, sizeof(struct cam_det_led_wrn_data));
    assert(cam_det_led_wrn_data_queue);

    QueueHandle_t cam_det_audio_wrn_data_queue = xQueueCreate(1, sizeof(struct cam_det_audio_wrn_data));
    assert(cam_det_audio_wrn_data_queue);

    ret = xTaskCreate(sys_blink_task, "SysBlinkThread", SYS_BLINK_TASK_STACK_SIZE, NULL, SYS_BLINK_TASK_PRIORITY, NULL);
    assert(ret == pdPASS);

    struct gnss_task_params gnss_task_params = {
        .gnss_data_queue = gnss_data_queue,
        .audio_sample_data_queue = audio_sample_data_queue,
    };
    ret = xTaskCreate(gnss_task, "GnssThread", GNSS_TASK_STACK_SIZE, &gnss_task_params, GNSS_TASK_PRIORITY, NULL);
    assert(ret == pdPASS);

    struct audio_task_params audio_task_params = {
        .audio_sample_data_queue = audio_sample_data_queue,
    };
    ret = xTaskCreate(audio_task, "AudioThread", AUDIO_TASK_STACK_SIZE, &audio_task_params, AUDIO_TASK_PRIORITY, NULL);
    assert(ret == pdPASS);

    struct cam_det_led_wrn_task_params cam_det_led_wrn_task_params = {
        .cam_det_led_wrn_data_queue = cam_det_led_wrn_data_queue,
    };
    ret = xTaskCreate(cam_det_led_wrn_task, "CamDetLedWrnThread", CAM_DET_LED_WRN_TASK_STACK_SIZE, &cam_det_led_wrn_task_params, CAM_DET_LED_WRN_TASK_PRIORITY, NULL);
    assert(ret == pdPASS);

    struct cam_det_audio_wrn_task_params cam_det_audio_wrn_task_params = {
        .cam_det_audio_wrn_data_queue = cam_det_audio_wrn_data_queue,
        .audio_sample_data_queue = audio_sample_data_queue,
    };
    ret = xTaskCreate(cam_det_audio_wrn_task, "CamDetAudioWrnThread", CAM_DET_AUDIO_WRN_TASK_STACK_SIZE, &cam_det_audio_wrn_task_params, CAM_DET_AUDIO_WRN_TASK_PRIORITY, NULL);
    assert(ret == pdPASS);

    // Play startup audio sample
    audio_play_sample_async(audio_sample_data_queue, AUDIO_SAMPLES_STARTUP);

    // Play one second pause
    audio_play_sample_async(audio_sample_data_queue, AUDIO_SAMPLES_ONE_SECOND_PAUSE);

    const struct cams_camera_info *last_camera = NULL;
    float last_distance_squared_in_meters = FLT_MAX;

    while(true) {
        // Wait for gnss data
        struct gnss_data gnss_data;
        xQueueReceive(gnss_data_queue, &gnss_data, portMAX_DELAY);

        printf("GNSS data: valid=%d, speed=%.2f kmph, course=%.2f deg, lat=%.6f, lon=%.6f\n",
               gnss_data.valid, KNOTS_TO_KMPH(gnss_data.speed_knots), gnss_data.course_deg, gnss_data.pos.lat, gnss_data.pos.lon);

        if (gnss_data.valid == false) {
            // Clear camera detection warning
            last_camera = NULL;
            last_distance_squared_in_meters = FLT_MAX;
            continue;
        }

        // Get nearest camera
        const struct cams_camera_info *camera;
        cams_get_nearest_camera(&camera, &gnss_data.pos);

        printf("Nearest camera: lat=%.6f, lon=%.6f, limit=%d\n", camera->pos.lat, camera->pos.lon, camera->limit);

        float distance_squared_in_meters = GNSS_DISTANCE_SQUARED_IN_METERS(gnss_data.pos, camera->pos);
        printf("Distance: %.2f m\n", sqrtf(distance_squared_in_meters));

        // Check if we are close to a camera
        if (distance_squared_in_meters <= 350.0f * 350.0f) {
            const uint8_t camera_limit = camera->limit;

            // Alert only if this is a new camera
            if (last_camera != camera) {
                // Flash and play camera detected warning
                struct cam_det_led_wrn_data cam_det_led_wrn_data = {
                    .flash_count = camera_limit / 10u,
                };
                xQueueOverwrite(cam_det_led_wrn_data_queue, &cam_det_led_wrn_data);

                audio_play_sample_async(audio_sample_data_queue, AUDIO_SAMPLES_THREE_BEEPS);
                play_camera_detected_warning(audio_sample_data_queue, camera_limit);

                last_camera = camera;
            }

            float current_speed_kmph = KNOTS_TO_KMPH(gnss_data.speed_knots);

            // Trigger camera detection warnings only if we're getting closer to the camera and the speed is above certain threshold
            if (distance_squared_in_meters < last_distance_squared_in_meters && current_speed_kmph > 10.0f) {
                // Play three beeps if the current speed is above the camera speed limit, otherwise play one beep
                struct cam_det_audio_wrn_data cam_det_audio_wrn_data = {
                    // TODO: handle different camera speed limit units
                    .audio_sample = current_speed_kmph > camera_limit ? AUDIO_SAMPLES_THREE_BEEPS : AUDIO_SAMPLES_ONE_BEEP,
                };
                xQueueOverwrite(cam_det_audio_wrn_data_queue, &cam_det_audio_wrn_data);

                // Flash camera detected LED based on the speed limit
                struct cam_det_led_wrn_data cam_det_led_wrn_data = {
                    .flash_count = camera_limit / 10u,
                };
                xQueueOverwrite(cam_det_led_wrn_data_queue, &cam_det_led_wrn_data);
            }
        } else {
            // Clear camera detection warning
            last_camera = NULL;
        }

        last_distance_squared_in_meters = distance_squared_in_meters;
    }
}

static void vLaunch(void)
{
    TaskHandle_t task;
    BaseType_t ret = xTaskCreate(main_task, "MainThread", MAIN_TASK_STACK_SIZE, NULL, MAIN_TASK_PRIORITY, &task);
    assert(ret == pdPASS);

    // we must bind the main task to one core (well at least while the init is called)
    vTaskCoreAffinitySet(task, 1);

    /* Start the tasks and timer running. */
    vTaskStartScheduler();
}

int main(void)
{
    leds_init();
    stdio_init_all();

    vLaunch();

    return 0;
}
