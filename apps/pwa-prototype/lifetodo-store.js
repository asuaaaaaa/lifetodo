const homeId = new URLSearchParams(location.search).get("home") || "demo-home";
const localKey = `lifetodo.${homeId}`;
const apiBase = getApiBase();

let syncTimer;
let pendingCloudWrites = 0;
let writeChain = Promise.resolve();

export function getHomeId() {
  return homeId;
}

export async function createStore(seed, onChange = () => {}) {
  const local = createLocalStore(seed);

  try {
    const cloud = await apiRequest(`/api/state?home=${encodeURIComponent(homeId)}`);
    local.replaceState(cloud.state || seed);
    return createApiStore(local, onChange);
  } catch (error) {
    console.warn("LifeTodo API unavailable, using localStorage.", error);
    return local;
  }
}

function createLocalStore(seed) {
  let state = readLocalState(seed);

  return {
    mode: "local",
    getState() {
      return state;
    },
    replaceState(nextState) {
      state = normalizeState(nextState);
      writeLocalState(state);
    },
    async addTask(task) {
      state.tasks.push(task);
      writeLocalState(state);
    },
    async updateTask(taskId, patch) {
      state.tasks = state.tasks.map((task) => (task.id === taskId ? { ...task, ...patch } : task));
      writeLocalState(state);
    },
    async deleteTask(taskId) {
      state.tasks = state.tasks.filter((task) => task.id !== taskId);
      Object.keys(state.completions).forEach((key) => {
        if (key.startsWith(`${taskId}_`)) {
          delete state.completions[key];
        }
      });
      writeLocalState(state);
    },
    async addMember(member) {
      state.members.push(member);
      writeLocalState(state);
    },
    async updateMember(memberId, patch) {
      state.members = state.members.map((member) => (member.id === memberId ? { ...member, ...patch } : member));
      writeLocalState(state);
    },
    async deleteMember(memberId) {
      state.members = state.members.filter((member) => member.id !== memberId);
      writeLocalState(state);
    },
    async addDevice(device) {
      state.devices = upsertDevice(state.devices, device);
      writeLocalState(state);
    },
    async updateDevice(deviceId, patch) {
      state.devices = state.devices.map((device) => (device.id === deviceId ? { ...device, ...patch } : device));
      writeLocalState(state);
    },
    async deleteDevice(deviceId) {
      state.devices = state.devices.filter((device) => device.id !== deviceId);
      writeLocalState(state);
    },
    async setCompletion(taskId, date, completed, source) {
      setLocalCompletion(state, taskId, date, completed, source);
      writeLocalState(state);
    },
    async toggleCompletion(taskId, date, source) {
      toggleLocalCompletion(state, taskId, date, source);
      writeLocalState(state);
    }
  };
}

