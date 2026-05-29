#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "audio_loopback.h"
#include "config.h"

int main(void) {
  HomepiConfig cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.zone_count = 16;
  snprintf(cfg.loopback_card_a, sizeof(cfg.loopback_card_a), "HomePiZonesA");
  snprintf(cfg.loopback_card_b, sizeof(cfg.loopback_card_b), "HomePiZonesB");

  char dev[128];

  assert(audio_loopback_playback_device(1, &cfg, dev, sizeof(dev)));
  assert(strcmp(dev, "hw:HomePiZonesA,0,0") == 0);
  assert(audio_loopback_capture_device(1, &cfg, dev, sizeof(dev)));
  assert(strcmp(dev, "hw:HomePiZonesA,1,0") == 0);

  assert(audio_loopback_playback_device(8, &cfg, dev, sizeof(dev)));
  assert(strcmp(dev, "hw:HomePiZonesA,0,7") == 0);

  assert(audio_loopback_playback_device(9, &cfg, dev, sizeof(dev)));
  assert(strcmp(dev, "hw:HomePiZonesB,0,0") == 0);

  assert(audio_loopback_playback_device(16, &cfg, dev, sizeof(dev)));
  assert(strcmp(dev, "hw:HomePiZonesB,0,7") == 0);

  printf("test_pcm_mapping: OK\n");
  return 0;
}
