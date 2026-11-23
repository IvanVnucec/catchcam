#pragma once

#include <stdbool.h>

#include "gnss/gnss_types.h"

// Returns true when a complete RMC sentence updated gnss_data.
bool gnss_nmea_parse_char(struct gnss_data *gnss_data, char ch);
