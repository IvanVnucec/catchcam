#include "audio.h"
#include "i2s_lsbj.pio.h"

#include "samples/audio_beep_sample_data.h"
#include "samples/audio_startup_sample_data.h"
#include "samples/audio_searching_for_a_gps_signal_sample_data.h"
#include "samples/audio_warning_speed_camera_ahead_limit_sample_data.h"
#include "samples/audio_unknown_sample_data.h"
#include "samples/audio_five_sample_data.h"
#include "samples/audio_ten_sample_data.h"
#include "samples/audio_twenty_sample_data.h"
#include "samples/audio_thirty_sample_data.h"
#include "samples/audio_forty_sample_data.h"
#include "samples/audio_fifty_sample_data.h"
#include "samples/audio_sixty_sample_data.h"
#include "samples/audio_seventy_sample_data.h"
#include "samples/audio_eighty_sample_data.h"
#include "samples/audio_ninety_sample_data.h"
#include "samples/audio_one_hundred_sample_data.h"
#include "samples/audio_gps_signal_sample_data.h"
#include "samples/audio_acquired_sample_data.h"
#include "samples/audio_lost_sample_data.h"

#include <stdbool.h>
#include <assert.h>
#include <stdio.h>

#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <pico/stdlib.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>

#define AUDIO_SAMPLE_RATE     44100
#define AUDIO_BITS_PER_SAMPLE 16

// Global variables for PIO and DMA management
static PIO pio = pio0;
static uint pio_sm = 0;
static int dma_chan;
static dma_channel_config dma_chan_cfg;

static SemaphoreHandle_t playback_complete_semaphore;

// DMA completion handler
static void __isr dma_handler(void) {
    BaseType_t higher_priority_task_woken = pdFALSE;

    // Clear the interrupt
    dma_channel_acknowledge_irq0(dma_chan);

    // Stop PIO state machine
    pio_sm_set_enabled(pio, pio_sm, false);

    // Signal completion
    assert(playback_complete_semaphore);
    xSemaphoreGiveFromISR(playback_complete_semaphore, &higher_priority_task_woken);

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void i2s_pio_init(void) {
    uint offset = pio_add_program(pio, &pio_lsbj_out_program);
    assert(offset >= 0);

    float system_clock = clock_get_hz(clk_sys);
    float divider = system_clock / (AUDIO_SAMPLE_RATE * 2 * AUDIO_BITS_PER_SAMPLE * 2);

    pio_lsbj_out_program_init(pio, pio_sm, offset, I2S_DATA_PIN, I2S_CLK_PIN_BASE, AUDIO_BITS_PER_SAMPLE);
    pio_sm_set_clkdiv(pio, pio_sm, divider);
}

static void i2s_dma_init(void) {
    // Initialize DMA
    dma_chan = dma_claim_unused_channel(true);
    assert(dma_chan >= 0);

    dma_chan_cfg = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_chan_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_chan_cfg, true);
    channel_config_set_write_increment(&dma_chan_cfg, false);
    channel_config_set_dreq(&dma_chan_cfg, pio_get_dreq(pio, pio_sm, true));

    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Enable DMA completion interrupt
    dma_channel_set_irq0_enabled(dma_chan, true);
}

static void audio_start_dma_transfer(const int16_t *sample, size_t sample_count) {
    assert(sample);

    dma_channel_configure(
        dma_chan,
        &dma_chan_cfg,    // Channel configuration
        &pio->txf[pio_sm],    // Write to PIO TX FIFO
        sample,           // Read from specified sample
        sample_count,     // Transfer count
        true              // Start immediately
    );

    // Start PIO state machine
    pio_sm_set_enabled(pio, pio_sm, true);
}

static void audio_set_mute(bool mute)
{
    gpio_put(AUDIO_MUTE_PIN, !mute);
}

static void audio_init(void)
{
    // Initialize audio mute pin
    gpio_init(AUDIO_MUTE_PIN);
    gpio_set_dir(AUDIO_MUTE_PIN, GPIO_OUT);
    gpio_put(AUDIO_MUTE_PIN, false);

    // Initialize I2S PIO
    i2s_pio_init();

    // Initialize DMA
    i2s_dma_init();
}

