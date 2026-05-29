# Audio

- **PCM routing:** `homepi-pcm-router` — 16 loopback zones, MQTT owner stack, routes active zone to Primary Audio Output (`hw:HomePiPrimaryAudio,0` from SQLite).
- **USB DAC assignment:** `homepi-usb-devices` settings UI — Primary Audio Output, paging, serial.
- **AirPlay (future):** `homepi-shairport` — 16 Shairport instances → loopback playback + MQTT.
