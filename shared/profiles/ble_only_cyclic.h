#pragma once

// Profile for older T3 / T-Beam footfall nodes:
// - show/send cyclic counters instead of cumulative totals
// - disable WiFi sniffing so BLE dominates the signal

#ifdef COUNTERMODE
#undef COUNTERMODE
#endif
#define COUNTERMODE 0

#ifdef WIFICOUNTER
#undef WIFICOUNTER
#endif
#define WIFICOUNTER 0

#ifdef SENDCYCLE
#undef SENDCYCLE
#endif
#define SENDCYCLE 3
