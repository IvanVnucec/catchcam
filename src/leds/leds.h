#pragma once

#include <stdbool.h>

void leds_init(void);
void leds_set_system_red_led(bool on);
void leds_set_system_green_led(bool on);
void leds_set_camera_detected_led(bool on);
void leds_set_gnss_fix_leds_state(bool fix);
