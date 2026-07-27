declare module "i2c-bus" {
  const i2c: {
    openPromisified(bus: number): Promise<{
      writeByte(addr: number, reg: number, value: number): Promise<void>;
      readByte(addr: number, reg: number): Promise<number>;
      close(): Promise<void>;
    }>;
  };
  export default i2c;
}