function createApiStore(local, onChange) {
  startPolling(local, onChange);

  const syncLater = (source) => {
    writeLocalState(local.getState());
    onChange();
    enqueueApiStateWrite(local.getState(), source, local, onChange);
  };

  return {
    mode: "lark",
    getState: local.getState,
    replaceState: local.replaceState,
    addTask(task) {
      local.getState().tasks.push(task);
      syncLater("addTask");
    },
    updateTask(taskId, patch) {
      local.getState().tasks = local.getState().tasks.map((task) => (task.id === taskId ? { ...task, ...patch } : task));
      syncLater("updateTask");
    },
    deleteTask(taskId) {
      local.getState().tasks = local.getState().tasks.filter((task) => task.id !== taskId);
      Object.keys(local.getState().completions).forEach((key) => {
        if (key.startsWith(`${taskId}_`)) {
          delete local.getState().completions[key];
        }
      });
      syncLater("deleteTask");
    },
    addMember(member) {
      local.getState().members.push(member);
      syncLater("addMember");
    },
    updateMember(memberId, patch) {
      local.getState().members = local
        .getState()
        .members.map((member) => (member.id === memberId ? { ...member, ...patch } : member));
      syncLater("updateMember");
    },
    deleteMember(memberId) {
      local.getState().members = local.getState().members.filter((member) => member.id !== memberId);
      syncLater("deleteMember");
    },
    addDevice(device) {
      local.getState().devices = upsertDevice(local.getState().devices, device);
      syncLater("addDevice");
    },
    updateDevice(deviceId, patch) {
      local.getState().devices = local
        .getState()
        .devices.map((device) => (device.id === deviceId ? { ...device, ...patch } : device));
      syncLater("updateDevice");
    },
    deleteDevice(deviceId) {
      local.getState().devices = local.getState().devices.filter((device) => device.id !== deviceId);
      syncLater("deleteDevice");
    },
    async setCompletion(taskId, date, completed, source) {
      const response = await apiRequest(`/api/completions?home=${encodeURIComponent(homeId)}`, {
        method: "POST",
        body: JSON.stringify({ taskId, date, completed, source })
      });
      local.replaceState(response.state);
      writeLocalState(local.getState());
    },
    async toggleCompletion(taskId, date, source) {
      const completed = !local.getState().completions[`${taskId}_${date}`];
      const response = await apiRequest(`/api/completions?home=${encodeURIComponent(homeId)}`, {
        method: "POST",
        body: JSON.stringify({ taskId, date, completed, source })
      });
      local.replaceState(response.state);
      writeLocalState(local.getState());
    }
  };
}

function startPolling(local, onChange) {
  clearInterval(syncTimer);
  syncTimer = setInterval(async () => {
    if (pendingCloudWrites > 0) return;
    try {
      const response = await apiRequest(`/api/state?home=${encodeURIComponent(homeId)}`);
      local.replaceState(response.state);
      onChange();
    } catch (error) {
      console.warn("LifeTodo API polling failed.", error);
    }
  }, 5000);
}

function enqueueApiStateWrite(state, source, local, onChange) {
  const snapshot = structuredClone(state);
  pendingCloudWrites++;
  writeChain = writeChain
    .catch(() => {})
    .then(async () => {
      const response = await writeApiState(snapshot, source);
      if (pendingCloudWrites === 1 && response?.state) {
        local.replaceState(response.state);
        onChange();
      }
    })
    .catch((error) => {
      console.warn("LifeTodo API background sync failed.", error);
    })
    .finally(() => {
      pendingCloudWrites = Math.max(0, pendingCloudWrites - 1);
    });
  return writeChain;
}

async function writeApiState(state, source) {
  return apiRequest(`/api/state?home=${encodeURIComponent(homeId)}`, {
    method: "PUT",
    body: JSON.stringify({ state, source })
  });
}

async function apiRequest(path, options = {}) {
  const response = await fetch(`${apiBase}${path}`, {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...(options.headers || {})
    }
  });
  if (!response.ok) {
    throw new Error(`LifeTodo API request failed: ${response.status}`);
  }
  return response.json();
}

function getApiBase() {
  const queryValue = new URLSearchParams(location.search).get("api");
  if (queryValue) {
    return queryValue.replace(/\/$/, "");
  }
  if (location.protocol === "http:" || location.protocol === "https:") {
    return location.origin;
  }
  return "https://lifetodo.xyz";
}

function readLocalState(seed) {
  const stored = localStorage.getItem(localKey);
  if (!stored) {
    return structuredClone(seed);
  }
  try {
    return normalizeState(JSON.parse(stored));
  } catch {
    return structuredClone(seed);
  }
}

function writeLocalState(state) {
  localStorage.setItem(localKey, JSON.stringify(state));
}

