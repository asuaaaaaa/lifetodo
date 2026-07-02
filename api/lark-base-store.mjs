import { execFile as nodeExecFile } from "node:child_process";
import { promisify } from "node:util";

import { formatBaseDateTime, normalizeState } from "./state-utils.mjs";

const execFileDefault = promisify(nodeExecFile);
const TODO_FIELDS = [
  "Task ID",
  "Title",
  "Assignee ID",
  "Assignee Name",
  "Assignee Color",
  "Enabled",
  "Recurrence Type",
  "Interval Every",
  "Anchor Date",
  "Weekdays",
  "Monthly Day",
  "Next Date",
  "Label",
  "Last Completed Date",
  "Last Completed At",
  "Last Completion Source"
];

export function createLarkBaseStore({
  baseToken = process.env.LIFETODO_LARK_BASE_TOKEN,
  tableId = process.env.LIFETODO_LARK_TODO_TABLE_ID || process.env.LIFETODO_LARK_TABLE_ID,
  execFile = execFileDefault,
  now = () => new Date()
} = {}) {
  if (!baseToken || !tableId) {
    throw new Error("Missing LIFETODO_LARK_BASE_TOKEN or LIFETODO_LARK_TODO_TABLE_ID");
  }

  async function listTodoRecords() {
    const args = ["base", "+record-list", "--base-token", baseToken, "--table-id", tableId];
    for (const field of TODO_FIELDS) {
      args.push("--field-id", field);
    }
    args.push("--limit", "200", "--format", "json", "--as", "user");
    const result = await execFile("lark-cli", args);
    return parseItems(result.stdout);
  }

  async function listTodoIndex() {
    const result = await execFile("lark-cli", [
      "base",
      "+record-list",
      "--base-token",
      baseToken,
      "--table-id",
      tableId,
      "--field-id",
      "Task ID",
      "--limit",
      "200",
      "--format",
      "json",
      "--as",
      "user"
    ]);
    return parseItems(result.stdout);
  }

  return {
    async readState(homeId, fallback = null) {
      const fallbackState = normalizeState(fallback || {});
      const records = await listTodoRecords();
      const tasks = records.map(recordToTask).filter((task) => task.id);
      const members = mergeMembers(fallbackState.members, records.map(recordToMember).filter((member) => member.id));
      const completions = {};
      for (const record of records) {
        const completion = recordToCompletion(record.fields || {});
        if (completion) {
          completions[`${completion.taskId}_${completion.date}`] = completion;
        }
      }
      return normalizeState({
        members,
        tasks,
        devices: fallbackState.devices,
        completions
      });
    },

    async writeState(homeId, state, source = "api") {
      const normalized = normalizeState(state);
      const records = await listTodoIndex();
      const recordByTaskId = new Map(records.map((record) => [readField(record.fields || {}, "Task ID"), record]));
      const nextTaskIds = new Set(normalized.tasks.map((task) => task.id));

      for (const task of normalized.tasks) {
        const payload = taskToRecord(task, normalized, now);
        const args = [
          "base",
          "+record-upsert",
          "--base-token",
          baseToken,
          "--table-id",
          tableId,
          "--json",
          JSON.stringify(payload),
          "--as",
          "user",
          "--format",
          "json"
        ];
        const existing = recordByTaskId.get(task.id);
        if (existing?.record_id) {
          args.splice(6, 0, "--record-id", existing.record_id);
        }
        await execFile("lark-cli", args);
      }

      const removedRecordIds = records
        .filter((record) => !nextTaskIds.has(readField(record.fields || {}, "Task ID")))
        .map((record) => record.record_id)
        .filter(Boolean);
      if (removedRecordIds.length) {
        const deleteArgs = [
          "base",
          "+record-delete",
          "--base-token",
          baseToken,
          "--table-id",
          tableId
        ];
        for (const recordId of removedRecordIds) {
          deleteArgs.push("--record-id", recordId);
        }
        deleteArgs.push("--yes", "--as", "user", "--format", "json");
        await execFile("lark-cli", deleteArgs);
      }

      return normalized;
    }
  };
}

function recordToTask(record) {
  const fields = record.fields || {};
  const recurrenceType = readField(fields, "Recurrence Type") || "intervalDays";
  return {
    id: readField(fields, "Task ID"),
    title: readField(fields, "Title") || "事项",
    assigneeId: readField(fields, "Assignee ID"),
    recurrence: recordToRecurrence(fields, recurrenceType),
    label: readField(fields, "Label") || recurrenceLabel(fields, recurrenceType),
    nextDate: readField(fields, "Next Date") || null,
    enabled: readBoolean(fields, "Enabled", true)
  };
}

function recordToMember(record) {
  const fields = record.fields || {};
  return {
    id: readField(fields, "Assignee ID"),
    name: readField(fields, "Assignee Name") || readField(fields, "Assignee ID"),
    color: readField(fields, "Assignee Color") || "#ef7f65"
  };
}

function recordToRecurrence(fields, recurrenceType) {
  if (recurrenceType === "weekly") {
    return {
      type: "weekly",
      daysOfWeek: parseWeekdays(readField(fields, "Weekdays"))
    };
  }
  if (recurrenceType === "monthlyDate") {
    return {
      type: "monthlyDate",
      day: Number(readField(fields, "Monthly Day") || 1)
    };
  }
  return {
    type: "intervalDays",
    every: Number(readField(fields, "Interval Every") || 1),
    anchorDate: readField(fields, "Anchor Date") || new Date().toISOString().slice(0, 10)
  };
}

