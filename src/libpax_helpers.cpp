#include "libpax_helpers.h"

// libpax payload
struct count_payload_t count_from_libpax;

typedef struct {
  bool initialized;
  uint16_t last_wifi_raw;
  uint16_t last_ble_raw;
  uint16_t last_pax_raw;
  uint32_t wifi_offset;
  uint32_t ble_offset;
  uint32_t pax_offset;
  uint32_t reset_events;
} libpax_shadow_state_t;

static libpax_shadow_state_t s_shadow = {};

static uint16_t clamp_u32_to_u16(uint32_t value) {
  return value > 0xFFFFU ? 0xFFFFU : (uint16_t)value;
}

static void log_count_payload(const char *prefix,
                              const struct count_payload_t &count) {
  ESP_LOGI(TAG, "%s pax=%u wifi=%u ble=%u", prefix, count.pax,
           count.wifi_count, count.ble_count);
}

static uint16_t compute_effective_counter(uint16_t raw_value,
                                          uint16_t &last_raw_value,
                                          uint32_t &offset,
                                          const char *label) {
  if (!s_shadow.initialized) {
    last_raw_value = raw_value;
    return raw_value;
  }

  if (cfg.countermode == 1 && raw_value < last_raw_value) {
    offset += last_raw_value;
    s_shadow.reset_events++;
    ESP_LOGW(TAG,
             "Cumulative %s counter dropped from %u to %u, carrying offset to %lu (reset_events=%lu)",
             label, last_raw_value, raw_value, (unsigned long)offset,
             (unsigned long)s_shadow.reset_events);
  }

  last_raw_value = raw_value;
  return clamp_u32_to_u16(offset + raw_value);
}

void init_libpax(void) {
  log_count_payload("init_libpax raw before init:", count_from_libpax);
  ESP_LOGI(TAG, "init_libpax config: sendcycle=%us countermode=%u blescantime=%u wifiscan=%u blescan=%u",
           cfg.sendcycle * 2, cfg.countermode, cfg.blescantime, cfg.wifiscan,
           cfg.blescan);

  libpax_counter_init(setSendIRQ, &count_from_libpax, cfg.sendcycle * 2,
                      cfg.countermode);
  libpax_counter_start();

  log_count_payload("init_libpax raw after init:", count_from_libpax);
}

void log_libpax_snapshot(const char *reason) {
  struct count_payload_t effective = get_effective_count_from_libpax();
  ESP_LOGI(
      TAG,
      "libpax snapshot (%s): raw pax=%u wifi=%u ble=%u | effective pax=%u wifi=%u ble=%u | offsets pax=%lu wifi=%lu ble=%lu | mode=%u",
      reason, count_from_libpax.pax, count_from_libpax.wifi_count,
      count_from_libpax.ble_count, effective.pax, effective.wifi_count,
      effective.ble_count, (unsigned long)s_shadow.pax_offset,
      (unsigned long)s_shadow.wifi_offset, (unsigned long)s_shadow.ble_offset,
      cfg.countermode);
}

void restart_libpax_counter(const char *reason) {
  log_libpax_snapshot(reason);
  ESP_LOGW(TAG, "Restarting libpax counter: %s", reason);
  libpax_counter_stop();
  init_libpax();
  log_libpax_snapshot(reason);
}

struct count_payload_t get_effective_count_from_libpax(void) {
  struct count_payload_t raw = count_from_libpax;
  struct count_payload_t effective = raw;

  effective.wifi_count = compute_effective_counter(
      raw.wifi_count, s_shadow.last_wifi_raw, s_shadow.wifi_offset, "wifi");
  effective.ble_count = compute_effective_counter(
      raw.ble_count, s_shadow.last_ble_raw, s_shadow.ble_offset, "ble");
  effective.pax = compute_effective_counter(raw.pax, s_shadow.last_pax_raw,
                                            s_shadow.pax_offset, "pax");

  // In cumulative mode keep pax aligned with its components when possible.
  if (cfg.countermode == 1) {
    effective.pax = clamp_u32_to_u16((uint32_t)effective.wifi_count +
                                     (uint32_t)effective.ble_count);
  }

  s_shadow.initialized = true;
  return effective;
}
