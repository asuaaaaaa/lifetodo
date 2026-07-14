import assert from "node:assert/strict";
import { test } from "node:test";

import { createLarkBaseStore } from "../api/lark-base-store.mjs";

const todoRow = {
  "Task ID": "litter",
  Title: "铲猫砂盆",
  "Assignee ID": "piggy",
  "Assignee Name": "猪猪",
  "Assignee Color": "#ef7f65",
  Enabled: true,
  "Recurrence Type": "intervalDays",
  "Interval Every": 3,
  "Anchor Date": "2026-07-02",
  Weekdays: "",
  "Monthly Day": null,
  "Next Date": "2026-07-05",
  Label: "每 3 天",
  "Last Completed Date": "2026-07-02",
  "Last Completed At": "2026-07-02 16:00:00",
  "Last Completion Source": "device-esp32"
};

test("reads structured TODO records from one home table into app state", async () => {
  const calls = [];
  const execFile = async (command, args) => {
    calls.push([command, args]);
    return {
      stdout: JSON.stringify({
        ok: true,
        data: {
          data: [Object.values(todoRow)],
          fields: Object.keys(todoRow),
          record_id_list: ["rec_litter"]
        }
      })
    };
  };

  const store = createLarkBaseStore({
    baseToken: "bas_demo",
    tableId: "tbl_todos",
    execFile
  });

  const state = await store.readState("demo-home", { members: [], tasks: [], devices: [], completions: {} });

  assert.equal(state.tasks[0].id, "litter");
  assert.equal(state.tasks[0].title, "铲猫砂盆");
  assert.deepEqual(state.tasks[0].recurrence, { type: "intervalDays", every: 3, anchorDate: "2026-07-02" });
  assert.equal(state.members[0].name, "猪猪");
  assert.equal(state.completions["litter_2026-07-02"].source, "device-esp32");
  assert.ok(calls[0][1].includes("Task ID"));
  assert.ok(!calls[0][1].includes("State JSON"));
});

test("writes structured TODO rows without storing a JSON state snapshot", async () => {
  const calls = [];
  const execFile = async (command, args) => {
    calls.push([command, args]);
    if (args.includes("+record-list")) {
      return {
        stdout: JSON.stringify({
          ok: true,
          data: {
            data: [["litter"]],
            fields: ["Task ID"],
            record_id_list: ["rec_litter"]
          }
        })
      };
    }
    return { stdout: JSON.stringify({ ok: true }) };
  };

  const store = createLarkBaseStore({
    baseToken: "bas_demo",
    tableId: "tbl_todos",
    execFile,
    now: () => new Date("2026-07-02T08:00:00.000Z")
  });

  await store.writeState(
    "demo-home",
    {
      members: [{ id: "piggy", name: "猪猪", color: "#ef7f65" }],
      tasks: [
        {
          id: "litter",
          title: "铲猫砂盆",
          assigneeId: "piggy",
          recurrence: { type: "intervalDays", every: 3, anchorDate: "2026-07-02" },
          label: "每 3 天",
          enabled: true
        }
      ],
      devices: [],
      completions: {
        "litter_2026-07-02": {
          taskId: "litter",
          date: "2026-07-02",
          completedAt: "2026-07-02T08:00:00.000Z",
          source: "device-esp32"
        }
      }
    },
    "device-esp32"
  );

  const upsert = calls.find(([, args]) => args.includes("+record-upsert"));
  assert.ok(upsert);
  assert.ok(upsert[1].includes("--record-id"));
  const json = JSON.parse(upsert[1][upsert[1].indexOf("--json") + 1]);
  assert.equal(json["Task ID"], "litter");
  assert.equal(json.Title, "铲猫砂盆");
  assert.equal(json["Assignee Name"], "猪猪");
  assert.equal(json["Next Date"], "2026-07-05");
  assert.equal(json["Last Completed Date"], "2026-07-02");
  assert.ok(!("State JSON" in json));
});

test("reads completion records from the dedicated completion table", async () => {
  const calls = [];
  const execFile = async (command, args) => {
    calls.push([command, args]);
    if (args.includes("tbl_completions")) {
      return {
        stdout: JSON.stringify({
          ok: true,
          data: {
            data: [["litter_2026-07-03", "litter", "2026-07-03", true, "2026-07-03 08:10:00", "app-h5"]],
            fields: ["Completion ID", "Task ID", "Date", "Completed", "Completed At", "Source"],
            record_id_list: ["rec_completion"]
          }
        })
      };
    }
    return {
      stdout: JSON.stringify({
        ok: true,
        data: {
          data: [Object.values(todoRow)],
          fields: Object.keys(todoRow),
          record_id_list: ["rec_litter"]
        }
      })
    };
  };

  const store = createLarkBaseStore({
    baseToken: "bas_demo",
    tableId: "tbl_todos",
    completionTableId: "tbl_completions",
    execFile
  });

  const state = await store.readState("demo-home", { members: [], tasks: [], devices: [], completions: {} });

  assert.equal(state.completions["litter_2026-07-03"].source, "app-h5");
  assert.equal(state.completions["litter_2026-07-02"], undefined);
  assert.ok(calls.some(([, args]) => args.includes("tbl_completions")));
});

