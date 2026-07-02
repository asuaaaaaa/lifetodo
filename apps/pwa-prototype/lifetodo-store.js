const homeId = new URLSearchParams(location.search).get("home") || "demo-home";
const localKey = `lifetodo.${homeId}`;
const apiBase = getApiBase();

let syncTimer;

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

  return {
    mode: "lark",
    getState: local.getState,
    replaceState: local.replaceState,
    async addTask(task) {
      local.getState().tasks.push(task);
      await writeApiState(local.getState(), "addTask");
      writeLocalState(local.getState());
    },
    async updateTask(taskId, patch) {
      local.getState().tasks = local.getState().tasks.map((task) => (task.id === taskId ? { ...task, ...patch } : task));
      await writeApiState(local.getState(), "updateTask");
      writeLocalState(local.getState());
    },
    async deleteTask(taskId) {
      local.getState().tasks = local.getState().tasks.filter((task) => task.id !== taskId);
      Object.keys(local.getState().completions).forEach((key) => {
        if (key.startsWith(`${taskId}_`)) {
          delete local.getState().completions[key];
        }
      });
      await writeApiState(local.getState(), "deleteTask");
      writeLocalState(local.getState());
    },
    async addMember(member) {
      local.getState().members.push(member);
      await writeApiState(local.getState(), "addMember");
      writeLocalState(local.getState());
    },
    async updateMember(memberId, patch) {
      local.getState().members = local
        .getState()
        .members.map((member) => (member.id === memberId ? { ...member, ...patch } : member));
      await writeApiState(local.getState(), "updateMember");
      writeLocalState(local.getState());
    },
    async deleteMember(memberId) {
      local.getState().members = local.getState().members.filter((member) => member.id !== memberId);
      await writeApiState(local.getState(), "deleteMember");
      writeLocalState(local.getState());
    },
    async addDevice(device) {
      local.getState().devices = upsertDevice(local.getState().devices, device);
      await writeApiState(local.getState(), "addDevice");
      writeLocalState(local.getState());
    },
    async updateDevice(deviceId, patch) {
      local.getState().devices = local
        .getState()
        .devices.map((device) => (device.id === deviceId ? { ...device, ...patch } : device));
      await writeApiState(local.getState(), "updateDevice");
      writeLocalState(local.getState());
    },
    async deleteDevice(deviceId) {
      local.getState().devices = local.getState().devices.filter((device) => device.id !== deviceId);
      await writeApiState(local.getState(), "deleteDevice");
      writeLocalState(local.getState());
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
    try {
      const response = await apiRequest(`/api/state?home=${encodeURIComponent(homeId)}`);
      local.replaceState(response.state);
      onChange();
    } catch (error) {
      console.warn("LifeTodo API polling failed.", error);
    }
  }, 5000);
}

async function writeApiState(state, source) {
  await apiRequest(`/api/state?home=${encodeURIComponent(homeId)}`, {
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
  return {
    members: Array.isArray(state.members) ? state.members.map(normalizeMember) : [],
    tasks: Array.isArray(state.tasks) ? state.tasks.map(normalizeTask) : [],
    devices: Array.isArray(state.devices) ? state.devices.map(normalizeDevice) : [],
    completions: state.completions && typeof state.completions === "object" ? state.completions : {}
  };
}

function normalizeMember(member) {
  return {
    id: member.id,
    name: member.name || member.displayName || "成员",
    color: member.color || member.avatarColor || "#ef7f65"
  };
}

function normalizeTask(task) {
  return {
    ...task,
    assigneeId: task.assigneeId || task.assigneeIds?.[0],
    enabled: task.enabled !== false
  };
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
  state.completions[key] = {
    taskId,
    date,
    completedAt: new Date().toISOString(),
    source
  };
}
