#include "homepi/hifi-serial/command-queue.hpp"

#include <thread>

namespace homepi::hifi_serial {

CommandQueue::CommandQueue(SerialPort& port, int interval_ms)
    : port_(port), interval_ms_(interval_ms), worker_([this]() { worker_loop(); }) {}

CommandQueue::~CommandQueue() { stop(); }

void CommandQueue::enqueue(std::string command) {
  {
    std::lock_guard lock(mutex_);
    queue_.push(std::move(command));
  }
  cv_.notify_one();
}

int CommandQueue::pending_count() const {
  std::lock_guard lock(mutex_);
  return static_cast<int>(queue_.size());
}

void CommandQueue::stop() {
  {
    std::lock_guard lock(mutex_);
    stop_ = true;
    while (!queue_.empty()) {
      queue_.pop();
    }
  }
  cv_.notify_all();
  if (worker_.joinable()) {
    worker_.join();
  }
}

void CommandQueue::worker_loop() {
  while (true) {
    std::string command;
    {
      std::unique_lock lock(mutex_);
      cv_.wait(lock, [this]() { return stop_ || !queue_.empty(); });
      if (stop_) {
        return;
      }
      command = std::move(queue_.front());
      queue_.pop();
    }
    port_.write_bytes(command);
    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms_));
  }
}

}  // namespace homepi::hifi_serial
