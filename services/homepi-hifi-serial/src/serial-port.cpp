#include "homepi/hifi-serial/serial-port.hpp"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>

namespace homepi::hifi_serial {

SerialPort::~SerialPort() { close(); }

bool SerialPort::open(const std::string& path, int baud_rate) {
  close();
  fd_ = ::open(path.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
  if (fd_ < 0) {
    return false;
  }

  termios tty{};
  if (tcgetattr(fd_, &tty) != 0) {
    close();
    return false;
  }

  cfmakeraw(&tty);
  tty.c_cflag |= (CLOCAL | CREAD);
  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;

  speed_t speed = B9600;
  if (baud_rate == 9600) {
    speed = B9600;
  }
  cfsetispeed(&tty, speed);
  cfsetospeed(&tty, speed);

  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 1;

  if (tcsetattr(fd_, TCSANOW, &tty) != 0) {
    close();
    return false;
  }

  reader_running_.store(true);
  reader_thread_ = std::thread([this]() { reader_loop(); });
  return true;
}

void SerialPort::close() {
  reader_running_.store(false);
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  if (reader_thread_.joinable()) {
    reader_thread_.detach();
  }
}

bool SerialPort::is_open() const { return fd_ >= 0; }

bool SerialPort::write_bytes(const std::string& data) {
  if (fd_ < 0) {
    return false;
  }
  std::size_t written = 0;
  while (written < data.size()) {
    const ssize_t n = ::write(fd_, data.data() + written, data.size() - written);
    if (n <= 0) {
      return false;
    }
    written += static_cast<std::size_t>(n);
  }
  return true;
}

void SerialPort::set_line_callback(LineCallback callback) { line_callback_ = std::move(callback); }

void SerialPort::reader_loop() {
  std::string buffer;
  char chunk[256];
  while (reader_running_.load()) {
    if (fd_ < 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    const ssize_t n = ::read(fd_, chunk, sizeof(chunk));
    if (n <= 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      continue;
    }
    buffer.append(chunk, static_cast<std::size_t>(n));
    std::size_t pos = 0;
    while ((pos = buffer.find('\n')) != std::string::npos) {
      std::string line = buffer.substr(0, pos);
      buffer.erase(0, pos + 1);
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      if (!line.empty() && line_callback_) {
        line_callback_(line);
      }
    }
  }
}

}  // namespace homepi::hifi_serial