function normalizeState(state) {
  const completions = state.completions && typeof state.completions === "object" ? state.completions : {};
  const members = Array.isArray(state.members) ? state.members.map(normalizeMember) : [];
  const devices = Array.isArray(state.devices) ? state.devices.map(normalizeDevice) : [];
  const rawTasks = Array.isArray(state.tasks) ? state.tasks.map(normalizeTask) : [];
  
  const todayStr = todayKey();
  const tasks = rawTasks.map((task) => enrichTask(task, completions, members, todayStr));
  
  tasks.sort((a, b) => {
    const scoreA = getTaskSortScore(a);
    const scoreB = getTaskSortScore(b);
    if (scoreA !== scoreB) return scoreA - scoreB;
    return (a.id || "").localeCompare(b.id || "");
  });

  return { members, tasks, devices, completions };
}

function getTaskSortScore(task) {
  if (task.enabled === false) return 100;
  if (task.overdueType === "pending_front") return 10;
  if (task.overdueType === "today_strong") return 20;
  if (task.isDueToday) return 30;
  return 50;
}

function normalizeMember(member) {
  return {
    id: member.id,
    name: member.name || member.displayName || "成员",
    color: member.color || member.avatarColor || "#ef7f65"
  };
}

function normalizeTask(task) {
  const assigneeIds = getTaskAssignees(task);
  return {
    ...task,
    assigneeIds,
    assigneeId: assigneeIds[0] || task.assigneeId || "",
    enabled: task.enabled !== false
  };
}

function getTaskAssignees(task) {
  if (Array.isArray(task?.assigneeIds) && task.assigneeIds.length > 0) {
    return task.assigneeIds.map(String).filter(Boolean);
  }
  if (typeof task?.assigneeId === "string" && task.assigneeId.includes(",")) {
    return task.assigneeId.split(",").map((s) => s.trim()).filter(Boolean);
  }
  if (task?.assigneeId) {
    return [String(task.assigneeId).trim()];
  }
  return [];
}

function enrichTask(task, completions, members, todayStr) {
  const assigneeIds = getTaskAssignees(task);
  const completedCount = Object.values(completions || {}).filter(
    (c) => c?.taskId === task.id && c?.completed !== false
  ).length;

  const cycleIndex = completedCount % (assigneeIds.length || 1);
  const currentAssigneeId = assigneeIds[cycleIndex] || task.assigneeId || "";
  const nextAssigneeId = assigneeIds[(completedCount + 1) % (assigneeIds.length || 1)] || currentAssigneeId;

  const isDueToday = isTaskScheduledOnDate(task, todayStr);
  const isCompletedToday = Boolean(completions?.[`${task.id}_${todayStr}`]);

  const lastReminderDate = findLastReminderDate(task, todayStr);
  const lastReminderCompleted = lastReminderDate ? Boolean(completions?.[`${task.id}_${lastReminderDate}`]) : true;

  const isOverdue = task.enabled !== false && Boolean(lastReminderDate) && !lastReminderCompleted;
  const nextTaskDateAfterLast = lastReminderDate ? findNextScheduledDate(task, lastReminderDate) : null;

  let overdueType = "";
  if (isOverdue && nextTaskDateAfterLast) {
    if (nextTaskDateAfterLast === todayStr) {
      overdueType = "today_strong";
    } else if (nextTaskDateAfterLast > todayStr) {
      overdueType = "pending_front";
    }
  }

  const missedCount = calculateMissedCount(task, completions, todayStr, isDueToday, isOverdue, lastReminderDate);

  return {
    ...task,
    assigneeIds,
    assigneeId: currentAssigneeId,
    cycleInfo: {
      enabled: assigneeIds.length >= 2,
      options: assigneeIds,
      currentIndex: cycleIndex,
      nextAssigneeId
    },
    isDueToday,
    isCompletedToday,
    lastReminderDate,
    lastReminderCompleted,
    nextTaskDate: nextTaskDateAfterLast || findNextScheduledDate(task, todayStr),
    isOverdue,
    overdueType,
    missedCount
  };
}

