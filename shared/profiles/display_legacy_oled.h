#pragma once

// Older T3 / T-Beam OLEDs behave best with explicit SSD1306 settings and a
// calmer contrast level. Keep this isolated to legacy boards so newer T3-S3
// nodes stay on their own defaults.

#ifdef MY_DISPLAY_ADDR
#undef MY_DISPLAY_ADDR
#endif
#define MY_DISPLAY_ADDR 0x3C

#ifdef OLED_TYPE
#undef OLED_TYPE
#endif
#define OLED_TYPE OLED_SSD1306_3C

#ifdef OLED_FREQUENCY
#undef OLED_FREQUENCY
#endif
#define OLED_FREQUENCY 100000L

#ifdef DISPLAYCONTRAST
#undef DISPLAYCONTRAST
#endif
#define DISPLAYCONTRAST 32
