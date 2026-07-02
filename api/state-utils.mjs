export function normalizeState(state) {
  return {
    members: Array.isArray(state?.members) ? state.members.map(normalizeMember) : [],
    tasks: Array.isArray(state?.tasks) ? state.tasks.map(normalizeTask) : [],
    devices: Array.isArray(state?.devices) ? state.devices.map(normalizeDevice) : [],
    completions: state?.completions && typeof state.completions === "object" ? state.completions : {}
  };
}

export function normalizeMember(member) {
  return {
    id: member?.id,
    name: member?.name || member?.displayName || "成员",
    color: member?.color || member?.avatarColor || "#ef7f65"
  };
}

export function normalizeTask(task) {
  return {
    ...task,
    assigneeId: task?.assigneeId || task?.assigneeIds?.[0],
    enabled: task?.enabled !== false
  };
}

export function normalizeDevice(device) {
  return {
    id: device?.id,
    name: device?.name || device?.id || "未命名设备",
    location: device?.location || "",
    model: device?.model || "ESP32-S3-Touch-LCD-4.3",
    status: device?.status || "offline",
    lastSeenAt: device?.lastSeenAt || null
  };
}

export function upsertDevice(devices, device) {
  const normalized = normalizeDevice(device);
  const exists = devices.some((item) => item.id === normalized.id);
  if (!exists) {
    return [...devices, normalized];
  }
  return devices.map((item) => (item.id === normalized.id ? { ...item, ...normalized } : item));
}

export function setCompletion(state, taskId, date, completed, source, now = () => new Date()) {
  const key = `${taskId}_${date}`;
  if (!completed) {
    delete state.completions[key];
    return;
  }
  state.completions[key] = {
    taskId,
    date,
    completedAt: now().toISOString(),
    source
  };
}

export function formatBaseDateTime(date) {
  const formatter = new Intl.DateTimeFormat("sv-SE", {
    timeZone: "Asia/Shanghai",
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    hour12: false
  });
  return formatter.format(date);
}
