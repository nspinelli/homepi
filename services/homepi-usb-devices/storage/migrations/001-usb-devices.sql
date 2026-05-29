-- HomePi USB devices schema (homepi-usb-devices service)

CREATE TABLE IF NOT EXISTS usb_devices (
  device_id TEXT PRIMARY KEY,
  display_name TEXT NOT NULL,
  kind TEXT NOT NULL CHECK (kind IN ('serial', 'audio')),
  id_vendor TEXT,
  id_product TEXT,
  serial TEXT,
  devpath TEXT,
  alsa_card TEXT,
  present INTEGER NOT NULL DEFAULT 0,
  updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS usb_assignments (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  serial_device_id TEXT,
  audio_primary_device_id TEXT,
  paging_device_id TEXT,
  updated_at TEXT NOT NULL,
  FOREIGN KEY (serial_device_id) REFERENCES usb_devices (device_id),
  FOREIGN KEY (audio_primary_device_id) REFERENCES usb_devices (device_id),
  FOREIGN KEY (paging_device_id) REFERENCES usb_devices (device_id)
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_usb_assign_serial
  ON usb_assignments (serial_device_id)
  WHERE serial_device_id IS NOT NULL;

CREATE UNIQUE INDEX IF NOT EXISTS idx_usb_assign_audio_primary
  ON usb_assignments (audio_primary_device_id)
  WHERE audio_primary_device_id IS NOT NULL;

CREATE UNIQUE INDEX IF NOT EXISTS idx_usb_assign_paging
  ON usb_assignments (paging_device_id)
  WHERE paging_device_id IS NOT NULL;

INSERT OR IGNORE INTO usb_assignments (id, serial_device_id, audio_primary_device_id, paging_device_id, updated_at)
VALUES (1, NULL, NULL, NULL, datetime('now'));