function calculateMissedCount(task, completions, todayStr, isDueToday, isOverdue, lastReminderDate) {
  if (task.enabled === false) return 0;
  let curDateStr = null;
  if (isDueToday && !completions?.[`${task.id}_${todayStr}`]) {
    curDateStr = todayStr;
  } else if (isOverdue) {
    curDateStr = lastReminderDate;
  }
  if (!curDateStr) return 0;

  let count = 0;
  let iterations = 0;
  while (curDateStr && iterations < 30) {
    iterations++;
    const isDone = Boolean(completions?.[`${task.id}_${curDateStr}`]);
    if (isDone) break;
    count++;
    curDateStr = findPrevScheduledDate(task, curDateStr);
  }
  return count;
}

function isTaskScheduledOnDate(task, dateStr) {
  if (task?.enabled === false) return false;
  const recurrence = task?.recurrence || {};
  const date = parseDate(dateStr);
  if (isNaN(date.getTime())) return false;

  if (recurrence.type === "intervalDays") {
    const anchorStr = recurrence.anchorDate || dateStr;
    const anchor = parseDate(anchorStr);
    const every = Number(recurrence.every || 1);
    if (isNaN(anchor.getTime()) || every <= 0) return false;
    const diff = Math.floor((date - anchor) / 86400000);
    return diff >= 0 && diff % every === 0;
  }
  if (recurrence.type === "weekly") {
    const days = recurrence.daysOfWeek || [];
    return days.includes(date.getDay());
  }
  if (recurrence.type === "monthlyDate") {
    const day = Number(recurrence.day || 1);
    return date.getDate() === day;
  }
  return false;
}

function findLastReminderDate(task, todayStr) {
  const recurrence = task?.recurrence || {};
  if (recurrence.type === "intervalDays") {
    const anchorStr = recurrence.anchorDate || todayStr;
    const anchor = parseDate(anchorStr);
    const today = parseDate(todayStr);
    const every = Number(recurrence.every || 1);
    if (isNaN(anchor.getTime()) || every <= 0) return null;
    const diffDays = Math.floor((today - anchor) / 86400000);
    if (diffDays <= 0) return null;
    const k = Math.floor((diffDays - 1) / every);
    if (k < 0) return null;
    const res = new Date(anchor);
    res.setDate(anchor.getDate() + k * every);
    return todayKey(res);
  }
  if (recurrence.type === "weekly") {
    const days = recurrence.daysOfWeek || [];
    if (!days.length) return null;
    const today = parseDate(todayStr);
    for (let offset = 1; offset <= 7; offset++) {
      const d = new Date(today);
      d.setDate(today.getDate() - offset);
      if (days.includes(d.getDay())) return todayKey(d);
    }
    return null;
  }
  if (recurrence.type === "monthlyDate") {
    const day = Number(recurrence.day || 1);
    const today = parseDate(todayStr);
    if (today.getDate() > day) {
      return todayKey(new Date(today.getFullYear(), today.getMonth(), day));
    }
    return todayKey(new Date(today.getFullYear(), today.getMonth() - 1, day));
  }
  return null;
}

function findNextScheduledDate(task, fromStr) {
  const recurrence = task?.recurrence || {};
  if (recurrence.type === "intervalDays") {
    const anchorStr = recurrence.anchorDate || fromStr;
    const anchor = parseDate(anchorStr);
    const from = parseDate(fromStr);
    const every = Number(recurrence.every || 1);
    if (isNaN(anchor.getTime()) || every <= 0) return null;
    const diffDays = Math.floor((from - anchor) / 86400000);
    const k = Math.floor(diffDays / every) + 1;
    const res = new Date(anchor);
    res.setDate(anchor.getDate() + k * every);
    return todayKey(res);
  }
  if (recurrence.type === "weekly") {
    const days = recurrence.daysOfWeek || [];
    if (!days.length) return null;
    const from = parseDate(fromStr);
    for (let offset = 1; offset <= 7; offset++) {
      const d = new Date(from);
      d.setDate(from.getDate() + offset);
      if (days.includes(d.getDay())) return todayKey(d);
    }
    return null;
  }
  if (recurrence.type === "monthlyDate") {
    const day = Number(recurrence.day || 1);
    const from = parseDate(fromStr);
    const res = new Date(from.getFullYear(), from.getMonth(), day);
    if (res <= from) res.setMonth(res.getMonth() + 1);
    return todayKey(res);
  }
  return null;
}

