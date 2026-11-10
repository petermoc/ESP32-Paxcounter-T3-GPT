// Basic Config
#include "senddata.h"
#include "http_fallback.h"
#include "libpax_helpers.h"
#if (HAS_LORA)
#include "lorawan.h"
#endif

namespace {
constexpr uint32_t kWifiCountSendIntervalMs = 10000;
constexpr uint32_t kLoRaCountSendIntervalMs = 60000;
uint32_t s_last_wifi_count_send_ms = 0;
uint32_t s_last_lora_count_send_ms = 0;
Ticker s_wifi_fallback_send_ticker;
bool s_wifi_fallback_send_ticker_active = false;

bool should_send_count_payload(bool use_wifi_fallback) {
  const uint32_t now_ms = millis();
  uint32_t &last_ms =
      use_wifi_fallback ? s_last_wifi_count_send_ms : s_last_lora_count_send_ms;
  const uint32_t interval_ms =
      use_wifi_fallback ? kWifiCountSendIntervalMs : kLoRaCountSendIntervalMs;

  if (last_ms != 0 && (uint32_t)(now_ms - last_ms) < interval_ms) {
    return false;
  }

  last_ms = now_ms;
  return true;
}

void stop_wifi_fallback_send_ticker() {
  if (!s_wifi_fallback_send_ticker_active) {
    return;
  }

  s_wifi_fallback_send_ticker.detach();
  s_wifi_fallback_send_ticker_active = false;
  ESP_LOGI(TAG, "Stopped dedicated WiFi fallback send ticker");
}

void ensure_wifi_fallback_send_ticker(bool use_wifi_fallback) {
  if (!use_wifi_fallback) {
    stop_wifi_fallback_send_ticker();
    return;
  }

  if (s_wifi_fallback_send_ticker_active) {
    return;
  }

  s_wifi_fallback_send_ticker.attach_ms(kWifiCountSendIntervalMs, setSendIRQ);
  s_wifi_fallback_send_ticker_active = true;
  ESP_LOGI(TAG, "Started dedicated WiFi fallback send ticker at %lu ms",
           (unsigned long)kWifiCountSendIntervalMs);
}
} // namespace

// void setSendIRQ(TimerHandle_t xTimer) {
//  xTaskNotify(irqHandlerTask, SENDCYCLE_IRQ, eSetBits);
//}

// void setSendIRQ(void) { setSendIRQ(NULL); }

void setSendIRQ(void) {
  // libpax can report its first cycle before main() has created the IRQ task.
  // Dropping that early notification is safe; notifying a null task crashes ESP32-S3.
  if (irqHandlerTask != nullptr) {
    xTaskNotify(irqHandlerTask, SENDCYCLE_IRQ, eSetBits);
  }
}

// put data to send in RTos Queues used for transmit over channels Lora and SPI
void SendPayload(uint8_t port) {
  ESP_LOGD(TAG, "sending Payload for Port %d", port);

  MessageBuffer_t SendBuffer; // contains MessageSize, MessagePort, Message[]

  SendBuffer.MessageSize = payload.getSize();

  switch (PAYLOAD_ENCODER) {
  case 1: // plain -> no mapping
  case 2: // packed -> no mapping
    SendBuffer.MessagePort = port;
    break;
  case 3: // Cayenne LPP dynamic -> all payload goes out on same port
    SendBuffer.MessagePort = CAYENNE_LPP1;
    break;
  case 4: // Cayenne LPP packed -> we need to map some paxcounter ports
    SendBuffer.MessagePort = CAYENNE_LPP2;
    switch (SendBuffer.MessagePort) {
    case COUNTERPORT:
      SendBuffer.MessagePort = CAYENNE_LPP2;
      break;
    case RCMDPORT:
      SendBuffer.MessagePort = CAYENNE_ACTUATOR;
      break;
    case TIMEPORT:
      SendBuffer.MessagePort = CAYENNE_DEVICECONFIG;
      break;
    }
    break;
  default:
    SendBuffer.MessagePort = port;
  }
  memcpy(SendBuffer.Message, payload.getBuffer(), SendBuffer.MessageSize);

// enqueue message in device's send queues
#if (HAS_LORA)
  lora_enqueuedata(&SendBuffer);
#endif
#ifdef HAS_SPI
  spi_enqueuedata(&SendBuffer);
#endif
#ifdef HAS_MQTT
  bool route_to_mqtt = true;
#if (HAS_LORA)
  route_to_mqtt = !lora_transport_healthy();
#endif
  if (route_to_mqtt) {
    mqtt_enqueuedata(&SendBuffer);
  } else {
    ESP_LOGI(TAG, "LoRaWAN healthy, skipping MQTT fallback enqueue on port %u",
             SendBuffer.MessagePort);
  }
#endif
} // SendPayload

