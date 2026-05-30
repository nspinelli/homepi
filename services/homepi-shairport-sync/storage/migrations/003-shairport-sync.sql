-- HomePi Shairport Sync schema (homepi-shairport-sync service)

ALTER TABLE hifi_sources ADD COLUMN is_airplay INTEGER NOT NULL DEFAULT 0;

CREATE UNIQUE INDEX IF NOT EXISTS idx_hifi_sources_one_airplay
  ON hifi_sources(is_airplay) WHERE is_airplay = 1;

UPDATE hifi_sources SET is_airplay = 1 WHERE source_number = 5;

CREATE TABLE IF NOT EXISTS shairport_zone_settings (
  zone_number INTEGER PRIMARY KEY CHECK (zone_number BETWEEN 1 AND 16),
  volume_control_profile TEXT NOT NULL DEFAULT 'standard',
  active_state_timeout REAL NOT NULL DEFAULT 5.0,
  session_timeout INTEGER NOT NULL DEFAULT 60,
  log_verbosity INTEGER NOT NULL DEFAULT 1,
  updated_at TEXT NOT NULL
);

INSERT OR IGNORE INTO shairport_zone_settings (zone_number, updated_at)
SELECT value, datetime('now') FROM (
  SELECT 1 AS value UNION SELECT 2 UNION SELECT 3 UNION SELECT 4 UNION
  SELECT 5 UNION SELECT 6 UNION SELECT 7 UNION SELECT 8 UNION
  SELECT 9 UNION SELECT 10 UNION SELECT 11 UNION SELECT 12 UNION
  SELECT 13 UNION SELECT 14 UNION SELECT 15 UNION SELECT 16
);
