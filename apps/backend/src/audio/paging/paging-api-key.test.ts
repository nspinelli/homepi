import { describe, expect, it } from "vitest";

import {
  derivePagingApiKeyPrefix,
  hashPagingApiKey,
  normalizePagingApiKey,
  PagingApiKeyValidationError,
  verifyPagingApiKey,
} from "./paging-api-key.js";

describe("paging-api-key", () => {
  it("validates minimum length", () => {
    expect(() => normalizePagingApiKey("short")).toThrow(PagingApiKeyValidationError);
    expect(normalizePagingApiKey("  my-secret-key  ")).toBe("my-secret-key");
  });

  it("derives a display prefix", () => {
    expect(derivePagingApiKeyPrefix("my-secret-key")).toBe("my-secre…");
    expect(derivePagingApiKeyPrefix("12345678")).toBe("12345678");
  });

  it("hashes and verifies keys", async () => {
    const apiKey = "my-home-shortcuts-key";
    const hash = await hashPagingApiKey(apiKey);
    await expect(verifyPagingApiKey(apiKey, hash)).resolves.toBe(true);
    await expect(verifyPagingApiKey("wrong-key-value", hash)).resolves.toBe(false);
  });
});
