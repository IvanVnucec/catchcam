#include <stdbool.h>
#include <assert.h>
#include <stdio.h>

#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <hardware/uart.h>

#include "gnss/gnss.h"
#include "leds/leds.h"
#include "audio/audio.h"

#include <FreeRTOS.h>
#include <stream_buffer.h>
#include <queue.h>

#include "minmea/minmea.h"

#define BAUD_RATE        9600
#define DATA_BITS        8
#define STOP_BITS        1
#define PARITY           UART_PARITY_NONE

static StreamBufferHandle_t gnss_nmea_stream_buff;

static void __isr gnss_on_uart_nmea_rx(void)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    assert(gnss_nmea_stream_buff);
    while (uart_is_readable(UART_NMEA_PMTK_ID)) {
        char ch = uart_getc(UART_NMEA_PMTK_ID);
        xStreamBufferSendFromISR(gnss_nmea_stream_buff, &ch, 1, &higher_priority_task_woken);
    }

    // Perform context switch if needed
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void uart_hw_init(void)
{
    uart_init(UART_NMEA_PMTK_ID, BAUD_RATE);
    // Set up the UART RX pin
    gpio_set_function(UART_NMEA_PMTK_RX_PIN, UART_FUNCSEL_NUM(UART_NMEA_PMTK_ID, UART_NMEA_PMTK_RX_PIN));
    // Disable HW CTS/RTS flow control
    uart_set_hw_flow(UART_NMEA_PMTK_ID, false, false);
    uart_set_format(UART_NMEA_PMTK_ID, DATA_BITS, STOP_BITS, PARITY);
    // Don't use the FIFOs
    uart_set_fifo_enabled(UART_NMEA_PMTK_ID, false);

    // And set up and enable the interrupt handlers
    int uart_irq = UART_NMEA_PMTK_ID == uart0 ? UART0_IRQ : UART1_IRQ;
    irq_set_exclusive_handler(uart_irq, gnss_on_uart_nmea_rx);
    irq_set_enabled(uart_irq, true);

    // Now enable the UART to send interrupts - RX only
    uart_set_irq_enables(UART_NMEA_PMTK_ID, true, false);
}

static bool gnss_nmea_parse_sentence(struct gnss_data *gnss_data, const char *sentence)
{
    assert(gnss_data);
    assert(sentence);

    enum minmea_sentence_id sentence_id = minmea_sentence_id(sentence, true);
    switch (sentence_id) {
        case MINMEA_SENTENCE_RMC: {
            struct minmea_sentence_rmc frame;
            if (minmea_parse_rmc(&frame, sentence)) {
                // TODO: see if valid is the right field to use
                gnss_data->valid = frame.valid;
                // TODO: handle NaN values
                gnss_data->speed_knots = minmea_tofloat(&frame.speed);
                gnss_data->course_deg = minmea_tofloat(&frame.course);
                gnss_data->pos.lat = minmea_tocoord(&frame.latitude);
                gnss_data->pos.lon = minmea_tocoord(&frame.longitude);
                return true;
            }
        } break;
        default: {
        } break;
    }

    return false;
}

static bool gnss_nmea_parse_char(struct gnss_data *gnss_data, char ch)
{
    assert(gnss_data);

    bool ret;
    static char nmea_buff[1024];
    static size_t nmea_buff_idx = 0;
    static bool nmea_start_found = false;

    ret = false;
    if (nmea_start_found == false) {
        // Skip any characters until we find the start of a NMEA sentence
        if (ch == '$') {
            nmea_buff[nmea_buff_idx++] = ch;
            nmea_start_found = true;
        }
    } else {
        // Copy characters until we find the end of the NMEA sentence
        if (ch == '\r' || ch == '\n') {
            nmea_buff[nmea_buff_idx] = '\0';
            ret = gnss_nmea_parse_sentence(gnss_data, nmea_buff);

            // Reset buffer
            nmea_buff_idx = 0;
            nmea_start_found = false;
        } else {
            nmea_buff[nmea_buff_idx++] = ch;
        }
    }

    // Clear buffer if it's full
    if (nmea_buff_idx >= sizeof(nmea_buff)) {
        nmea_buff_idx = 0;
        nmea_start_found = false;
    }

    return ret;
}

void gnss_task(void *params)
{
    assert(params);
    struct gnss_task_params *task_params = (struct gnss_task_params *)params;
    struct gnss_data gnss_data = {0};
    bool gnss_fix_acquired_played = false;

    // Test GNSS LEDs
    leds_set_gnss_fix_leds_state(true);
    sleep_ms(1000);
    leds_set_gnss_fix_leds_state(false);

    gnss_nmea_stream_buff = xStreamBufferCreate(1024, 1);
    assert(gnss_nmea_stream_buff);

    uart_hw_init();

    QueueHandle_t gnss_data_queue = task_params->gnss_data_queue;
    assert(gnss_data_queue);
    QueueHandle_t audio_sample_data_queue = task_params->audio_sample_data_queue;
    assert(audio_sample_data_queue);

    audio_play_sample_async(audio_sample_data_queue, AUDIO_SAMPLES_SEARCHING_FOR_A_GPS_SIGNAL);

    while (true) {
        // Wait for gnss data
        // TODO: See if we need to set a timeout here
        char ch;
        xStreamBufferReceive(gnss_nmea_stream_buff, &ch, sizeof(ch), portMAX_DELAY);

        // Parse NMEA sentence and send data to main task if available
        bool new_data_available = gnss_nmea_parse_char(&gnss_data, ch);
        if (new_data_available) {
            xQueueOverwrite(task_params->gnss_data_queue, &gnss_data);

            if (gnss_data.valid) {
                leds_set_gnss_fix_leds_state(true);

                // Play GNSS fix acquired only when fix was previously lost
                if (gnss_fix_acquired_played == false) {
                    audio_play_sample_async(audio_sample_data_queue, AUDIO_SAMPLES_GPS_SIGNAL_ACQUIRED);
                    gnss_fix_acquired_played = true;
                }
            } else {
                leds_set_gnss_fix_leds_state(false);

                if (gnss_fix_acquired_played) {
                    audio_play_sample_async(audio_sample_data_queue, AUDIO_SAMPLES_GPS_SIGNAL_LOST);
                    gnss_fix_acquired_played = false;
                }
            }
        }
    }
}
