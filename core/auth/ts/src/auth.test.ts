import { describe, expect, it } from "vitest";
import principalExample from "../../examples/principal.example.json" with { type: "json" };
import principalSchema from "../../schema/principal.schema.json" with { type: "json" };
import { validateAgainstSchema } from "@homepi/tooling-schema-validation";
import { createServicePrincipal } from "./create-service-principal.js";
import { hasPermission } from "./has-permission.js";

describe("Principal", () => {
  it("validates the documented example", () => {
    const result = validateAgainstSchema(principalSchema, principalExample);
    expect(result.valid).toBe(true);
  });

  it("creates service principals", () => {
    const principal = createServicePrincipal({
      id: "svc-homepi-backend",
      displayName: "HomePi Backend",
      roles: ["backend"],
    });
    expect(principal.type).toBe("service");
  });

  it("evaluates allow and deny permissions", () => {
    expect(
      hasPermission([{ resource: "config", action: "read", effect: "allow" }], "config", "read")
    ).toBe(true);
    expect(
      hasPermission([{ resource: "config", action: "read", effect: "deny" }], "config", "read")
    ).toBe(false);
  });
});
