// clang-format off
// upload_speed 460800
// board lilygo-t3-s3

// for pinouts see https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series/blob/master/docs/en/t3_s3_sx1262/t3_s3_sx1262_hw.md

#ifndef _TTGOT3S3_H
#define _TTGOT3S3_H

#include <stdint.h>

// Pins for SD-card
//#define HAS_SDCARD  1      // this board has an SD-card-reader/writer
//#define SDCARD_CS    (13)
//#define SDCARD_MOSI  (11)
//#define SDCARD_MISO  (2)
//#define SDCARD_SCLK  (14)

// display SSD1306 address 0x3c
#define HAS_DISPLAY 1
#define MY_DISPLAY_SDA (18)
#define MY_DISPLAY_SCL (17)
#define FAST_BOOT_DISPLAY 1 // skip long splash delays so tracker comes up quickly
// T3-S3 OLED reset is not wired to a dedicated GPIO; GPIO14 is SD SCLK.
// Let the display driver use its default "no reset pin" path instead.
//#define MY_DISPLAY_FLIP  1 // use if display is rotated

// button and led
#define HAS_BUTTON GPIO_NUM_0       // button is on GPIO0
#define HAS_LED (37) // LED is on GPIO37
#define HAS_MQTT 1 // enable WiFi HTTP fallback transport

// LORA settings
#define HAS_LORA 1 // use on board LORA
#define CFG_sx1262_radio 1 // HPD16A
// Pins for LORA chip SPI interface, reset line and interrupt lines
#define LORA_IRQ  (33)   // RADIO_DIO1
#define LORA_IO1  LMIC_UNUSED_PIN
#define LORA_IO2  LMIC_UNUSED_PIN
#define LORA_BUSY (34)   // RADIO_BUSY
#define LORA_RST  (8)   // RADIO_NRSET
#define LORA_CS   (7)  // RADIO_CD
#define LORA_MOSI (6)  // RADIO_MOSI
#define LORA_SCK  (5)  // RADIO_SCK
#define LORA_MISO (3)  // RADIO_MISO

#endif
