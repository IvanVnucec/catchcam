#pragma once

#include <stdint.h>

// Map in meters: x points east, y points north.

void drive_reset(void);
void drive_print(const char *name);

void camera(float x, float y, uint8_t limit);
void start(float x, float y);
void drive_to(float x, float y, float speed_kmph);
void stop_for(int seconds);
void gps_off(void);
void gps_on(void);
void gps_noise(float meters);

int announcements(void);
float first_announcement_m(void);
int warnings(void);
int speeding_warnings(void);
int warnings_after_passing(void);
