#pragma once

#include <functional>
#include <string>

namespace homepi::hifi_serial {

/**
 * RS-232 serial port at 9600 8N1 for Hi-Fi2.
 */
class SerialPort {
 public:
  using LineCallback = std::function<void(const std::string& line)>;

  SerialPort() = default;
  ~SerialPort();

  SerialPort(const SerialPort&) = delete;
  SerialPort& operator=(const SerialPort&) = delete;

  /**
   * Opens the serial device.
   * @param path Device path.
   * @param baud_rate Baud rate (default 9600).
   * @return True on success.
   */
  bool open(const std::string& path, int baud_rate = 9600);

  /** Closes the port and reader thread. */
  void close();

  /**
   * @return True when file descriptor is open.
   */
  bool is_open() const;

  /**
   * Writes raw bytes to the port.
   * @param data Bytes to write.
   * @return True on success.
   */
  bool write_bytes(const std::string& data);

  /**
   * Sets callback for complete response lines (without CR/LF).
   * @param callback Invoked from reader thread.
   */
  void set_line_callback(LineCallback callback);

 private:
  void reader_loop();

  int fd_ = -1;
  LineCallback line_callback_;
};

}  // namespace homepi::hifi_serial
