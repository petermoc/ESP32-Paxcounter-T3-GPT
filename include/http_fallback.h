#ifndef _HTTP_FALLBACK_H
#define _HTTP_FALLBACK_H

#ifdef HAS_MQTT

#include "globals.h"
#include "libpax_helpers.h"

void http_fallback_send_counts(const count_payload_t &count);

#endif

#endif // _HTTP_FALLBACK_H