function recordToCompletion(fields) {
  const taskId = readField(fields, "Task ID");
  const date = readField(fields, "Last Completed Date");
  if (!taskId || !date) return null;
  return {
    taskId,
    date,
    completedAt: readField(fields, "Last Completed At") || `${date}T00:00:00.000Z`,
    source: readField(fields, "Last Completion Source") || "api"
  };
}

function taskToRecord(task, state, now) {
  const member = state.members.find((item) => item.id === task.assigneeId);
  const completion = latestCompletionForTask(state.completions, task.id);
  const recurrence = task.recurrence || { type: "intervalDays", every: 1, anchorDate: todayKey(now()) };
  return {
    "Task ID": task.id,
    Title: task.title || "事项",
    "Assignee ID": task.assigneeId || "",
    "Assignee Name": member?.name || task.assigneeId || "",
    "Assignee Color": member?.color || "#ef7f65",
    Enabled: task.enabled !== false,
    "Recurrence Type": recurrence.type || "intervalDays",
    "Interval Every": recurrence.type === "intervalDays" ? Number(recurrence.every || 1) : null,
    "Anchor Date": recurrence.type === "intervalDays" ? recurrence.anchorDate || todayKey(now()) : null,
    Weekdays: recurrence.type === "weekly" ? (recurrence.daysOfWeek || []).join(",") : "",
    "Monthly Day": recurrence.type === "monthlyDate" ? Number(recurrence.day || 1) : null,
    "Next Date": nextDueDate(task, completion?.date || null, now()),
    Label: task.label || recurrenceLabelFromTask(task),
    "Last Completed Date": completion?.date || null,
    "Last Completed At": completion?.completedAt ? formatBaseDateTime(new Date(completion.completedAt)) : null,
    "Last Completion Source": completion?.source || null
  };
}

function latestCompletionForTask(completions, taskId) {
  return Object.values(completions || {})
    .filter((completion) => completion?.taskId === taskId)
    .sort((a, b) => String(b.date).localeCompare(String(a.date)))[0];
}

function nextDueDate(task, lastCompletedDate, now) {
  const recurrence = task.recurrence || {};
  const from = parseDate(lastCompletedDate || todayKey(now));
  if (recurrence.type === "monthlyDate") {
    const day = Number(recurrence.day || 1);
    const next = new Date(from.getFullYear(), from.getMonth(), day);
    if (next <= from) next.setMonth(next.getMonth() + 1);
    return todayKey(next);
  }
  if (recurrence.type === "weekly") {
    const days = [...(recurrence.daysOfWeek || [])].sort((a, b) => a - b);
    for (let offset = 1; offset <= 7; offset++) {
      const next = new Date(from);
      next.setDate(from.getDate() + offset);
      if (days.includes(next.getDay())) return todayKey(next);
    }
  }
  const next = new Date(from);
  next.setDate(from.getDate() + Number(recurrence.every || 1));
  return todayKey(next);
}

function recurrenceLabel(fields, recurrenceType) {
  if (recurrenceType === "weekly") return `每周 ${readField(fields, "Weekdays") || ""}`;
  if (recurrenceType === "monthlyDate") return `每月 ${readField(fields, "Monthly Day") || 1} 日`;
  return `每 ${readField(fields, "Interval Every") || 1} 天`;
}

function recurrenceLabelFromTask(task) {
  const recurrence = task.recurrence || {};
  if (recurrence.type === "weekly") return `每周 ${(recurrence.daysOfWeek || []).join(",")}`;
  if (recurrence.type === "monthlyDate") return `每月 ${recurrence.day || 1} 日`;
  return `每 ${recurrence.every || 1} 天`;
}

function mergeMembers(fallbackMembers, recordMembers) {
  const byId = new Map();
  for (const member of fallbackMembers) byId.set(member.id, member);
  for (const member of recordMembers) byId.set(member.id, member);
  return [...byId.values()];
}

function parseWeekdays(value) {
  return String(value || "")
    .split(",")
    .map((item) => Number(item.trim()))
    .filter((item) => Number.isInteger(item) && item >= 0 && item <= 6);
}

function parseDate(value) {
  const [year, month, day] = String(value).slice(0, 10).split("-").map(Number);
  if (!year || !month || !day) return new Date();
  return new Date(year, month - 1, day);
}

function todayKey(date) {
  const year = date.getFullYear();
  const month = String(date.getMonth() + 1).padStart(2, "0");
  const day = String(date.getDate()).padStart(2, "0");
  return `${year}-${month}-${day}`;
}

function parseItems(stdout) {
  const parsed = JSON.parse(stdout || "{}");
  if (Array.isArray(parsed?.data?.items)) return parsed.data.items;
  if (Array.isArray(parsed?.items)) return parsed.items;
  if (Array.isArray(parsed?.data?.records)) return parsed.data.records;
  if (Array.isArray(parsed?.data?.data) && Array.isArray(parsed?.data?.fields)) {
    return parsed.data.data.map((row, index) => {
      const fields = {};
      parsed.data.fields.forEach((field, fieldIndex) => {
        fields[field] = row[fieldIndex];
      });
      return {
        record_id: parsed.data.record_id_list?.[index],
        fields
      };
    });
  }
  return [];
}

function readBoolean(fields, name, fallback) {
  const value = readField(fields, name);
  if (typeof value === "boolean") return value;
  if (value === "true") return true;
  if (value === "false") return false;
  return fallback;
}

function readField(fields, name) {
  const value = fields[name];
  if (Array.isArray(value)) {
    return value.map((item) => item?.text || item).join("");
  }
  if (value && typeof value === "object" && "text" in value) {
    return value.text;
  }
  return value;
}
