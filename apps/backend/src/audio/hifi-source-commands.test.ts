import { describe, expect, it } from "vitest";

import {
  buildSourceControllerCommands,
  buildSourceDisplayLineCommand,
  buildSourceEnableCommand,
  buildSourceInputGainCommand,
  buildSourceNameCommand,
} from "./hifi-source-commands.js";

describe("hifi-source-commands", () => {
  it("builds individual SET commands", () => {
    expect(buildSourceNameCommand(3, "Radio")).toBe('*S3NAME"Radio"');
    expect(buildSourceEnableCommand(2, 1)).toBe("*S2ENABLE1");
    expect(buildSourceInputGainCommand(4, 12)).toBe("*S4INGAIN12");
    expect(buildSourceDisplayLineCommand(5, "Now Playing")).toBe('*S5DISPLINE"Now Playing"');
  });

  it("escapes protocol special characters in quoted values", () => {
    expect(buildSourceNameCommand(1, 'Line "A"')).toBe('*S1NAME"Line \\"A\\""');
  });

  it("builds only changed fields", () => {
    expect(
      buildSourceControllerCommands(6, {
        name: "TV",
        enabled: 0,
      })
    ).toEqual(['*S6NAME"TV"', "*S6ENABLE0"]);

    expect(buildSourceControllerCommands(1, {})).toEqual([]);
  });
});
