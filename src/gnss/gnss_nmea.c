#include <assert.h>
#include <math.h>
#include <stddef.h>

#include "gnss/gnss_nmea.h"
#include "minmea/minmea.h"

#define DEG_TO_RAD(deg)  ((deg) * 0.017453292519943295f)

static bool gnss_nmea_parse_sentence(struct gnss_data *gnss_data, const char *sentence)
{
    assert(gnss_data);
    assert(sentence);

    enum minmea_sentence_id sentence_id = minmea_sentence_id(sentence, true);
    switch (sentence_id) {
        case MINMEA_SENTENCE_RMC: {
            struct minmea_sentence_rmc frame;
            if (minmea_parse_rmc(&frame, sentence)) {
                // TODO: handle NaN values
                gnss_data->speed_knots = minmea_tofloat(&frame.speed);
                gnss_data->course_deg = minmea_tofloat(&frame.course);
                gnss_data->pos.lat = minmea_tocoord(&frame.latitude);
                gnss_data->pos.lon = minmea_tocoord(&frame.longitude);
                // TODO: see if valid is the right field to use
                gnss_data->valid = frame.valid && !isnan(gnss_data->pos.lat) && !isnan(gnss_data->pos.lon);
                float lon_scale = cosf(DEG_TO_RAD(gnss_data->pos.lat));
                gnss_data->lon_scale_squared = lon_scale * lon_scale;
                return true;
            }
        } break;
        default: {
        } break;
    }

    return false;
}

bool gnss_nmea_parse_char(struct gnss_data *gnss_data, char ch)
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
