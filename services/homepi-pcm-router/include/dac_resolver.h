#pragma once

#include <stdbool.h>

#include "config.h"

/** Resolved primary DAC assignment from SQLite. */
typedef struct DacAssignment {
  char device_id[256];
  char display_name[256];
  char dac_device[128];
  bool present;
} DacAssignment;

/**
 * Reads primary audio assignment from homepi.sqlite.
 * @param cfg Service configuration.
 * @param out Resolved assignment.
 * @return True when a valid present primary audio device is assigned.
 */
bool dac_resolver_load_primary(const HomepiConfig* cfg, DacAssignment* out);
