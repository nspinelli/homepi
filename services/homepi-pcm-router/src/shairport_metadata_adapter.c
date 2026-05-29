#include "shairport_metadata_adapter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void shairport_topic_parse(const char* topic, ShairportTopic* out) {
  if (!out) {
    return;
  }
  out->valid = false;
  out->zone_id = 0;
  out->field[0] = '\0';

  if (!topic) {
    return;
  }

  char copy[512];
  snprintf(copy, sizeof(copy), "%s", topic);

  char* save = NULL;
  char* base = strtok_r(copy, "/", &save);
  char* type = strtok_r(NULL, "/", &save);
  char* zone = strtok_r(NULL, "/", &save);
  char* field = strtok_r(NULL, "/", &save);

  if (!base || !type || !zone || !field) {
    return;
  }
  if (strcmp(base, "shairport") != 0 || strcmp(type, "zone") != 0) {
    return;
  }

  const int zone_id = atoi(zone);
  if (zone_id < 1 || zone_id > 16) {
    return;
  }

  out->zone_id = zone_id;
  snprintf(out->field, sizeof(out->field), "%s", field);
  out->valid = true;
}
