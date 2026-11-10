#pragma once

#include "globals.h"

struct wifi_transport_status_t {
  int active_profile_index;
  size_t profile_count;
  uint32_t last_success_age_seconds;
  uint32_t last_failure_age_seconds;
  uint32_t consecutive_failures;
  uint32_t transport_sequence;
  const char *active_ssid;
  const char *last_ack_id;
};

int wifi_connect_any(const char *reason, uint8_t attempts_per_profile = 10,
                     uint16_t delay_ms = 500);
const char *wifi_get_active_profile_ssid(void);
void wifi_note_transport_failure(const char *reason);
void wifi_note_transport_success(const char *ack_id);
wifi_transport_status_t wifi_get_transport_status(void);
size_t wifi_profile_count(void);
