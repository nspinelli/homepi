#pragma once

#include <stddef.h>

/** Event publish callback. */
typedef void (*JsonEventSink)(const char* line, void* user);

/**
 * Registers global event sink for envelope emission.
 * @param sink Callback.
 * @param user User pointer.
 */
void json_events_set_sink(JsonEventSink sink, void* user);

/**
 * Publishes a core event envelope.
 * @param topic Event topic.
 * @param event Event name.
 * @param correlation_id Correlation id.
 * @param payload_json JSON object body (without outer braces optional).
 */
void json_events_emit(const char* topic, const char* event, const char* correlation_id,
                      const char* payload_json);

/**
 * Builds JSON snapshot of router state.
 * @param owner_zone_id Current owner (0 = none).
 * @param stack Zone id stack.
 * @param stack_len Stack length.
 * @param dac_state DAC state string.
 * @param out Output buffer.
 * @param out_len Buffer size.
 */
void json_events_build_snapshot(int owner_zone_id, const int* stack, size_t stack_len,
                                const char* dac_state, char* out, size_t out_len);
