#pragma once

// Some older T-Beam OLED modules acknowledge at 0x3C but fail the installed
// controller driver. Do not initialize that controller until it is identified.
#ifdef HAS_DISPLAY
#undef HAS_DISPLAY
#endif
