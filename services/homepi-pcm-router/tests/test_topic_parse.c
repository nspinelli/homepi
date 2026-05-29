#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "shairport_metadata_adapter.h"

int main(void) {
  ShairportTopic parsed;

  shairport_topic_parse("shairport/zone/5/title", &parsed);
  assert(parsed.valid);
  assert(parsed.zone_id == 5);
  assert(strcmp(parsed.field, "title") == 0);

  shairport_topic_parse("shairport/zone/16/active_start", &parsed);
  assert(parsed.valid);
  assert(parsed.zone_id == 16);
  assert(strcmp(parsed.field, "active_start") == 0);

  shairport_topic_parse("shairport/kitchen/title", &parsed);
  assert(!parsed.valid);

  shairport_topic_parse("shairport/zone/17/title", &parsed);
  assert(!parsed.valid);

  printf("test_topic_parse: OK\n");
  return 0;
}
