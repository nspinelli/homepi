import { describe, expect, it } from "vitest";

import { BrokerState } from "./broker-state.js";
import { createRequest } from "@homepi/core-messaging";

describe("BrokerState", () => {
  it("handles ping", () => {
    const broker = new BrokerState();
    const request = createRequest({
      source: "test",
      target: "homepi-broker",
      command: "ping",
    });

    expect(broker.handleCommand(request)).toEqual({ pong: true, service: "homepi-broker" });
  });
});
