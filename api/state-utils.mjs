export function normalizeState(state, now = () => new Date()) {
  const completions = state?.completions && typeof state.completions === "object" ? state.completions : {};
  const members = Array.isArray(state?.members) ? state.members.map(normalizeMember) : [];
  const devices = Array.isArray(state?.devices) ? state.devices.map(normalizeDevice) : [];
  const rawTasks = Array.isArray(state?.tasks) ? state.tasks.map(normalizeTask) : [];
  
  const todayStr = todayKey(now());
  const tasks = rawTasks.map((task) => enrichTask(task, completions, members, todayStr));
  
  tasks.sort((a, b) => {
    const scoreA = getTaskSortScore(a);
    const scoreB = getTaskSortScore(b);
    if (scoreA !== scoreB) return scoreA - scoreB;
    return (a.id || "").localeCompare(b.id || "");
  });

  return {
    members,
    tasks,
    devices,
    completions,
    alertHistory: state?.alertHistory && typeof state.alertHistory === "object" ? state.alertHistory : {}
  };
}

function getTaskSortScore(task) {
  if (task.enabled === false) return 100;
  if (task.overdueType === "pending_front") return 10;
  if (task.overdueType === "today_strong") return 20;
  if (task.isDueToday) return 30;
  return 50;
}

export function normalizeMember(member) {
  return {
    id: member?.id,
    name: member?.name || member?.displayName || "成员",
    color: member?.color || member?.avatarColor || "#ef7f65"
  };
}

export function normalizeTask(task) {
  const assigneeIds = getTaskAssignees(task);
  return {
    ...task,
    assigneeIds,
    assigneeId: assigneeIds[0] || task?.assigneeId || "",
    enabled: task?.enabled !== false
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

export function enrichTask(task, completions, members, todayStr) {
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

export function isTaskScheduledOnDate(task, dateStr) {
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

export async function checkAndNotifyOverdueTasks(state, homeId, notifyFn, now = () => new Date()) {
  if (!notifyFn || !state?.tasks) return 0;
  const todayStr = todayKey(now());
  const alertHistory = state.alertHistory || {};
  let notifiedCount = 0;

  for (const task of state.tasks) {
    if (task.enabled === false) continue;
    if (task.missedCount >= 2 && !task.isCompletedToday) {
      const alertKey = `${task.id}_${todayStr}`;
      if (!alertHistory[alertKey]) {
        const member = state.members?.find((m) => m.id === task.assigneeId);
        const text = [
          "🚨 【LifeTodo 飞书龙虾组织强提醒】连续未执行警报",
          `家庭：${homeId || "默认家庭"}`,
          `任务：${task.title}`,
          `负责成员：${member?.name || task.assigneeId} (${task.assigneeId})`,
          `连续未做：${task.missedCount} 次`,
          `上次提醒：${task.lastReminderDate || "未知"}`,
          `请负责人立即执行并打卡！`
        ].join("\n");
        try {
          const success = await notifyFn(text, {
            taskId: task.id,
            homeId,
            missedCount: task.missedCount,
            overdueType: task.overdueType
          });
          if (success) {
            alertHistory[alertKey] = now().toISOString();
            notifiedCount++;
          }
        } catch (err) {
          console.warn("Failed to notify overdue task:", err);
        }
      }
    }
  }
  state.alertHistory = alertHistory;
  return notifiedCount;
}

export function todayKey(date = new Date()) {
  const year = date.getFullYear();
  const month = String(date.getMonth() + 1).padStart(2, "0");
  const day = String(date.getDate()).padStart(2, "0");
  return `${year}-${month}-${day}`;
}

export function parseDate(value) {
  const [year, month, day] = String(value).slice(0, 10).split("-").map(Number);
  if (!year || !month || !day) return new Date(NaN);
  return new Date(year, month - 1, day);
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
  const completedAt = now();
  const completedAtDate = todayKey(completedAt);
  const task = state.tasks?.find((item) => item.id === taskId);
  const effectiveDate = completed ? completionTargetDate(task, date, completedAtDate) : date;
  const key = `${taskId}_${effectiveDate}`;
  if (!completed) {
    delete state.completions[key];
    return;
  }

  if (task?.recurrence?.type === "intervalDays") {
    task.recurrence = {
      ...task.recurrence,
      anchorDate: completedAtDate
    };
  }

  state.completions[key] = {
    taskId,
    date: effectiveDate,
    completed: true,
    completedAt: completedAt.toISOString(),
    source
  };
}

function completionTargetDate(task, requestedDate, completedAtDate) {
  if (!task) return requestedDate;
  if (requestedDate === completedAtDate && task.isOverdue && task.lastReminderDate) {
    return task.lastReminderDate;
  }
  return requestedDate;
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
