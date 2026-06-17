-- HomePi audio profile schema (homepi-usb-devices service)

CREATE TABLE IF NOT EXISTS usb_audio_capabilities (
  device_id TEXT PRIMARY KEY REFERENCES usb_devices (device_id),
  probed_at TEXT NOT NULL,
  probe_error TEXT
);

CREATE TABLE IF NOT EXISTS supported_profile_tuples (
  device_id TEXT NOT NULL REFERENCES usb_devices (device_id),
  sample_rate INTEGER NOT NULL,
  channels INTEGER NOT NULL,
  sample_format TEXT NOT NULL CHECK (sample_format IN ('S16_LE', 'S32_LE')),
  PRIMARY KEY (device_id, sample_rate, channels, sample_format)
);

CREATE TABLE IF NOT EXISTS audio_operating_profiles (
  role TEXT PRIMARY KEY CHECK (role IN ('platform_loopback', 'primary_audio')),
  device_id TEXT REFERENCES usb_devices (device_id),
  alsa_device TEXT,
  sample_rate INTEGER NOT NULL,
  channels INTEGER NOT NULL,
  sample_format TEXT NOT NULL CHECK (sample_format IN ('S16_LE', 'S32_LE')),
  profile_source TEXT NOT NULL CHECK (profile_source IN ('user_selected', 'platform_policy')),
  updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS audio_profile_state (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  profile_revision INTEGER NOT NULL DEFAULT 0,
  profile_status TEXT NOT NULL DEFAULT 'active' CHECK (profile_status IN ('active', 'paused_invalid')),
  updated_at TEXT NOT NULL
);

INSERT OR IGNORE INTO audio_profile_state (id, profile_revision, profile_status, updated_at)
VALUES (1, 0, 'active', datetime('now'));

INSERT OR IGNORE INTO audio_operating_profiles (
  role, device_id, alsa_device, sample_rate, channels, sample_format, profile_source, updated_at
) VALUES (
  'platform_loopback', NULL, NULL, 44100, 2, 'S16_LE', 'platform_policy', datetime('now')
);