test("writes completion records to the dedicated completion table", async () => {
  const calls = [];
  const execFile = async (command, args) => {
    calls.push([command, args]);
    if (args.includes("tbl_completions")) {
      return {
        stdout: JSON.stringify({
          ok: true,
          data: {
            data: [["litter_2026-07-03"]],
            fields: ["Completion ID"],
            record_id_list: ["rec_completion"]
          }
        })
      };
    }
    if (args.includes("+record-list")) {
      return {
        stdout: JSON.stringify({
          ok: true,
          data: {
            data: [["litter"]],
            fields: ["Task ID"],
            record_id_list: ["rec_litter"]
          }
        })
      };
    }
    return { stdout: JSON.stringify({ ok: true }) };
  };

  const store = createLarkBaseStore({
    baseToken: "bas_demo",
    tableId: "tbl_todos",
    completionTableId: "tbl_completions",
    execFile,
    now: () => new Date("2026-07-03T08:00:00.000Z")
  });

  await store.writeState(
    "demo-home",
    {
      members: [{ id: "piggy", name: "猪猪", color: "#ef7f65" }],
      tasks: [
        {
          id: "litter",
          title: "铲猫砂盆",
          assigneeId: "piggy",
          recurrence: { type: "intervalDays", every: 3, anchorDate: "2026-07-02" },
          label: "每 3 天",
          enabled: true
        }
      ],
      devices: [],
      completions: {
        "litter_2026-07-03": {
          taskId: "litter",
          date: "2026-07-03",
          completedAt: "2026-07-03T08:00:00.000Z",
          source: "device-esp32"
        }
      }
    },
    "device-esp32"
  );

  const completionUpsert = calls.find(([, args]) => args.includes("+record-upsert") && args.includes("tbl_completions"));
  assert.ok(completionUpsert);
  assert.ok(completionUpsert[1].includes("--record-id"));
  const json = JSON.parse(completionUpsert[1][completionUpsert[1].indexOf("--json") + 1]);
  assert.equal(json["Completion ID"], "litter_2026-07-03");
  assert.equal(json.Completed, true);
  assert.equal(json.Source, "device-esp32");
});

test("calculates next dates using actual completion date for interval tasks and calendar dates for fixed tasks", async () => {
  const calls = [];
  const execFile = async (command, args) => {
    calls.push([command, args]);
    if (args.includes("+record-list")) {
      return {
        stdout: JSON.stringify({
          ok: true,
          data: {
            data: [["interval"], ["monthly"]],
            fields: ["Task ID"],
            record_id_list: ["rec_interval", "rec_monthly"]
          }
        })
      };
    }
    return { stdout: JSON.stringify({ ok: true }) };
  };

  const store = createLarkBaseStore({
    baseToken: "bas_demo",
    tableId: "tbl_todos",
    execFile,
    now: () => new Date("2026-07-06T08:00:00.000Z")
  });

  await store.writeState(
    "demo-home",
    {
      members: [{ id: "piggy", name: "猪猪", color: "#ef7f65" }],
      tasks: [
        {
          id: "interval",
          title: "间隔任务",
          assigneeId: "piggy",
          recurrence: { type: "intervalDays", every: 3, anchorDate: "2026-07-06" },
          enabled: true
        },
        {
          id: "monthly",
          title: "固定日期任务",
          assigneeId: "piggy",
          recurrence: { type: "monthlyDate", day: 2 },
          enabled: true
        }
      ],
      devices: [],
      completions: {
        "interval_2026-07-04": {
          taskId: "interval",
          date: "2026-07-04",
          completedAt: "2026-07-06T08:00:00.000Z",
          source: "device-esp32"
        },
        "monthly_2026-07-02": {
          taskId: "monthly",
          date: "2026-07-02",
          completedAt: "2026-07-06T08:00:00.000Z",
          source: "device-esp32"
        }
      }
    },
    "device-esp32"
  );

  const upserts = calls.filter(([, args]) => args.includes("+record-upsert"));
  const records = upserts.map(([, args]) => JSON.parse(args[args.indexOf("--json") + 1]));
  const interval = records.find((record) => record["Task ID"] === "interval");
  const monthly = records.find((record) => record["Task ID"] === "monthly");

  assert.equal(interval["Anchor Date"], "2026-07-06");
  assert.equal(interval["Next Date"], "2026-07-09");
  assert.equal(monthly["Next Date"], "2026-08-02");
});

test("deletes TODO records that no longer exist in app state", async () => {
  const calls = [];
  const execFile = async (command, args) => {
    calls.push([command, args]);
    if (args.includes("+record-list")) {
      return {
        stdout: JSON.stringify({
          ok: true,
          data: {
            data: [["old-task"]],
            fields: ["Task ID"],
            record_id_list: ["rec_old"]
          }
        })
      };
    }
    return { stdout: JSON.stringify({ ok: true }) };
  };

  const store = createLarkBaseStore({
    baseToken: "bas_demo",
    tableId: "tbl_todos",
    execFile
  });

  await store.writeState("demo-home", { members: [], tasks: [], devices: [], completions: {} }, "deleteTask");

  const deletion = calls.find(([, args]) => args.includes("+record-delete"));
  assert.ok(deletion);
  assert.ok(deletion[1].includes("rec_old"));
  assert.ok(deletion[1].includes("--yes"));
});
