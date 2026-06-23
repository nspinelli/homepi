#include "homepi/usb-devices/post-assignment-hook.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstdlib>
#include <sstream>

namespace homepi::usb_devices {

namespace {

constexpr const char* kPostAssignmentHook =
    "/opt/homepi/services/usb-devices/scripts/post-assignment-hook.sh";
constexpr const char* kPostAssignmentHookLock = "/run/homepi/post-assignment-hook.lock";

}  // namespace

bool post_assignment_hook_active() {
  const int fd = ::open(kPostAssignmentHookLock, O_RDONLY);
  if (fd < 0) {
    return false;
  }

  struct flock lock {};
  lock.l_type = F_WRLCK;
  lock.l_whence = SEEK_SET;
  const bool active = (::fcntl(fd, F_GETLK, &lock) == 0 && lock.l_type != F_UNLCK);
  ::close(fd);
  return active;
}

void run_post_assignment_hook_async(bool serial_changed, bool audio_changed) {
  if (!serial_changed && !audio_changed) {
    return;
  }

  const pid_t pid = ::fork();
  if (pid < 0) {
    return;
  }
  if (pid > 0) {
    return;
  }

  if (::setsid() < 0) {
    _exit(1);
  }

  const pid_t worker = ::fork();
  if (worker < 0) {
    _exit(1);
  }
  if (worker > 0) {
    _exit(0);
  }

  std::ostringstream command;
  command << "sudo -n " << kPostAssignmentHook << ' ' << (serial_changed ? "1" : "0") << ' '
          << (audio_changed ? "1" : "0")
          << " >>/opt/homepi/runtime/cache/post-assignment-hook.log 2>&1";
  std::system(command.str().c_str());
  _exit(0);
}

}  // namespace homepi::usb_devices
