-- HomePi HiFi serial schema (homepi-hifi-serial service)

CREATE TABLE IF NOT EXISTS hifi_controller (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  firmware_version TEXT,
  hardware_version TEXT,
  device_name TEXT,
  mac_address TEXT,
  dhcp_enabled INTEGER,
  ip_address TEXT,
  subnet_mask TEXT,
  gateway TEXT,
  tcp_port INTEGER,
  page_active INTEGER,
  serial_device_id TEXT,
  serial_path TEXT,
  last_full_sync_at TEXT,
  updated_at TEXT NOT NULL
);

INSERT OR IGNORE INTO hifi_controller (id, updated_at)
VALUES (1, datetime('now'));

CREATE TABLE IF NOT EXISTS hifi_zones (
  zone_number INTEGER PRIMARY KEY CHECK (zone_number BETWEEN 1 AND 16),
  name TEXT,
  enabled INTEGER,
  treble INTEGER,
  bass INTEGER,
  balance INTEGER,
  loudness INTEGER,
  initial_volume INTEGER,
  page_volume INTEGER,
  group_number INTEGER,
  power INTEGER,
  volume INTEGER,
  mute INTEGER,
  source INTEGER,
  updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS hifi_sources (
  source_number INTEGER PRIMARY KEY CHECK (source_number BETWEEN 1 AND 8),
  name TEXT,
  enabled INTEGER,
  input_gain INTEGER,
  display_line TEXT,
  updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS hifi_groups (
  group_number INTEGER PRIMARY KEY CHECK (group_number BETWEEN 1 AND 8),
  name TEXT,
  type INTEGER,
  updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS hifi_language_strings (
  string_number INTEGER PRIMARY KEY CHECK (string_number BETWEEN 0 AND 100),
  value TEXT,
  updated_at TEXT NOT NULL
);
