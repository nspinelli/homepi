#include "ringbuffer.h"

#include <stdlib.h>
#include <string.h>

bool ringbuffer_init(PcmRingBuffer* rb, size_t capacity_frames, size_t channels) {
  if (!rb || capacity_frames == 0 || channels == 0) {
    return false;
  }
  rb->data = (float*)calloc(capacity_frames * channels, sizeof(float));
  if (!rb->data) {
    return false;
  }
  rb->capacity_frames = capacity_frames;
  rb->channels = channels;
  rb->write_index = 0;
  rb->read_index = 0;
  rb->available_frames = 0;
  return true;
}

void ringbuffer_free(PcmRingBuffer* rb) {
  if (!rb) {
    return;
  }
  free(rb->data);
  rb->data = NULL;
}

void ringbuffer_clear(PcmRingBuffer* rb) {
  if (!rb) {
    return;
  }
  rb->write_index = 0;
  rb->read_index = 0;
  rb->available_frames = 0;
}

void ringbuffer_write(PcmRingBuffer* rb, const float* frames, size_t frame_count) {
  if (!rb || !rb->data || frame_count == 0) {
    return;
  }
  for (size_t f = 0; f < frame_count; ++f) {
    if (rb->available_frames >= rb->capacity_frames) {
      rb->read_index = (rb->read_index + 1) % rb->capacity_frames;
      rb->available_frames--;
    }
    const size_t idx = rb->write_index;
    memcpy(&rb->data[idx * rb->channels], &frames[f * rb->channels],
           rb->channels * sizeof(float));
    rb->write_index = (rb->write_index + 1) % rb->capacity_frames;
    rb->available_frames++;
  }
}

size_t ringbuffer_read(PcmRingBuffer* rb, float* out, size_t frame_count) {
  if (!rb || !rb->data || !out || frame_count == 0) {
    return 0;
  }
  size_t read = 0;
  while (read < frame_count && rb->available_frames > 0) {
    const size_t idx = rb->read_index;
    memcpy(&out[read * rb->channels], &rb->data[idx * rb->channels],
           rb->channels * sizeof(float));
    rb->read_index = (rb->read_index + 1) % rb->capacity_frames;
    rb->available_frames--;
    read++;
  }
  return read;
}
