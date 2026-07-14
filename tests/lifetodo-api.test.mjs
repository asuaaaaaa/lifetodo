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

test("POST /api/completions clears an overdue interval task and re-anchors it to the actual completion date", async () => {
  let saved;
  const handler = createApiHandler({
    store: {
      readState: async () => ({
        members: [{ id: "piggy", name: "猪猪" }],
        tasks: [
          {
            id: "litter",
            title: "铲猫砂盆",
            assigneeId: "piggy",
            recurrence: { type: "intervalDays", every: 3, anchorDate: "2026-07-01" },
            enabled: true
          }
        ],
        devices: [],
        completions: {}
      }),
      writeState: async (homeId, state, source) => {
        saved = { homeId, state, source };
      }
    },
    seed: { members: [], tasks: [], devices: [], completions: {} },
    now: () => new Date("2026-07-06T08:00:00.000Z")
  });

  const response = await handler(
    new Request("http://localhost/api/completions?home=demo-home", {
      method: "POST",
      body: JSON.stringify({ taskId: "litter", date: "2026-07-06", completed: true, source: "device-esp32" })
    })
  );
  const body = await response.json();

  assert.equal(response.status, 200);
  assert.equal(saved.state.completions["litter_2026-07-04"].completed, true);
  assert.equal(saved.state.completions["litter_2026-07-04"].completedAt, "2026-07-06T08:00:00.000Z");
  assert.equal(saved.state.tasks[0].recurrence.anchorDate, "2026-07-06");
  assert.equal(body.state.tasks[0].isOverdue, false);
  assert.equal(body.state.tasks[0].overdueType, "");
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

test("GET /api/state returns enriched task cycling assignee and overdue fields", async () => {
  const handler = createApiHandler({
    store: {
      readState: async (homeId) => ({
        members: [
          { id: "m1", name: "猪猪", color: "#ef7f65" },
          { id: "m2", name: "熊熊", color: "#336699" }
        ],
        tasks: [
          {
            id: "task_cycle",
            title: "倒垃圾",
            assigneeIds: ["m1", "m2"],
            recurrence: { type: "intervalDays", every: 1, anchorDate: "2026-07-01" },
            enabled: true
          }
        ],
        devices: [],
        completions: {
          "task_cycle_2026-07-01": { taskId: "task_cycle", date: "2026-07-01", completed: true }
        },
        homeId
      }),
      writeState: async () => {}
    },
    seed: { members: [], tasks: [], devices: [], completions: {} },
    now: () => new Date("2026-07-02T10:00:00.000Z")
  });

  const response = await handler(new Request("http://localhost/api/state?home=demo-home"));
  const body = await response.json();

  assert.equal(response.status, 200);
  const task = body.state.tasks[0];
  assert.equal(task.cycleInfo.enabled, true);
  assert.equal(task.cycleInfo.currentIndex, 1);
  assert.equal(task.assigneeId, "m2"); // Cycled to member 2
});

test("GET /api/state sorts tasks putting pending_front and today_strong before regular due tasks", async () => {
  const handler = createApiHandler({
    store: {
      readState: async (homeId) => ({
        members: [{ id: "m1", name: "猪猪" }],
        tasks: [
          { id: "normal_due", title: "正常今日", assigneeId: "m1", recurrence: { type: "intervalDays", every: 1, anchorDate: "2026-07-05" }, enabled: true },
          { id: "front_task", title: "前置任务", assigneeId: "m1", recurrence: { type: "intervalDays", every: 3, anchorDate: "2026-07-01" }, enabled: true }
        ],
        devices: [],
        completions: {},
        homeId
      }),
      writeState: async () => {}
    },
    seed: { members: [], tasks: [], devices: [], completions: {} },
    now: () => new Date("2026-07-05T10:00:00.000Z")
  });

  const response = await handler(new Request("http://localhost/api/state?home=demo-home"));
  const body = await response.json();

  assert.equal(body.state.tasks[0].id, "front_task");
  assert.equal(body.state.tasks[0].overdueType, "pending_front");
  assert.equal(body.state.tasks[1].id, "normal_due");
});

test("POST /api/tasks/check-overdue sends strong alert to Lark lobster group when missed count >= 2", async () => {
  let alertText = "";
  const handler = createApiHandler({
    store: {
      readState: async (homeId) => ({
        members: [{ id: "m1", name: "猪猪" }],
        tasks: [
          { id: "t_overdue", title: "铲猫砂", assigneeId: "m1", recurrence: { type: "intervalDays", every: 1, anchorDate: "2026-07-01" }, enabled: true }
        ],
        devices: [],
        completions: {},
        homeId
      }),
      writeState: async () => {}
    },
    seed: { members: [], tasks: [], devices: [], completions: {} },
    now: () => new Date("2026-07-05T10:00:00.000Z"),
    notifyAlert: async (text) => {
      alertText = text;
      return true;
    }
  });

  const response = await handler(new Request("http://localhost/api/tasks/check-overdue?home=demo-home", { method: "POST" }));
  const body = await response.json();

  assert.equal(response.status, 200);
  assert.equal(body.notifiedCount, 1);
  assert.match(alertText, /飞书龙虾组织强提醒/);
  assert.match(alertText, /铲猫砂/);
  assert.match(alertText, /猪猪/);
});

test("GET /api/weather returns Beijing Chaoyang weather summary", async () => {
  const handler = createApiHandler({
    store: {
      readState: async () => ({ members: [], tasks: [], devices: [], completions: {} }),
      writeState: async () => {}
    },
    seed: { members: [], tasks: [], devices: [], completions: {} },
    weatherProvider: async () => ({
      location: "北京朝阳 时间国际",
      temperatureC: 29,
      condition: "多云",
      rainExpected: true,
      rainText: "今天可能下雨",
      precipitationProbability: 72,
      updatedAt: "2026-07-07T08:00:00.000Z"
    })
  });

  const response = await handler(new Request("http://localhost/api/weather?home=demo-home"));
  const body = await response.json();

  assert.equal(response.status, 200);
  assert.equal(body.homeId, "demo-home");
  assert.equal(body.weather.location, "北京朝阳 时间国际");
  assert.equal(body.weather.temperatureC, 29);
  assert.equal(body.weather.rainText, "今天可能下雨");
});

test("GET /api/ha/status returns Home Assistant friendly state", async () => {
  const handler = createApiHandler({
    store: {
      readState: async (homeId) => ({
        members: [{ id: "m1", name: "猪猪" }],
        tasks: [
          { id: "today_done", title: "已经完成", assigneeId: "m1", recurrence: { type: "intervalDays", every: 1, anchorDate: "2026-07-08" }, enabled: true },
          { id: "today_open", title: "还没完成", assigneeId: "m1", recurrence: { type: "intervalDays", every: 1, anchorDate: "2026-07-08" }, enabled: true },
          { id: "future", title: "明天再做", assigneeId: "m1", recurrence: { type: "intervalDays", every: 2, anchorDate: "2026-07-09" }, enabled: true }
        ],
        devices: [{ id: "entry", name: "门口屏", status: "online", lastSeenAt: "2026-07-08T08:03:00.000Z" }],
        completions: {
          "today_done_2026-07-08": { taskId: "today_done", date: "2026-07-08", completed: true }
        },
        homeId
      }),
      writeState: async () => {}
    },
    seed: { members: [], tasks: [], devices: [], completions: {} },
    now: () => new Date("2026-07-08T08:05:00.000Z"),
    weatherProvider: async () => ({
      location: "北京朝阳 时间国际",
      temperatureC: 29,
      condition: "晴",
      rainExpected: true,
      rainText: "近期有雨",
      precipitationProbability: 23,
      updatedAt: "2026-07-08T08:05:00.000Z"
    })
  });

  const response = await handler(new Request("http://localhost/api/ha/status?home=demo-home"));
  const body = await response.json();

  assert.equal(response.status, 200);
  assert.equal(body.homeId, "demo-home");
  assert.equal(body.summary.todayTotal, 2);
  assert.equal(body.summary.completedToday, 1);
  assert.equal(body.summary.remainingToday, 1);
  assert.equal(body.summary.hasStrongAlert, false);
  assert.equal(body.weather.temperatureC, 29);
  assert.equal(body.weather.rainExpected, true);
  assert.equal(body.devices.onlineCount, 1);
  assert.equal(body.tasks[0].id, "today_open");
});

test("POST /api/ha/tasks/:taskId/complete writes today's completion", async () => {
  let saved;
  const handler = createApiHandler({
    store: {
      readState: async (homeId) => ({
        members: [{ id: "m1", name: "猪猪" }],
        tasks: [
          { id: "today_open", title: "还没完成", assigneeId: "m1", recurrence: { type: "intervalDays", every: 1, anchorDate: "2026-07-08" }, enabled: true }
        ],
        devices: [],
        completions: {},
        homeId
      }),
      writeState: async (homeId, state, source) => {
        saved = { homeId, state, source };
      }
    },
    seed: { members: [], tasks: [], devices: [], completions: {} },
    now: () => new Date("2026-07-08T08:05:00.000Z"),
    weatherProvider: async () => ({
      location: "北京朝阳 时间国际",
      temperatureC: 29,
      condition: "晴",
      rainExpected: false,
      rainText: "天气正常",
      precipitationProbability: 0,
      updatedAt: "2026-07-08T08:05:00.000Z"
    })
  });

  const response = await handler(
    new Request("http://localhost/api/ha/tasks/today_open/complete?home=demo-home", {
      method: "POST",
      body: JSON.stringify({ completed: true })
    })
  );
  const body = await response.json();

  assert.equal(response.status, 200);
  assert.equal(saved.homeId, "demo-home");
  assert.equal(saved.source, "home-assistant");
  assert.equal(saved.state.completions["today_open_2026-07-08"].completed, true);
  assert.equal(body.summary.remainingToday, 0);
});
