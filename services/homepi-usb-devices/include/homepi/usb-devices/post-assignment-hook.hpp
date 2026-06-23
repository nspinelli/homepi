#pragma once

namespace homepi::usb_devices {

/**
 * Returns true when the post-assignment hook holds its runtime lock.
 * Used to defer USB hotplug rescans during ALSA reload.
 * @return True when the hook is active.
 */
bool post_assignment_hook_active();

/**
 * Starts the post-assignment hook in a detached background process.
 * @param serial_changed True when serial assignment changed.
 * @param audio_changed True when audio assignment changed.
 */
void run_post_assignment_hook_async(bool serial_changed, bool audio_changed);

}  // namespace homepi::usb_devices