static void audio_send_sample_to_dma_and_wait(SemaphoreHandle_t semaphore, enum audio_samples sample)
{
    static_assert(AUDIO_SAMPLES_LENGTH == 20 && "Add new audio sample handling code.");
    assert(semaphore);
    assert(sample < AUDIO_SAMPLES_LENGTH);

    if (sample == AUDIO_SAMPLES_ONE_SECOND_PAUSE) {
        sleep_ms(1000);
        return;
    }

    switch (sample) {
        case AUDIO_SAMPLES_ONE_BEEP:
            audio_start_dma_transfer(audio_beep_sample_data, audio_beep_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_THREE_BEEPS:
            for (size_t i=0; i<3; i++) {
                audio_start_dma_transfer(audio_beep_sample_data, audio_beep_sample_data_length);
                xSemaphoreTake(semaphore, portMAX_DELAY);
                sleep_ms(100);
            }
            break;
        case AUDIO_SAMPLES_STARTUP:
            audio_start_dma_transfer(audio_startup_sample_data, audio_startup_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_SEARCHING_FOR_A_GPS_SIGNAL:
            audio_start_dma_transfer(audio_searching_for_a_gps_signal_sample_data, audio_searching_for_a_gps_signal_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_WARNING_SPEED_CAMERA_AHEAD_LIMIT:
            audio_start_dma_transfer(audio_warning_speed_camera_ahead_limit_sample_data, audio_warning_speed_camera_ahead_limit_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_UNKNOWN:
            audio_start_dma_transfer(audio_unknown_sample_data, audio_unknown_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_FIVE:
            audio_start_dma_transfer(audio_five_sample_data, audio_five_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_TEN:
            audio_start_dma_transfer(audio_ten_sample_data, audio_ten_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_TWENTY:
            audio_start_dma_transfer(audio_twenty_sample_data, audio_twenty_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_THIRTY:
            audio_start_dma_transfer(audio_thirty_sample_data, audio_thirty_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_FORTY:
            audio_start_dma_transfer(audio_forty_sample_data, audio_forty_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_FIFTY:
            audio_start_dma_transfer(audio_fifty_sample_data, audio_fifty_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_SIXTY:
            audio_start_dma_transfer(audio_sixty_sample_data, audio_sixty_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_SEVENTY:
            audio_start_dma_transfer(audio_seventy_sample_data, audio_seventy_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_EIGHTY:
            audio_start_dma_transfer(audio_eighty_sample_data, audio_eighty_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_NINETY:
            audio_start_dma_transfer(audio_ninety_sample_data, audio_ninety_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_ONE_HUNDRED:
            audio_start_dma_transfer(audio_one_hundred_sample_data, audio_one_hundred_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_GPS_SIGNAL_ACQUIRED:
            audio_start_dma_transfer(audio_gps_signal_sample_data, audio_gps_signal_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            audio_start_dma_transfer(audio_acquired_sample_data, audio_acquired_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        case AUDIO_SAMPLES_GPS_SIGNAL_LOST:
            audio_start_dma_transfer(audio_gps_signal_sample_data, audio_gps_signal_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            audio_start_dma_transfer(audio_lost_sample_data, audio_lost_sample_data_length);
            xSemaphoreTake(semaphore, portMAX_DELAY);
            break;
        default:
            printf("ERROR: Unknown audio sample %d\n", sample);
            assert(0);
    };
}

void audio_play_sample_async(QueueHandle_t samples, enum audio_samples sample)
{
    assert(samples);
    assert(sample < AUDIO_SAMPLES_LENGTH);

    struct audio_sample_data sample_data = {
        .sample = sample,
        .done_playing = NULL
    };
    xQueueSend(samples, &sample_data, portMAX_DELAY);
}

void audio_play_sample_blocking(QueueHandle_t samples, enum audio_samples sample, SemaphoreHandle_t done_playing)
{
    assert(samples);
    assert(sample < AUDIO_SAMPLES_LENGTH);
    assert(done_playing);

    struct audio_sample_data sample_data = {
        .sample = sample,
        .done_playing = done_playing
    };
    xQueueSend(samples, &sample_data, portMAX_DELAY);

    xSemaphoreTake(done_playing, portMAX_DELAY);
}

void audio_task(void *params)
{
    assert(params);
    struct audio_task_params *task_params = (struct audio_task_params *)params;

    playback_complete_semaphore = xSemaphoreCreateBinary();
    assert(playback_complete_semaphore);

    audio_init();

    QueueHandle_t audio_samples = task_params->audio_sample_data_queue;
    assert(audio_samples);

    while (true) {
        // Wait for a request to play a sample
        struct audio_sample_data sample_data;
        xQueueReceive(audio_samples, &sample_data, portMAX_DELAY);

        audio_set_mute(false);
        // Wait for the audio amplifier to turn on
        sleep_ms(250);
        // Play the samples until the queue is empty
        do {
            printf("Playing audio sample %d\n", sample_data.sample);

            // Play the sample and wait for completion
            audio_send_sample_to_dma_and_wait(playback_complete_semaphore, sample_data.sample);

            // Signal completion if required
            if (sample_data.done_playing != NULL) {
                xSemaphoreGive(sample_data.done_playing);
            }
        } while (xQueueReceive(audio_samples, &sample_data, 0) == pdTRUE);
        audio_set_mute(true);
    }
}
