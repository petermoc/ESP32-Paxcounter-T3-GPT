#pragma once

// TB-f4 acknowledges at the common 0x3C I2C address but does not accept the
// SSD1306 initialization sequence used by the other T-Beam. Its OLED uses the
// compatible SH1106 command set instead.
#ifdef OLED_TYPE
#undef OLED_TYPE
#endif
#define OLED_TYPE OLED_SH1106_3C

// This board revision has no safe discrete status LED pin. Avoid starting the
// LED task while diagnosing the OLED, which also prevents its restart loop.
#ifdef HAS_LED
#undef HAS_LED
#endif
#define HAS_LED NOT_A_PIN
