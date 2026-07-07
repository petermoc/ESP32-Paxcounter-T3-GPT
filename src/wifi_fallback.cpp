#ifdef HAS_MQTT

#include "wifi_fallback.h"

#include <WiFi.h>
#include <cstdio>

#if (HAS_LORA)
#include "lorawan.h"
#endif

namespace {
struct WifiProfile {
  const char *ssid;
  const char *password;
};

const WifiProfile kWifiProfiles[] = {
    {WIFI_SSID, WIFI_PASS},
#ifdef WIFI_FALLBACK_SSID_1
    {WIFI_FALLBACK_SSID_1, WIFI_FALLBACK_PASS_1},
#endif
#ifdef WIFI_FALLBACK_SSID_2
    {WIFI_FALLBACK_SSID_2, WIFI_FALLBACK_PASS_2},
#endif
#ifdef WIFI_FALLBACK_SSID_3
    {WIFI_FALLBACK_SSID_3, WIFI_FALLBACK_PASS_3},
#endif
#ifdef WIFI_FALLBACK_SSID_4
    {WIFI_FALLBACK_SSID_4, WIFI_FALLBACK_PASS_4},
#endif
#ifdef WIFI_FALLBACK_SSID_5
    {WIFI_FALLBACK_SSID_5, WIFI_FALLBACK_PASS_5},
#endif
};

constexpr size_t kWifiProfileCount =
    sizeof(kWifiProfiles) / sizeof(kWifiProfiles[0]);
size_t s_last_successful_profile = 0;
int s_active_profile_index = -1;

bool is_profile_usable(const WifiProfile &profile) {
  return profile.ssid != nullptr && profile.ssid[0] != '\0';
}

bool is_open_network(const WifiProfile &profile) {
  return profile.password == nullptr || profile.password[0] == '\0';
}

void update_display_with_wifi_profile(const char *ssid) {
#if (HAS_LORA)
  if (ssid == nullptr || ssid[0] == '\0') {
    lora_set_status_override("WF:connected");
    return;
  }

  char compact_ssid[12] = {0};
  size_t write_idx = 0;
  for (size_t read_idx = 0; ssid[read_idx] != '\0' && write_idx < sizeof(compact_ssid) - 1;
       ++read_idx) {
    const char c = ssid[read_idx];
    if (c == ' ' || c == '\t') {
      continue;
    }
    compact_ssid[write_idx++] = c;
  }
  compact_ssid[write_idx] = '\0';

  char display_msg[LMIC_EVENTMSG_LEN] = {0};
  snprintf(display_msg, sizeof(display_msg), "WF:%s", compact_ssid[0] ? compact_ssid : "wifi");
  lora_set_status_override(display_msg);
#endif
}

void start_profile_connect(const WifiProfile &profile) {
  if (is_open_network(profile)) {
    WiFi.begin(profile.ssid);
  } else {
    WiFi.begin(profile.ssid, profile.password);
  }
}

int connect_profile(const WifiProfile &profile, const char *reason,
                    uint8_t attempts_per_profile, uint16_t delay_ms) {
  ESP_LOGI(TAG, "Trying WiFi profile '%s' for %s", profile.ssid, reason);

  WiFi.disconnect(true, true);
  delay(150);
  start_profile_connect(profile);

  for (uint8_t attempt = 0; attempt < attempts_per_profile; ++attempt) {
    if (WiFi.status() == WL_CONNECTED) {
      ESP_LOGI(TAG, "WiFi connected via '%s', IP=%s", profile.ssid,
               WiFi.localIP().toString().c_str());
      s_active_profile_index = (int)(&profile - kWifiProfiles);
      update_display_with_wifi_profile(profile.ssid);
      return 0;
    }
    delay(delay_ms);
  }

  ESP_LOGW(TAG, "WiFi connect failed for profile '%s'", profile.ssid);
  return -1;
}
} // namespace

int wifi_connect_any(const char *reason, uint8_t attempts_per_profile,
                     uint16_t delay_ms) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(clientId);

  if (WiFi.status() == WL_CONNECTED) {
    ESP_LOGI(TAG, "WiFi already connected to '%s' for %s", WiFi.SSID().c_str(),
             reason);
    return 0;
  }

  for (size_t offset = 0; offset < kWifiProfileCount; ++offset) {
    const size_t index = (s_last_successful_profile + offset) % kWifiProfileCount;
    const WifiProfile &profile = kWifiProfiles[index];
    if (!is_profile_usable(profile)) {
      continue;
    }

    if (connect_profile(profile, reason, attempts_per_profile, delay_ms) == 0) {
      s_last_successful_profile = index;
      return 0;
    }
  }

  ESP_LOGW(TAG, "All WiFi fallback profiles failed for %s", reason);
  return -1;
}

const char *wifi_get_active_profile_ssid(void) {
  static String active;
  active = WiFi.SSID();
  return active.length() > 0 ? active.c_str() : "";
}

void wifi_note_transport_failure(const char *reason) {
  const String active_ssid = WiFi.SSID();
  if (s_active_profile_index >= 0 && kWifiProfileCount > 0) {
    s_last_successful_profile =
        ((size_t)s_active_profile_index + 1) % kWifiProfileCount;
  }

  ESP_LOGW(TAG, "WiFi transport failure on '%s' for %s; rotating to next profile",
           active_ssid.length() ? active_ssid.c_str() : "(disconnected)", reason);
  s_active_profile_index = -1;
  WiFi.disconnect(true, true);
  delay(150);
}

size_t wifi_profile_count(void) { return kWifiProfileCount; }

#endif // HAS_MQTT
