#pragma once

#include "globals.h"

int wifi_connect_any(const char *reason, uint8_t attempts_per_profile = 10,
                     uint16_t delay_ms = 500);
const char *wifi_get_active_profile_ssid(void);
void wifi_note_transport_failure(const char *reason);
size_t wifi_profile_count(void);
