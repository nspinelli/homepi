#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Fixed-size SPSC ring buffer for interleaved PCM frames. */
typedef struct PcmRingBuffer {
  float* data;
  size_t capacity_frames;
  size_t channels;
  size_t write_index;
  size_t read_index;
  size_t available_frames;
  pthread_mutex_t mutex;
} PcmRingBuffer;

/**
 * Allocates a ring buffer.
 * @param rb Ring buffer.
 * @param capacity_frames Max frames stored.
 * @param channels Channel count.
 * @return True on success.
 */
bool ringbuffer_init(PcmRingBuffer* rb, size_t capacity_frames, size_t channels);

/** Releases ring buffer memory. */
void ringbuffer_free(PcmRingBuffer* rb);

/**
 * Writes frames into the buffer, dropping oldest on overflow.
 * @param rb Ring buffer.
 * @param frames Interleaved samples.
 * @param frame_count Number of frames.
 */
void ringbuffer_write(PcmRingBuffer* rb, const float* frames, size_t frame_count);

/**
 * Reads up to frame_count frames.
 * @param rb Ring buffer.
 * @param out Output buffer.
 * @param frame_count Max frames to read.
 * @return Frames actually read.
 */
size_t ringbuffer_read(PcmRingBuffer* rb, float* out, size_t frame_count);

/** Clears all buffered frames. */
void ringbuffer_clear(PcmRingBuffer* rb);

/**
 * Keeps only the newest frames, dropping older buffered audio.
 * @param rb Ring buffer.
 * @param keep_frames Number of newest frames to retain.
 */
void ringbuffer_sync_to_latest(PcmRingBuffer* rb, size_t keep_frames);

/**
 * Returns the number of frames currently available to read.
 * @param rb Ring buffer.
 * @return Available frame count.
 */
size_t ringbuffer_available(const PcmRingBuffer* rb);
