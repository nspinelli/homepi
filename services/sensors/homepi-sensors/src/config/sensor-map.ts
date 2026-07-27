import type { ControllerSeed, SensorSeed } from "../types/contact-sensor-types.js";

/** MCP23017 controller definitions for the Expansion HAT. */
export const CONTACT_SENSOR_CONTROLLERS: ControllerSeed[] = [
  {
    controllerId: "mcp1",
    controllerType: "mcp23017",
    controllerModel: "MCP1",
    i2cBus: "/dev/i2c-1",
    i2cAddress: "0x20",
    intaNetName: "IC1_INTA",
    intaPiPhysicalPin: 16,
    intaBcmGpio: 23,
    intbNetName: "IC1_INTB",
    intbPiPhysicalPin: 18,
    intbBcmGpio: 24,
  },
  {
    controllerId: "mcp2",
    controllerType: "mcp23017",
    controllerModel: "MCP2",
    i2cBus: "/dev/i2c-1",
    i2cAddress: "0x21",
    intaNetName: "IC2_INTA",
    intaPiPhysicalPin: 22,
    intaBcmGpio: 25,
    intbNetName: "IC2_INTB",
    intbPiPhysicalPin: 36,
    intbBcmGpio: 16,
  },
  {
    controllerId: "raspberry_pi",
    controllerType: "raspberry_pi_gpio",
    controllerModel: "Raspberry Pi",
    i2cBus: null,
    i2cAddress: null,
    intaNetName: null,
    intaPiPhysicalPin: null,
    intaBcmGpio: null,
    intbNetName: null,
    intbPiPhysicalPin: null,
    intbBcmGpio: null,
  },
];

/**
 * Builds MCP sensor seeds for one expander.
 * @param controllerId - Controller id (`mcp1` or `mcp2`).
 * @param hardwareModel - MCP1 or MCP2 label.
 * @param i2cAddress - I2C address string.
 * @param startNumber - First sensor number (1 or 17).
 * @returns Sixteen sensor seed rows.
 */
function buildMcpSeeds(
  controllerId: string,
  hardwareModel: string,
  i2cAddress: string,
  startNumber: number
): SensorSeed[] {
  const seeds: SensorSeed[] = [];
  for (let pin = 0; pin < 8; pin += 1) {
    const number = startNumber + pin;
    seeds.push({
      sensorNumber: number,
      controllerId,
      hardwareType: "mcp23017",
      hardwareModel,
      schematicNetName: `GPIO_${number.toString().padStart(2, "0")}`,
      i2cAddress,
      mcpBank: "A",
      mcpPin: pin,
    });
  }
  for (let pin = 0; pin < 8; pin += 1) {
    const number = startNumber + 8 + pin;
    seeds.push({
      sensorNumber: number,
      controllerId,
      hardwareType: "mcp23017",
      hardwareModel,
      schematicNetName: `GPIO_${number.toString().padStart(2, "0")}`,
      i2cAddress,
      mcpBank: "B",
      mcpPin: pin,
    });
  }
  return seeds;
}

/** All 38 sensor hardware seeds for the Expansion HAT. */
export const CONTACT_SENSOR_SEEDS: SensorSeed[] = [
  ...buildMcpSeeds("mcp1", "MCP1", "0x20", 1),
  ...buildMcpSeeds("mcp2", "MCP2", "0x21", 17),
  {
    sensorNumber: 33,
    controllerId: "raspberry_pi",
    hardwareType: "raspberry_pi_gpio",
    hardwareModel: "Raspberry Pi",
    schematicNetName: "GPIO_33",
    piPhysicalPin: 11,
    bcmGpio: 17,
  },
  {
    sensorNumber: 34,
    controllerId: "raspberry_pi",
    hardwareType: "raspberry_pi_gpio",
    hardwareModel: "Raspberry Pi",
    schematicNetName: "GPIO_34",
    piPhysicalPin: 13,
    bcmGpio: 27,
  },
  {
    sensorNumber: 35,
    controllerId: "raspberry_pi",
    hardwareType: "raspberry_pi_gpio",
    hardwareModel: "Raspberry Pi",
    schematicNetName: "GPIO_35",
    piPhysicalPin: 15,
    bcmGpio: 22,
  },
  {
    sensorNumber: 36,
    controllerId: "raspberry_pi",
    hardwareType: "raspberry_pi_gpio",
    hardwareModel: "Raspberry Pi",
    schematicNetName: "GPIO_36",
    piPhysicalPin: 29,
    bcmGpio: 5,
  },
  {
    sensorNumber: 37,
    controllerId: "raspberry_pi",
    hardwareType: "raspberry_pi_gpio",
    hardwareModel: "Raspberry Pi",
    schematicNetName: "GPIO_37",
    piPhysicalPin: 31,
    bcmGpio: 6,
  },
  {
    sensorNumber: 38,
    controllerId: "raspberry_pi",
    hardwareType: "raspberry_pi_gpio",
    hardwareModel: "Raspberry Pi",
    schematicNetName: "GPIO_38",
    piPhysicalPin: 37,
    bcmGpio: 26,
  },
];

/**
 * Formats a stable sensor id from sensor number.
 * @param sensorNumber - 1-based sensor index.
 * @returns Sensor id string.
 */
export function sensorIdFromNumber(sensorNumber: number): string {
  return `contact_${sensorNumber.toString().padStart(3, "0")}`;
}
