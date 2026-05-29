#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "zone_state.h"

int main(void) {
  HomepiConfig cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.zone_count = 16;

  ZoneState state;
  zone_state_init(&state, &cfg);

  zone_state_on_active_start(&state, 1);
  assert(zone_state_get_owner(&state) == 1);

  zone_state_on_active_start(&state, 12);
  assert(zone_state_get_owner(&state) == 12);

  zone_state_on_active_start(&state, 16);
  assert(zone_state_get_owner(&state) == 16);

  zone_state_on_active_end(&state, 16);
  assert(zone_state_get_owner(&state) == 12);

  zone_state_on_active_end(&state, 12);
  assert(zone_state_get_owner(&state) == 1);

  const bool cleared = zone_state_on_active_end(&state, 1);
  assert(cleared);
  assert(zone_state_get_owner(&state) == 0);

  zone_state_destroy(&state);
  printf("test_active_stack: OK\n");
  return 0;
}
