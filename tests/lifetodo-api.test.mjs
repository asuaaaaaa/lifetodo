import assert from "node:assert/strict";
import { test } from "node:test";

import { createApiHandler } from "../api/server.mjs";

test("GET /api/state returns the state for the requested home", async () => {
  const handler = createApiHandler({
    store: {
      readState: async (homeId) => ({
        members: [{ id: "piggy", name: "猪猪", color: "#ef7f65" }],
        tasks: [],
        devices: [],
        completions: {},
        homeId
      }),
      writeState: async () => {}
    },
    seed: { members: [], tasks: [], devices: [], completions: {} }
  });

  const response = await handler(new Request("http://localhost/api/state?home=demo-home"));
  const body = await response.json();

  assert.equal(response.status, 200);
  assert.equal(body.homeId, "demo-home");
  assert.equal(body.state.members[0].name, "猪猪");
});

test("POST /api/completions toggles a completion and persists the full state", async () => {
  let saved;
  const handler = createApiHandler({
    store: {
      readState: async () => ({ members: [], tasks: [], devices: [], completions: {} }),
      writeState: async (homeId, state, source) => {
        saved = { homeId, state, source };
      }
    },
    seed: { members: [], tasks: [], devices: [], completions: {} },
    now: () => new Date("2026-07-02T08:00:00.000Z")
  });

  const response = await handler(
    new Request("http://localhost/api/completions?home=demo-home", {
      method: "POST",
      body: JSON.stringify({ taskId: "litter", date: "2026-07-02", completed: true, source: "device-esp32" })
    })
  );
  const body = await response.json();

  assert.equal(response.status, 200);
  assert.equal(saved.homeId, "demo-home");
  assert.equal(saved.source, "device-esp32");
  assert.equal(saved.state.completions["litter_2026-07-02"].completedAt, "2026-07-02T08:00:00.000Z");
  assert.equal(body.state.completions["litter_2026-07-02"].source, "device-esp32");
});

test("POST /api/devices/sync-failure sends a notification payload", async () => {
  let notification;
  const handler = createApiHandler({
    store: {
      readState: async () => ({ members: [], tasks: [], devices: [], completions: {} }),
      writeState: async () => {}
    },
    seed: { members: [], tasks: [], devices: [], completions: {} },
    now: () => new Date("2026-07-03T04:00:00.000Z"),
    notifySyncFailure: async (text, body) => {
      notification = { text, body };
      return true;
    }
  });

  const response = await handler(
    new Request("http://localhost/api/devices/sync-failure?home=demo-home", {
      method: "POST",
      body: JSON.stringify({ deviceId: "entry", failures: 3, error: "connection refused" })
    })
  );
  const body = await response.json();

  assert.equal(response.status, 200);
  assert.equal(body.notified, true);
  assert.match(notification.text, /LifeTodo 设备同步失败/);
  assert.match(notification.text, /entry/);
  assert.equal(notification.body.error, "connection refused");
});
