# HomePi USB Devices

A service to manage the rules for the usb devices connected to the Raspberry Pi.

## Purpose
HomePi needs a way to assign different USB devices to different services. This service is in charge of interacting with the UI to store the user settings based on what is selected. 

### Core Functionality
- Utilize the `core` features of the HomePi application
- Automatically detect USB devices that are connected to the Raspberry Pi
- USB information stored in database using `core/storgae` to ensure the user cannot select the same USB device for different services.
- Provides rules to create virtual ports and socks based on the service.
- Creates UDEV rules so that if the plug is not detected the system is notified immediately
- Used to set the Serial connection for the hifi-controller
- Used to set the primary audio dac for the audio system
- Used to set the primary audio dac for the paging functionality 
- Need to find a unique identifier for each USB device that is not the port. 

- If the plug is removed and plugged back in it should automatically detect and make the system aware using the features in `core/config`

### UI
- This should be a part of the status page to ensure that this service is running and functioning correctly.
- A new section in the main Settings tab for "Audio Configuration"
- Three modern drop downs with these names
    - Primary Serial Connection
    - Primary Audio Output
    - Primary Paging Output
- There should be a save button within the Audio Configuration box.
- When clicked it should save the information into the database for other services to use.

---

## Implementation

Native C++ daemon (`homepi-usb-devices`) with:

- **Stable `deviceId`**: `usb:{vendor}:{product}:{serial}` or `usb:fallback:{hash}` when no serial
- **SQLite** at `/opt/homepi/runtime/state/homepi.sqlite` (see `storage/migrations/`)
- **Unix socket** `/run/homepi/usb-devices.sock` — proxied by `apps/backend`
- **Generated artifacts** under `/opt/homepi/runtime/generated/`:
  - udev: `SYMLINK+="vHifi"` for serial assignment
  - ALSA: pcm aliases `AudioOut` and `AudioPaging` (`plug:AudioOut`, `plug:AudioPaging`)
- **Hotplug**: libudev monitor → rescan → SSE status via backend health poll

Install and manual validation: [`services-homepi-usb-devices-install-readme.md`](services-homepi-usb-devices-install-readme.md)

Consumer examples: [`examples/`](examples/)