#ifdef HAS_MQTT

#include "http_fallback.h"

#include "gpsread.h"
#include "power.h"
#include "reset.h"
#include "rcommand.h"
#include "wifi_fallback.h"

#include <mbedtls/base64.h>

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

static int http_fallback_connect_wifi(void) {
  return wifi_connect_any("HTTP fallback", 10, 500);
}

static void append_json_number(String &json, const char *key, int32_t value,
                               bool &needs_comma) {
  if (needs_comma)
    json += ",";
  json += "\"";
  json += key;
  json += "\":";
  json += String(value);
  needs_comma = true;
}

static void append_json_float(String &json, const char *key, double value,
                              uint8_t decimals, bool &needs_comma) {
  if (needs_comma)
    json += ",";
  json += "\"";
  json += key;
  json += "\":";
  json += String((double)value, (unsigned int)decimals);
  needs_comma = true;
}

static void append_json_string(String &json, const char *key, const String &value,
                               bool &needs_comma) {
  if (needs_comma)
    json += ",";
  json += "\"";
  json += key;
  json += "\":\"";
  json += value;
  json += "\"";
  needs_comma = true;
}

static bool extract_json_string_field(const String &json, const char *key, String &value) {
  const String needle = String("\"") + key + "\":\"";
  const int start = json.indexOf(needle);
  if (start < 0) {
    return false;
  }

  const int value_start = start + needle.length();
  int value_end = value_start;
  while (value_end < json.length()) {
    if (json[value_end] == '"' && json[value_end - 1] != '\\') {
      value = json.substring(value_start, value_end);
      return true;
    }
    value_end++;
  }

  return false;
}

static void process_http_command_response(const String &response_body) {
  if (response_body.length() == 0) {
    return;
  }

  String command_base64;
  if (!extract_json_string_field(response_body, "commandBase64", command_base64) ||
      command_base64.length() == 0) {
    return;
  }

  size_t out_len = 0;
  const unsigned char *payload = (const unsigned char *)command_base64.c_str();
  if (mbedtls_base64_decode(NULL, 0, &out_len, payload, command_base64.length()) != 0 || out_len == 0) {
    ESP_LOGW(TAG, "HTTP fallback command decode sizing failed");
    return;
  }

  unsigned char decoded[out_len];
  if (mbedtls_base64_decode(decoded, out_len, &out_len, payload, command_base64.length()) != 0) {
    ESP_LOGW(TAG, "HTTP fallback command base64 decode failed");
    return;
  }

  ESP_LOGI(TAG, "HTTP fallback received remote command (%u byte(s))", out_len);
  rcommand(decoded, out_len);
}

static bool pause_scanners_for_http(libpax_config_t &saved_config) {
  if (!cfg.blescan && !cfg.wifiscan) {
    return false;
  }

  log_libpax_snapshot("http fallback pause_scanners_for_http before stop");
  libpax_counter_stop();
  libpax_get_current_config(&saved_config);

  libpax_config_t paused_config = saved_config;
  paused_config.blecounter = 0;
  paused_config.wificounter = 0;

  const int config_update = libpax_update_config(&paused_config);
  if (config_update != 0) {
    ESP_LOGW(TAG, "HTTP fallback could not pause scanners (%d)", config_update);
    restart_libpax_counter("http fallback pause_scanners_for_http config restore after failure");
    return false;
  }

  libpax_counter_start();
  delay(150);
  ESP_LOGI(TAG, "HTTP fallback paused scanners, free heap=%u",
           (unsigned int)ESP.getFreeHeap());
  return true;
}

static void resume_scanners_after_http(libpax_config_t saved_config,
                                       bool scanners_paused) {
  if (!scanners_paused) {
    return;
  }

  log_libpax_snapshot("http fallback resume_scanners_after_http before stop");
  libpax_counter_stop();

  const int config_update = libpax_update_config(&saved_config);
  if (config_update != 0) {
    ESP_LOGW(TAG, "HTTP fallback could not restore scanners (%d)",
             config_update);
    restart_libpax_counter("http fallback resume_scanners_after_http restore failure");
    return;
  }

  // Restart scanning with the restored config without re-initializing libpax,
  // so cumulative counters keep running across WiFi fallback posts.
  libpax_counter_start();
  delay(100);
  log_libpax_snapshot("http fallback resume_scanners_after_http after start");
  ESP_LOGI(TAG, "HTTP fallback restored scanners, free heap=%u",
           (unsigned int)ESP.getFreeHeap());
}

static QueueHandle_t httpFallbackQueue = nullptr;
static TaskHandle_t httpFallbackTaskHandle = nullptr;

