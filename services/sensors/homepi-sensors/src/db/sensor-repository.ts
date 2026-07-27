import { mkdirSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

import Database from "better-sqlite3";

import {
  CONTACT_SENSOR_CONTROLLERS,
  CONTACT_SENSOR_SEEDS,
  sensorIdFromNumber,
} from "../config/sensor-map.js";
import type {
  ContactSensorPatch,
  ContactSensorRecord,
  ContactState,
  SensorType,
} from "../types/contact-sensor-types.js";

const __dirname = dirname(fileURLToPath(import.meta.url));

/**
 * Opens the shared HomePi SQLite database and runs migrations.
 * @param dbPath - Database file path.
 * @returns Open database handle.
 */
export function openSensorDatabase(dbPath: string): Database.Database {
  mkdirSync(dirname(dbPath), { recursive: true });
  const db = new Database(dbPath);
  db.pragma("journal_mode = WAL");
  runMigrations(db);
  seedIfEmpty(db);
  return db;
}

/**
 * Applies SQL migrations from the storage/migrations folder.
 * @param db - Database handle.
 */
function runMigrations(db: Database.Database): void {
  db.exec(`
    CREATE TABLE IF NOT EXISTS schema_migrations (
      name TEXT PRIMARY KEY,
      applied_at TEXT NOT NULL
    );
  `);

  const migrationsDir = join(__dirname, "../../storage/migrations");
  const files = readdirSync(migrationsDir)
    .filter((name) => name.endsWith(".sql"))
    .sort();

  for (const file of files) {
    const applied = db
      .prepare("SELECT 1 FROM schema_migrations WHERE name = ?")
      .get(file);
    if (applied) {
      continue;
    }
    const sql = readFileSync(join(migrationsDir, file), "utf8");
    db.exec(sql);
    db.prepare("INSERT INTO schema_migrations (name, applied_at) VALUES (?, ?)").run(
      file,
      new Date().toISOString()
    );
  }
}

/**
 * Seeds controllers and sensors when tables are empty.
 * @param db - Database handle.
 */
function seedIfEmpty(db: Database.Database): void {
  const count = db.prepare("SELECT COUNT(*) AS c FROM contact_sensors").get() as {
    c: number;
  };
  if (count.c > 0) {
    return;
  }

  const now = new Date().toISOString();
  const insertController = db.prepare(`
    INSERT INTO contact_sensor_controllers (
      controller_id, controller_type, controller_model,
      i2c_bus, i2c_address,
      inta_net_name, inta_pi_physical_pin, inta_bcm_gpio,
      intb_net_name, intb_pi_physical_pin, intb_bcm_gpio,
      created_at, updated_at
    ) VALUES (
      @controllerId, @controllerType, @controllerModel,
      @i2cBus, @i2cAddress,
      @intaNetName, @intaPiPhysicalPin, @intaBcmGpio,
      @intbNetName, @intbPiPhysicalPin, @intbBcmGpio,
      @now, @now
    )
  `);

  for (const controller of CONTACT_SENSOR_CONTROLLERS) {
    insertController.run({ ...controller, now });
  }

  const insertSensor = db.prepare(`
    INSERT INTO contact_sensors (
      sensor_id, sensor_number, sensor_name, sensor_type,
      controller_id, hardware_type, hardware_model, schematic_net_name,
      i2c_address, mcp_bank, mcp_pin,
      pi_physical_pin, bcm_gpio,
      invert_logic, debounce_ms, homekit_enabled,
      contact_state, faulted, tamper_supported,
      created_at, updated_at
    ) VALUES (
      @sensorId, @sensorNumber, @sensorName, @sensorType,
      @controllerId, @hardwareType, @hardwareModel, @schematicNetName,
      @i2cAddress, @mcpBank, @mcpPin,
      @piPhysicalPin, @bcmGpio,
      0, 50, 0,
      'unknown', 0, 0,
      @now, @now
    )
  `);

  for (const seed of CONTACT_SENSOR_SEEDS) {
    insertSensor.run({
      sensorId: sensorIdFromNumber(seed.sensorNumber),
      sensorNumber: seed.sensorNumber,
      sensorName: `Sensor ${seed.sensorNumber}`,
      sensorType: "other",
      controllerId: seed.controllerId,
      hardwareType: seed.hardwareType,
      hardwareModel: seed.hardwareModel,
      schematicNetName: seed.schematicNetName,
      i2cAddress: seed.i2cAddress ?? null,
      mcpBank: seed.mcpBank ?? null,
      mcpPin: seed.mcpPin ?? null,
      piPhysicalPin: seed.piPhysicalPin ?? null,
      bcmGpio: seed.bcmGpio ?? null,
      now,
    });
  }
}

/**
 * Repository for contact sensor persistence.
 */
export class SensorRepository {
  /**
   * Creates a sensor repository.
   * @param db - SQLite database handle.
   */
  constructor(private readonly db: Database.Database) {}

  /**
   * Returns all contact sensors ordered by sensor number.
   * @returns Sensor records.
   */
  listSensors(): ContactSensorRecord[] {
    const rows = this.db
      .prepare("SELECT * FROM contact_sensors ORDER BY sensor_number ASC")
      .all() as Array<Record<string, unknown>>;
    return rows.map(rowToRecord);
  }

  /**
   * Loads one sensor by id.
   * @param sensorId - Sensor id.
   * @returns Sensor record or null.
   */
  getSensor(sensorId: string): ContactSensorRecord | null {
    const row = this.db
      .prepare("SELECT * FROM contact_sensors WHERE sensor_id = ?")
      .get(sensorId) as Record<string, unknown> | undefined;
    return row ? rowToRecord(row) : null;
  }

  /**
   * Updates sensor runtime state after a hardware read.
   * @param sensorId - Sensor id.
   * @param state - Normalized contact state.
   * @param rawValue - Raw GPIO/MCP bit.
   * @param faulted - Fault flag.
   * @param faultReason - Optional fault reason.
   */
  updateSensorState(
    sensorId: string,
    state: ContactState,
    rawValue: number,
    faulted: boolean,
    faultReason: string | null
  ): void {
    const existing = this.getSensor(sensorId);
    if (!existing) {
      return;
    }
    const now = new Date().toISOString();
    const stateChanged = existing.contactState !== state;
    this.db
      .prepare(
        `UPDATE contact_sensors SET
          contact_state = ?,
          raw_value = ?,
          faulted = ?,
          fault_reason = ?,
          last_seen_at = ?,
          last_changed_at = CASE WHEN ? THEN ? ELSE last_changed_at END,
          updated_at = ?
        WHERE sensor_id = ?`
      )
      .run(
        state,
        rawValue,
        faulted ? 1 : 0,
        faultReason,
        now,
        stateChanged ? 1 : 0,
        now,
        now,
        sensorId
      );
  }

  /**
   * Marks all sensors on a controller as faulted.
   * @param controllerId - Controller id.
   * @param faultReason - Fault reason text.
   */
  markControllerFaulted(controllerId: string, faultReason: string): void {
    const now = new Date().toISOString();
    this.db
      .prepare(
        `UPDATE contact_sensors SET
          faulted = 1,
          fault_reason = ?,
          contact_state = 'unknown',
          updated_at = ?
        WHERE controller_id = ?`
      )
      .run(faultReason, now, controllerId);
  }

  /**
   * Applies a configuration patch to one sensor.
   * @param sensorId - Sensor id.
   * @param patch - Patch fields.
   * @returns Updated record or null.
   */
  patchSensor(sensorId: string, patch: ContactSensorPatch): ContactSensorRecord | null {
    const existing = this.getSensor(sensorId);
    if (!existing) {
      return null;
    }

    const now = new Date().toISOString();
    const sensorName = patch.sensorName ?? existing.sensorName;
    const sensorType = patch.sensorType ?? existing.sensorType;
    const roomId =
      patch.roomId !== undefined ? patch.roomId : existing.roomId;
    const roomName =
      patch.roomName !== undefined ? patch.roomName : existing.roomName;
    const homekitEnabled =
      patch.homekitEnabled !== undefined
        ? patch.homekitEnabled
        : existing.homekitEnabled;

    this.db
      .prepare(
        `UPDATE contact_sensors SET
          sensor_name = ?,
          sensor_type = ?,
          room_id = ?,
          room_name = ?,
          homekit_enabled = ?,
          updated_at = ?
        WHERE sensor_id = ?`
      )
      .run(
        sensorName,
        sensorType,
        roomId,
        roomName,
        homekitEnabled ? 1 : 0,
        now,
        sensorId
      );

    return this.getSensor(sensorId);
  }
}

/**
 * Maps a database row to a contact sensor record.
 * @param row - SQLite row object.
 * @returns Contact sensor record.
 */
function rowToRecord(row: Record<string, unknown>): ContactSensorRecord {
  return {
    sensorId: String(row.sensor_id),
    sensorNumber: Number(row.sensor_number),
    sensorName: String(row.sensor_name),
    sensorType: String(row.sensor_type) as SensorType,
    roomId: row.room_id ? String(row.room_id) : null,
    roomName: row.room_name ? String(row.room_name) : null,
    controllerId: String(row.controller_id),
    hardwareType: row.hardware_type as ContactSensorRecord["hardwareType"],
    hardwareModel: String(row.hardware_model),
    schematicNetName: row.schematic_net_name ? String(row.schematic_net_name) : null,
    i2cAddress: row.i2c_address ? String(row.i2c_address) : null,
    mcpBank: row.mcp_bank ? String(row.mcp_bank) : null,
    mcpPin: row.mcp_pin !== null && row.mcp_pin !== undefined ? Number(row.mcp_pin) : null,
    piPhysicalPin:
      row.pi_physical_pin !== null && row.pi_physical_pin !== undefined
        ? Number(row.pi_physical_pin)
        : null,
    bcmGpio:
      row.bcm_gpio !== null && row.bcm_gpio !== undefined ? Number(row.bcm_gpio) : null,
    invertLogic: Boolean(row.invert_logic),
    debounceMs: Number(row.debounce_ms),
    homekitEnabled: Boolean(row.homekit_enabled),
    homekitAccessoryUuid: row.homekit_accessory_uuid
      ? String(row.homekit_accessory_uuid)
      : null,
    contactState: String(row.contact_state) as ContactState,
    rawValue:
      row.raw_value !== null && row.raw_value !== undefined ? Number(row.raw_value) : null,
    faulted: Boolean(row.faulted),
    faultReason: row.fault_reason ? String(row.fault_reason) : null,
    tamperSupported: Boolean(row.tamper_supported),
    tampered: Boolean(row.tampered),
    tamperReason: row.tamper_reason ? String(row.tamper_reason) : null,
    lastChangedAt: row.last_changed_at ? String(row.last_changed_at) : null,
    lastSeenAt: row.last_seen_at ? String(row.last_seen_at) : null,
  };
}

/**
 * Serializes a sensor record for API responses.
 * @param record - Sensor record.
 * @returns JSON-friendly object.
 */
export function serializeSensorRecord(record: ContactSensorRecord): Record<string, unknown> {
  return {
    id: record.sensorId,
    sensorNumber: record.sensorNumber,
    name: record.sensorName,
    type: record.sensorType,
    roomId: record.roomId,
    roomName: record.roomName,
    controllerId: record.controllerId,
    hardwareType: record.hardwareType,
    hardwareModel: record.hardwareModel,
    contactState: record.contactState,
    open: record.contactState === "open",
    faulted: record.faulted,
    faultReason: record.faultReason,
    homekitEnabled: record.homekitEnabled,
    lastChangedAt: record.lastChangedAt,
    lastSeenAt: record.lastSeenAt,
  };
}
