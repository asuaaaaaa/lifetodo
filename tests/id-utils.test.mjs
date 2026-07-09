import assert from "node:assert/strict";
import { test } from "node:test";

import { createClientId } from "../apps/pwa-prototype/id-utils.js";

test("createClientId uses crypto.randomUUID when available", () => {
  const id = createClientId("task", {
    randomUUID: () => "uuid-value"
  });

  assert.equal(id, "uuid-value");
});

test("createClientId falls back when crypto.randomUUID is unavailable", () => {
  const cryptoLike = {
    getRandomValues(values) {
      values[0] = 0x12345678;
      values[1] = 0x90abcdef;
      return values;
    }
  };

  const id = createClientId("task", cryptoLike, () => 1710000000000);

  assert.match(id, /^task_1710000000000_[0-9a-z]+$/);
  assert.notEqual(id, "task_1710000000000_0");
});
