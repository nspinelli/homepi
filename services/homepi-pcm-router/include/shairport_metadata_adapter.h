#pragma once

#include <stdbool.h>

/** Parsed Shairport MQTT topic. */
typedef struct ShairportTopic {
  int zone_id;
  char field[64];
  bool valid;
} ShairportTopic;

/**
 * Parses shairport/zone/N/field topics.
 * @param topic MQTT topic.
 * @param out Parsed result.
 */
void shairport_topic_parse(const char* topic, ShairportTopic* out);
