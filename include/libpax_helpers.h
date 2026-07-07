#ifndef _LIBPAX_HELPERS_H
#define _LIBPAX_HELPERS_H

#include <stdio.h>
#include <libpax_api.h>
#include "senddata.h"
#include "configmanager.h"

void init_libpax(void);
void restart_libpax_counter(const char *reason);
void log_libpax_snapshot(const char *reason);
struct count_payload_t get_effective_count_from_libpax(void);

extern struct count_payload_t count_from_libpax; // libpax count storage

#endif
