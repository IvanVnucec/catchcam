#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "drive.h"
#include "alert/alert.h"
#include "gnss/gnss_nmea.h"

#define BASE_LAT 45.0
#define BASE_LON 15.0
#define M_PER_DEG 111317.099692198
#define PI 3.14159265358979323846
#define DEG_TO_RAD(deg) ((deg) * PI / 180.0)

#define MAX_CAMERAS 16
#define MAX_FIXES 7200

struct fix {
    double x;
    double y;
    double speed_kmph;
    bool gps;
    struct alert_actions alert;
};

static struct cams_camera_info cameras[MAX_CAMERAS];
static size_t camera_count;
static struct fix fixes[MAX_FIXES];
static size_t fix_count;

static struct alert_state alert_state;
static struct gnss_data gnss_data;

static double pos_x, pos_y, course_deg;
static bool gps_enabled;
static double noise_m;
static unsigned int rng;

static double to_lat(double y) { return BASE_LAT + y / M_PER_DEG; }
static double to_lon(double x) { return BASE_LON + x / (M_PER_DEG * cos(DEG_TO_RAD(BASE_LAT))); }

static double noise(void)
{
    rng = rng * 1103515245u + 12345u;
    return (((rng >> 16) & 0x7fff) / 32767.0 * 2.0 - 1.0) * noise_m;
}

static int format_coord(char *buf, size_t size, double deg, int deg_digits, char pos, char neg)
{
    char hemisphere = deg < 0 ? neg : pos;
    deg = fabs(deg);
    int whole = (int)deg;
    return snprintf(buf, size, "%0*d%07.4f,%c", deg_digits, whole, (deg - whole) * 60.0, hemisphere);
}

static void emit(double speed_kmph)
{
    assert(fix_count < MAX_FIXES);

    size_t t = fix_count;
    char body[160];
    int n = snprintf(body, sizeof(body), "GPRMC,%02zu%02zu%02zu.00,", t / 3600 % 24, t / 60 % 60, t % 60);

    if (gps_enabled) {
        char lat[32], lon[32];
        format_coord(lat, sizeof(lat), to_lat(pos_y + noise()), 2, 'N', 'S');
        format_coord(lon, sizeof(lon), to_lon(pos_x + noise()), 3, 'E', 'W');
        snprintf(body + n, sizeof(body) - n, "A,%s,%s,%.2f,%.2f,231126,,,A", lat, lon, speed_kmph / 1.852, course_deg);
    } else {
        snprintf(body + n, sizeof(body) - n, "V,,,,,,,231126,,,N");
    }

    unsigned char checksum = 0;
    for (const char *c = body; *c; c++) {
        checksum ^= (unsigned char)*c;
    }

    char sentence[200];
    snprintf(sentence, sizeof(sentence), "$%s*%02X\r\n", body, checksum);

    struct alert_actions alert = {0};
    for (const char *c = sentence; *c; c++) {
        if (gnss_nmea_parse_char(&gnss_data, *c)) {
            alert = alert_update(&alert_state, &gnss_data, cameras, camera_count);
        }
    }

    fixes[fix_count++] = (struct fix){
        .x = pos_x,
        .y = pos_y,
        .speed_kmph = speed_kmph,
        .gps = gps_enabled,
        .alert = alert,
    };
}

void drive_reset(void)
{
    camera_count = 0;
    fix_count = 0;
    alert_init(&alert_state);
    gnss_nmea_parse_char(&gnss_data, '\n');
    memset(&gnss_data, 0, sizeof(gnss_data));
    pos_x = 0;
    pos_y = 0;
    course_deg = 0;
    gps_enabled = true;
    noise_m = 0;
    rng = 1;
}

void camera(float x, float y, uint8_t limit)
{
    assert(camera_count < MAX_CAMERAS);
    cameras[camera_count++] = (struct cams_camera_info){
        .pos = { .lat = (float)to_lat(y), .lon = (float)to_lon(x) },
        .limit = limit,
    };
}

void start(float x, float y)
{
    pos_x = x;
    pos_y = y;
}

void drive_to(float x, float y, float speed_kmph)
{
    assert(speed_kmph > 0);

    double from_x = pos_x, from_y = pos_y;
    double dx = x - from_x, dy = y - from_y;
    double length = hypot(dx, dy);
    double step = speed_kmph / 3.6;

    course_deg = fmod(atan2(dx, dy) * 180.0 / PI + 360.0, 360.0);

    for (double travelled = 0; travelled < length; ) {
        travelled = fmin(travelled + step, length);
        pos_x = from_x + dx * travelled / length;
        pos_y = from_y + dy * travelled / length;
        emit(speed_kmph);
    }
}

void stop_for(int seconds)
{
    for (int i = 0; i < seconds; i++) {
        emit(0);
    }
}

void gps_off(void) { gps_enabled = false; }
void gps_on(void) { gps_enabled = true; }
void gps_noise(float meters) { noise_m = meters; }

static float distance_m(const struct fix *f)
{
    return f->alert.camera ? sqrtf(f->alert.distance_squared_in_meters) : -1.0f;
}

int announcements(void)
{
    int count = 0;
    for (size_t i = 0; i < fix_count; i++) {
        count += fixes[i].alert.announce;
    }
    return count;
}

float first_announcement_m(void)
{
    for (size_t i = 0; i < fix_count; i++) {
        if (fixes[i].alert.announce) {
            return distance_m(&fixes[i]);
        }
    }
    return -1.0f;
}

int warnings(void)
{
    int count = 0;
    for (size_t i = 0; i < fix_count; i++) {
        count += fixes[i].alert.warn;
    }
    return count;
}

int speeding_warnings(void)
{
    int count = 0;
    for (size_t i = 0; i < fix_count; i++) {
        count += fixes[i].alert.warn && fixes[i].alert.speeding;
    }
    return count;
}

int warnings_after_passing(void)
{
    size_t closest = 0;
    float closest_m = INFINITY;
    for (size_t i = 0; i < fix_count; i++) {
        float d = distance_m(&fixes[i]);
        if (d >= 0 && d < closest_m) {
            closest_m = d;
            closest = i;
        }
    }

    int count = 0;
    for (size_t i = closest + 1; i < fix_count; i++) {
        count += fixes[i].alert.warn;
    }
    return count;
}

void drive_print(const char *name)
{
    printf("\n%s\n", name);
    printf("     t       x       y   km/h    dist  radius\n");
    for (size_t i = 0; i < fix_count; i++) {
        const struct fix *f = &fixes[i];
        printf("%5zus %7.0f %7.0f %6.0f", i + 1, f->x, f->y, f->speed_kmph);
        if (!f->gps) {
            printf("   no gps\n");
            continue;
        }
        if (f->alert.camera) {
            printf(" %7.0f %7.0f", distance_m(f), f->alert.warning_radius_m);
        }
        if (f->alert.announce) {
            printf("  ANNOUNCE limit %u", f->alert.camera->limit);
        }
        if (f->alert.warn) {
            printf("  %s", f->alert.speeding ? "BEEP x3" : "BEEP");
        }
        printf("\n");
    }
}