function findPrevScheduledDate(task, fromStr) {
  const recurrence = task?.recurrence || {};
  if (recurrence.type === "intervalDays") {
    const anchorStr = recurrence.anchorDate || fromStr;
    const anchor = parseDate(anchorStr);
    const from = parseDate(fromStr);
    const every = Number(recurrence.every || 1);
    if (isNaN(anchor.getTime()) || every <= 0) return null;
    const diffDays = Math.floor((from - anchor) / 86400000);
    if (diffDays <= 0) return null;
    const k = Math.floor((diffDays - 1) / every);
    if (k < 0) return null;
    const res = new Date(anchor);
    res.setDate(anchor.getDate() + k * every);
    return todayKey(res);
  }
  if (recurrence.type === "weekly") {
    const days = recurrence.daysOfWeek || [];
    if (!days.length) return null;
    const from = parseDate(fromStr);
    for (let offset = 1; offset <= 7; offset++) {
      const d = new Date(from);
      d.setDate(from.getDate() - offset);
      if (days.includes(d.getDay())) return todayKey(d);
    }
    return null;
  }
  if (recurrence.type === "monthlyDate") {
    const day = Number(recurrence.day || 1);
    const from = parseDate(fromStr);
    if (from.getDate() > day) {
      return todayKey(new Date(from.getFullYear(), from.getMonth(), day));
    }
    return todayKey(new Date(from.getFullYear(), from.getMonth() - 1, day));
  }
  return null;
}

function todayKey(date = new Date()) {
  const year = date.getFullYear();
  const month = String(date.getMonth() + 1).padStart(2, "0");
  const day = String(date.getDate()).padStart(2, "0");
  return `${year}-${month}-${day}`;
}

function parseDate(value) {
  const [year, month, day] = String(value).slice(0, 10).split("-").map(Number);
  if (!year || !month || !day) return new Date(NaN);
  return new Date(year, month - 1, day);
}

function normalizeDevice(device) {
  return {
    id: device.id,
    name: device.name || device.id || "未命名设备",
    location: device.location || "",
    model: device.model || "ESP32-S3-Touch-LCD-4.3",
    status: device.status || "offline",
    lastSeenAt: device.lastSeenAt || null
  };
}

function upsertDevice(devices, device) {
  const normalized = normalizeDevice(device);
  const exists = devices.some((item) => item.id === normalized.id);
  if (!exists) {
    return [...devices, normalized];
  }
  return devices.map((item) => (item.id === normalized.id ? { ...item, ...normalized } : item));
}

function toggleLocalCompletion(state, taskId, date, source) {
  const key = `${taskId}_${date}`;
  if (state.completions[key]) {
    delete state.completions[key];
    return;
  }
  setLocalCompletion(state, taskId, date, true, source);
}

function setLocalCompletion(state, taskId, date, completed, source) {
  const key = `${taskId}_${date}`;
  if (!completed) {
    delete state.completions[key];
    return;
  }
  const completedAt = new Date();
  const task = state.tasks.find((item) => item.id === taskId);
  if (task?.recurrence?.type === "intervalDays") {
    task.recurrence = {
      ...task.recurrence,
      anchorDate: toDateKey(completedAt)
    };
  }
  state.completions[key] = {
    taskId,
    date,
    completed: true,
    completedAt: completedAt.toISOString(),
    source
  };
}

function toDateKey(date) {
  return date.toISOString().slice(0, 10);
}