// timer triggered function to prepare payload to send
void sendData() {
  uint8_t bitmask = cfg.payloadmask;
  uint8_t mask = 1;

#if (HAS_GPS)
  gpsStatus_t gps_status;
#endif
#if (HAS_SDS011)
  sdsStatus_t sds_status;
#endif
  struct count_payload_t count = get_effective_count_from_libpax();
  ESP_LOGD(TAG, "Sending count results: pax=%d / wifi=%d / ble=%d", count.pax,
           count.wifi_count, count.ble_count);

#ifdef HAS_MQTT
  // Prefer WiFi HTTP samples whenever a configured hotspot/network is
  // reachable so tracker maps get dense, GPS-joined pax points. LoRa stays
  // enabled as a lower-rate backup path.
  bool use_wifi_fallback = true;
#else
  bool use_wifi_fallback = false;
#endif

#if (HAS_LORA)
  if (use_wifi_fallback) {
    const uint32_t lora_age_ms = lora_last_tx_success_age_ms();
    if (lora_age_ms == UINT32_MAX) {
      ESP_LOGI(TAG, "LoRaWAN not yet healthy; WiFi HTTP path enabled");
    } else {
      ESP_LOGI(TAG,
               "LoRaWAN last success age %lu ms; WiFi HTTP path enabled in parallel",
               (unsigned long)lora_age_ms);
    }
  }
#endif

  ensure_wifi_fallback_send_ticker(use_wifi_fallback);

  while (bitmask) {
    switch (bitmask & mask) {
    case COUNT_DATA:
      payload.reset();

#if !(PAYLOAD_OPENSENSEBOX)
      payload.addCount(count.wifi_count, MAC_SNIFF_WIFI);
      if (cfg.blescan)
        payload.addCount(count.ble_count, MAC_SNIFF_BLE);
#endif

#if (HAS_GPS)
      if (GPSPORT == COUNTERPORT) {
        // send GPS position only if we have a fix
        if (gps_hasfix()) {
          if (gps_storelocation(&gps_status)) {
            payload.addGPS(gps_status);
          }
        } else
          ESP_LOGD(TAG, "No valid GPS position");
      }
#endif

#if (PAYLOAD_OPENSENSEBOX)
      payload.addCount(count.wifi_count, MAC_SNIFF_WIFI);
      if (cfg.blescan)
        payload.addCount(count.ble_count, MAC_SNIFF_BLE);
#endif

#if (HAS_SDS011)
      sds011_store(&sds_status);
      payload.addSDS(sds_status);
#endif

#ifdef HAS_DISPLAY
      dp_plotCurve(count.pax, true);
#endif

#if (HAS_SDCARD)
      sdcardWriteData(count.wifi_count, count.ble_count
#if (defined BAT_MEASURE_ADC || defined HAS_PMU)
                      ,
                      read_voltage()
#endif
      );
#endif // HAS_SDCARD

      if (!should_send_count_payload(use_wifi_fallback)) {
        if (use_wifi_fallback) {
          ESP_LOGI(
              TAG,
              "Skipping count payload; WiFi fallback cadence limit not reached yet");
        } else {
          ESP_LOGI(TAG,
                   "Skipping count payload; LoRa cadence limit not reached yet");
        }
        break;
      }

      SendPayload(COUNTERPORT);
#ifdef HAS_MQTT
      if (use_wifi_fallback) {
        ESP_LOGI(TAG,
                 "Queueing WiFi HTTP sample: pax=%d wifi=%d ble=%d",
                 count.pax, count.wifi_count, count.ble_count);
        http_fallback_send_counts(count);
      }
#endif
      break; // case COUNTDATA

#if (HAS_BME)
    case MEMS_DATA:
      payload.reset();
      payload.addBME(bme_status);
      SendPayload(BMEPORT);
      break;
#endif

#if (HAS_GPS)
    case GPS_DATA:
      if (GPSPORT != COUNTERPORT) {
        // send GPS position only if we have a fix
        if (gps_hasfix()) {
          if (gps_storelocation(&gps_status)) {
            payload.reset();
            payload.addGPS(gps_status);
            SendPayload(GPSPORT);
          }
        } else
          ESP_LOGD(TAG, "No valid GPS position");
      }
      break;
#endif

#if (HAS_SENSORS)
#if (HAS_SENSOR_1)
    case SENSOR1_DATA:
      payload.reset();
      payload.addSensor(sensor_read(1));
      SendPayload(SENSOR1PORT);
      break;
#endif
#if (HAS_SENSOR_2)
    case SENSOR2_DATA:
      payload.reset();
      payload.addSensor(sensor_read(2));
      SendPayload(SENSOR2PORT);
      break;
#endif
#if (HAS_SENSOR_3)
    case SENSOR3_DATA:
      payload.reset();
      payload.addSensor(sensor_read(3));
      SendPayload(SENSOR3PORT);
      break;
#endif
#endif

#if (defined BAT_MEASURE_ADC || defined HAS_PMU)
    case BATT_DATA:
      payload.reset();
      payload.addVoltage(read_voltage());
      SendPayload(BATTPORT);
      break;
#endif
    } // switch
    bitmask &= ~mask;
    mask <<= 1;
  } // while (bitmask)
} // sendData()

void flushQueues(void) {
  rcmd_queuereset();
#if (HAS_LORA)
  lora_queuereset();
#endif
#ifdef HAS_SPI
  spi_queuereset();
#endif
#ifdef HAS_MQTT
  mqtt_queuereset();
#endif
}

bool allQueuesEmtpy(void) {
  uint32_t rc = rcmd_queuewaiting();
#if (HAS_LORA)
  rc += lora_queuewaiting();
#endif
#ifdef HAS_SPI
  rc += spi_queuewaiting();
#endif
#ifdef HAS_MQTT
  rc += mqtt_queuewaiting();
#endif
  return (rc == 0) ? true : false;
}