static void http_fallback_send_counts_impl(const count_payload_t &count) {
  const size_t max_profiles = wifi_profile_count() > 0 ? wifi_profile_count() : 1;

  for (size_t attempt = 0; attempt < max_profiles; ++attempt) {
    if (http_fallback_connect_wifi() != 0) {
      return;
    }

#if (HAS_GPS)
    gpsStatus_t gps_status;
    // WiFi fallback should forward the most recent valid GPS location even if
    // the stricter live-fix gate (HDOP/altitude freshness) is not currently met.
    const bool has_gps = gps_storelocation(&gps_status);
#endif
    const uint16_t voltage_mv = read_voltage();
    const uint64_t uptime_seconds = uptime() / 1000ULL;
    const String device_uid = WiFi.macAddress();

    String json;
    json.reserve(320);
    json += "{";
    bool needs_comma = false;

    append_json_string(json, "deviceId", String(clientId), needs_comma);
    append_json_string(json, "deviceUid", device_uid, needs_comma);
    append_json_string(json, "wifiSsid", WiFi.SSID(), needs_comma);
    append_json_number(json, "wifi", count.wifi_count, needs_comma);
    append_json_number(json, "ble", count.ble_count, needs_comma);
    append_json_number(json, "pax", count.pax, needs_comma);
    append_json_number(json, "batteryMv", voltage_mv, needs_comma);
    append_json_number(json, "uptime", (int32_t)uptime_seconds, needs_comma);

    if (batt_level >= 0) {
      append_json_number(json, "batteryPercent", batt_level, needs_comma);
    }

#if (HAS_GPS)
    if (has_gps) {
      append_json_float(json, "latitude", gps_status.latitude / 1000000.0, 6,
                        needs_comma);
      append_json_float(json, "longitude", gps_status.longitude / 1000000.0, 6,
                        needs_comma);
      append_json_number(json, "satellites", gps_status.satellites, needs_comma);
      append_json_float(json, "hdop", gps_status.hdop / 100.0, 2, needs_comma);
      append_json_number(json, "altitude", gps_status.altitude, needs_comma);
    }
#endif

    json += "}";

    HTTPClient http;
    const String fallback_url = String(HTTP_FALLBACK_URL);
    const bool use_tls = fallback_url.startsWith("https://");
    libpax_config_t saved_config;
    const bool scanners_paused = use_tls && pause_scanners_for_http(saved_config);
    bool begin_ok = false;

    WiFiClient plain_client;
    WiFiClientSecure secure_client;
    if (use_tls) {
      secure_client.setInsecure();
      secure_client.setHandshakeTimeout(5);
      begin_ok = http.begin(secure_client, HTTP_FALLBACK_URL);
    } else {
      begin_ok = http.begin(plain_client, HTTP_FALLBACK_URL);
    }

    if (!begin_ok) {
      ESP_LOGW(TAG, "HTTP fallback begin failed");
      resume_scanners_after_http(saved_config, scanners_paused);
      wifi_note_transport_failure("HTTP fallback begin");
      continue;
    }

    http.setConnectTimeout(5000);
    http.setTimeout(5000);
    http.addHeader("Content-Type", "application/json");

    if (strlen(HTTP_FALLBACK_APIKEY) > 0) {
      http.addHeader("X-Api-Key", HTTP_FALLBACK_APIKEY);
    }

    ESP_LOGI(TAG, "HTTP fallback POST to %s via '%s' (heap=%u)",
             HTTP_FALLBACK_URL, WiFi.SSID().c_str(), (unsigned int)ESP.getFreeHeap());
    const int status = http.POST((uint8_t *)json.c_str(), json.length());
    if (status > 0 && status < 300) {
      const String response_body = http.getString();
      ESP_LOGI(TAG, "HTTP fallback sent count payload (%d)", status);
      process_http_command_response(response_body);
      http.end();
      resume_scanners_after_http(saved_config, scanners_paused);
      return;
    }

    ESP_LOGW(TAG, "HTTP fallback POST failed (%d) on '%s'", status,
             WiFi.SSID().c_str());
    http.end();
    resume_scanners_after_http(saved_config, scanners_paused);
    wifi_note_transport_failure("HTTP fallback POST");
  }
}

static void http_fallback_task(void *parameter) {
  count_payload_t count;

  for (;;) {
    if (xQueueReceive(httpFallbackQueue, &count, portMAX_DELAY) == pdTRUE) {
      http_fallback_send_counts_impl(count);
    }
  }
}

static void ensure_http_fallback_task() {
  if (httpFallbackQueue == nullptr) {
    httpFallbackQueue = xQueueCreate(2, sizeof(count_payload_t));
  }

  if (httpFallbackTaskHandle == nullptr && httpFallbackQueue != nullptr) {
    xTaskCreatePinnedToCore(http_fallback_task, "http_fallback", 12288, NULL,
                            1, &httpFallbackTaskHandle, 1);
  }
}

void http_fallback_send_counts(const count_payload_t &count) {
  ensure_http_fallback_task();

  if (httpFallbackQueue == nullptr) {
    ESP_LOGW(TAG, "HTTP fallback queue not available");
    return;
  }

  if (xQueueSend(httpFallbackQueue, &count, 0) != pdTRUE) {
    ESP_LOGW(TAG, "HTTP fallback queue full, dropping sample");
  }
}

#endif // HAS_MQTT
