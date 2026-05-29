#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "homepi/hifi-serial/serial-port.hpp"

namespace homepi::hifi_serial {

/**
 * Serial command queue with minimum spacing between transmissions.
 */
class CommandQueue {
 public:
  /**
   * Creates a command queue.
   * @param port Serial port to write to.
   * @param interval_ms Minimum delay between commands.
   */
  CommandQueue(SerialPort& port, int interval_ms);

  ~CommandQueue();

  CommandQueue(const CommandQueue&) = delete;
  CommandQueue& operator=(const CommandQueue&) = delete;

  /**
   * Enqueues a command for transmission.
   * @param command Full command including * and CR.
   */
  void enqueue(std::string command);

  /**
   * @return Number of pending commands.
   */
  int pending_count() const;

  /** Stops the worker thread. */
  void stop();

 private:
  void worker_loop();

  SerialPort& port_;
  int interval_ms_;
  std::queue<std::string> queue_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_ = false;
  std::thread worker_;
};

}  // namespace homepi::hifi_serial
