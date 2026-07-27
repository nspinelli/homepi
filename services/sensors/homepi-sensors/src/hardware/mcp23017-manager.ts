import type { Logger } from "@homepi/core-logging";

/** MCP23017 register addresses. */
const REG = {
  IODIRA: 0x00,
  IODIRB: 0x01,
  GPINTENA: 0x04,
  GPINTENB: 0x05,
  DEFVALA: 0x06,
  DEFVALB: 0x07,
  INTCONA: 0x08,
  INTCONB: 0x09,
  IOCON: 0x0a,
  GPPUA: 0x0c,
  GPPUB: 0x0d,
  INTFA: 0x0e,
  INTFB: 0x0f,
  INTCAPA: 0x10,
  INTCAPB: 0x11,
  GPIOA: 0x12,
  GPIOB: 0x13,
} as const;

type I2cBusModule = {
  openPromisified(bus: number): Promise<{
    writeByte(addr: number, reg: number, value: number): Promise<void>;
    readByte(addr: number, reg: number): Promise<number>;
    close(): Promise<void>;
  }>;
};

/**
 * MCP23017 expander configuration.
 */
export interface McpConfig {
  /** Controller id. */
  controllerId: string;
  /** I2C bus number. */
  busNumber: number;
  /** I2C address (e.g. 0x20). */
  address: number;
  /** INTA BCM GPIO. */
  intaBcmGpio: number;
  /** INTB BCM GPIO. */
  intbBcmGpio: number;
}

/**
 * Callback when an MCP bank changes.
 */
export type McpBankChangeHandler = (
  controllerId: string,
  bank: "A" | "B",
  capturedValue: number
) => void;

/**
 * Manages MCP23017 I2C expanders for contact sensor inputs.
 */
export class Mcp23017Manager {
  private i2c: I2cBusModule | null = null;
  private bus: Awaited<ReturnType<I2cBusModule["openPromisified"]>> | null = null;
  private initialized = false;
  private faulted = false;
  private faultReason: string | null = null;

  /**
   * Creates an MCP manager.
   * @param mcpConfigs - MCP configurations.
   * @param logger - Structured logger.
   */
  constructor(
    private readonly mcpConfigs: McpConfig[],
    private readonly logger: Logger
  ) {}

  /**
   * Returns whether MCP hardware initialized successfully.
   * @returns True when at least one expander responds.
   */
  isHealthy(): boolean {
    return this.initialized && !this.faulted;
  }

  /**
   * Returns the fault reason when unhealthy.
   * @returns Fault reason or null.
   */
  getFaultReason(): string | null {
    return this.faultReason;
  }

  /**
   * Initializes I2C and configures expanders as interrupt-driven inputs.
   */
  async initialize(): Promise<void> {
    try {
      const mod = await import("i2c-bus");
      this.i2c = mod.default as I2cBusModule;
      const busNumber = this.mcpConfigs[0]?.busNumber ?? 1;
      this.bus = await this.i2c.openPromisified(busNumber);

      for (const config of this.mcpConfigs) {
        await this.configureExpander(config);
      }

      this.initialized = true;
      this.logger.info({
        module: "sensors",
        event: "mcp_initialized",
        message: "MCP23017 expanders configured",
        data: { count: this.mcpConfigs.length },
      });
    } catch (error) {
      this.faulted = true;
      this.faultReason =
        error instanceof Error ? error.message : "MCP23017 initialization failed";
      this.logger.error({
        module: "sensors",
        event: "mcp_init_failed",
        message: this.faultReason,
      });
    }
  }

  /**
   * Reads current GPIO values for both banks on one expander.
   * @param config - MCP configuration.
   * @returns Bank A and B values.
   */
  async readBanks(config: McpConfig): Promise<{ bankA: number; bankB: number }> {
    if (!this.bus) {
      throw new Error("MCP bus not open");
    }
    const bankA = await this.bus.readByte(config.address, REG.GPIOA);
    const bankB = await this.bus.readByte(config.address, REG.GPIOB);
    return { bankA, bankB };
  }

  /**
   * Handles an interrupt by reading captured values for the triggered bank.
   * @param controllerId - Controller id.
   * @param bank - Bank that interrupted.
   * @param onChange - Change handler.
   */
  async handleInterrupt(
    controllerId: string,
    bank: "A" | "B",
    onChange: McpBankChangeHandler
  ): Promise<void> {
    const config = this.mcpConfigs.find((c) => c.controllerId === controllerId);
    if (!config || !this.bus) {
      return;
    }

    const intfReg = bank === "A" ? REG.INTFA : REG.INTFB;
    const capReg = bank === "A" ? REG.INTCAPA : REG.INTCAPB;
    const intf = await this.bus.readByte(config.address, intfReg);
    if (intf === 0) {
      return;
    }
    const captured = await this.bus.readByte(config.address, capReg);
    onChange(controllerId, bank, captured);
  }

  /**
   * Closes the I2C bus.
   */
  async close(): Promise<void> {
    if (this.bus) {
      await this.bus.close();
      this.bus = null;
    }
  }

  /**
   * Returns MCP configs for interrupt line wiring.
   * @returns MCP configurations.
   */
  getConfigs(): McpConfig[] {
    return this.mcpConfigs;
  }

  private async configureExpander(config: McpConfig): Promise<void> {
    if (!this.bus) {
      return;
    }

    const addr = config.address;
    await this.bus.writeByte(addr, REG.IOCON, 0x40);
    await this.bus.writeByte(addr, REG.IODIRA, 0xff);
    await this.bus.writeByte(addr, REG.IODIRB, 0xff);
    await this.bus.writeByte(addr, REG.GPPUA, 0xff);
    await this.bus.writeByte(addr, REG.GPPUB, 0xff);
    await this.bus.writeByte(addr, REG.INTCONA, 0x00);
    await this.bus.writeByte(addr, REG.INTCONB, 0x00);
    await this.bus.writeByte(addr, REG.DEFVALA, 0x00);
    await this.bus.writeByte(addr, REG.DEFVALB, 0x00);
    await this.bus.writeByte(addr, REG.GPINTENA, 0xff);
    await this.bus.writeByte(addr, REG.GPINTENB, 0xff);
    await this.bus.readByte(addr, REG.GPIOA);
    await this.bus.readByte(addr, REG.GPIOB);
  }
}

/**
 * Parses an I2C address string like `0x20` to a number.
 * @param address - Address string.
 * @returns Numeric address.
 */
export function parseI2cAddress(address: string): number {
  return Number.parseInt(address, 16);
}
