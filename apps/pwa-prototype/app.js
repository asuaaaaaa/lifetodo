import { createStore, getHomeId } from "./lifetodo-store.js";

const todayKey = new Date().toISOString().slice(0, 10);
const isDevicePage = document.body.classList.contains("device-only");
const currentDeviceId = new URLSearchParams(location.search).get("device");

const seed = {
  members: [
    { id: "piggy", name: "猪猪", color: "#ef7f65" },
    { id: "xingyue", name: "星月", color: "#5d8fb4" }
  ],
  tasks: [
    {
      id: "litter",
      title: "铲猫砂盆",
      assigneeId: "piggy",
      recurrence: { type: "intervalDays", every: 3, anchorDate: todayKey },
      label: "每 3 天"
    },
    {
      id: "toilet",
      title: "清洁马桶",
      assigneeId: "xingyue",
      recurrence: { type: "monthlyDate", day: new Date().getDate() },
      label: "每月 1 日"
    },
    {
      id: "plants",
      title: "给阳台植物浇水",
      assigneeId: "piggy",
      recurrence: { type: "intervalDays", every: 2, anchorDate: todayKey },
      label: "每 2 天"
    }
  ],
  devices: [
    {
      id: "entry-screen",
      name: "玄关屏",
      location: "门口",
      model: "ESP32-S3-Touch-LCD-4.3",
      status: "offline",
      lastSeenAt: null
    }
  ],
  completions: {}
};

let store;

const views = {
  today: document.querySelector("#todayView"),
  tasks: document.querySelector("#tasksView"),
  members: document.querySelector("#membersView"),
  device: document.querySelector("#deviceView")
};

bootstrap();

async function bootstrap() {
  store = await createStore(seed, () => render());
  await migrateDefaultMembers();
  bindEvents();
  updateRecurrenceFields();
  startDeviceHeartbeat();
  render();
}

async function migrateDefaultMembers() {
  const current = state();
  const defaultMemberIds = current.members.map((member) => member.id).sort().join(",");
  if (defaultMemberIds !== "dad,kid,mom") {
    return;
  }

  await store.updateMember("mom", { id: "piggy", name: "猪猪", color: "#ef7f65" });
  await store.updateMember("dad", { id: "xingyue", name: "星月", color: "#5d8fb4" });
  await store.deleteMember("kid");
  await Promise.all(
    current.tasks.map((task) => {
      const assigneeId = task.assigneeId === "dad" ? "xingyue" : "piggy";
      return store.updateTask(task.id, { assigneeId });
    })
  );
}

function bindEvents() {
  document.querySelectorAll(".segment").forEach((button) => {
    button.addEventListener("click", () => {
      document.querySelectorAll(".segment").forEach((item) => item.classList.remove("active"));
      button.classList.add("active");
      Object.values(views)
        .filter(Boolean)
        .forEach((view) => view.classList.remove("active"));
      views[button.dataset.view]?.classList.add("active");
    });
  });

  document.querySelector("#resetData")?.addEventListener("click", () => {
    localStorage.clear();
    location.reload();
  });

  document.querySelector("#taskForm")?.addEventListener("submit", async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    const recurrence = parseRecurrence(form);
    const { assigneeIds, assigneeId } = parseAssignee(form);
    await store.addTask({
      id: crypto.randomUUID(),
      title: String(form.get("title")).trim(),
      assigneeId,
      assigneeIds,
      recurrence: recurrence.value,
      label: recurrence.label,
      enabled: true
    });
    event.currentTarget.reset();
    updateRecurrenceFields();
    updateAssigneeModeFields();
    render();
  });

  document.querySelectorAll("input[name='assigneeMode']").forEach((input) => {
    input.addEventListener("change", updateAssigneeModeFields);
  });

  document.querySelector("#memberForm")?.addEventListener("submit", async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    await store.addMember({
      id: crypto.randomUUID(),
      name: String(form.get("name")).trim(),
      color: String(form.get("color") || "#ef7f65")
    });
    event.currentTarget.reset();
    render();
  });

  document.querySelector("#deviceForm")?.addEventListener("submit", async (event) => {
    event.preventDefault();
    const form = new FormData(event.currentTarget);
    const deviceId = String(form.get("deviceId")).trim();
    if (!deviceId) return;
    await store.addDevice({
      id: deviceId,
      name: String(form.get("name")).trim() || deviceId,
      location: String(form.get("location")).trim(),
      model: "ESP32-S3-Touch-LCD-4.3",
      status: "offline",
      lastSeenAt: null
    });
    event.currentTarget.reset();
    render();
  });

  document.querySelector("#recurrenceType")?.addEventListener("change", updateRecurrenceFields);
}

function parseRecurrence(form) {
  const type = String(form.get("recurrenceType"));
  if (type === "weekly") {
    const weekday = Number(form.get("weekday"));
    return { label: `每周${weekdayLabel(weekday)}`, value: { type: "weekly", daysOfWeek: [weekday] } };
  }
  if (type === "monthlyDate") {
    const day = clamp(Number(form.get("monthDay")), 1, 31);
    return { label: `每月 ${day} 日`, value: { type: "monthlyDate", day } };
  }
  const every = clamp(Number(form.get("intervalEvery")), 1, 60);
  return { label: `每 ${every} 天`, value: { type: "intervalDays", every, anchorDate: todayKey } };
}

function parseAssignee(form) {
  const mode = String(form.get("assigneeMode") || "single");
  if (mode === "cycleAll") {
    const allIds = state().members.map((m) => m.id);
    return { assigneeIds: allIds, assigneeId: allIds[0] || "" };
  }
  if (mode === "cycleSelect") {
    const ids = form.getAll("cycleMemberId").map(String).filter(Boolean);
    if (ids.length > 0) {
      return { assigneeIds: ids, assigneeId: ids[0] };
    }
    const allIds = state().members.map((m) => m.id);
    return { assigneeIds: allIds, assigneeId: allIds[0] || "" };
  }
  const selected = String(form.get("singleAssigneeId") || "");
  return { assigneeIds: selected ? [selected] : [], assigneeId: selected };
}

function updateAssigneeModeFields() {
  const mode = document.querySelector("input[name='assigneeMode']:checked")?.value || "single";
  const singleField = document.querySelector("#singleAssigneeField");
  const cycleField = document.querySelector("#cycleAssigneeField");
  if (mode === "single") {
    singleField?.classList.remove("hidden");
  } else {
    singleField?.classList.add("hidden");
  }
  if (mode === "cycleSelect") {
    cycleField?.classList.remove("hidden");
  } else {
    cycleField?.classList.add("hidden");
  }
}

function updateRecurrenceFields() {
  const type = document.querySelector("#recurrenceType")?.value || "intervalDays";
  document.querySelectorAll("[data-recurrence-field]").forEach((field) => {
    field.hidden = field.dataset.recurrenceField !== type;
  });
}

function weekdayLabel(day) {
  return ["日", "一", "二", "三", "四", "五", "六"][day] || "日";
}

function clamp(value, min, max) {
  if (Number.isNaN(value)) return min;
  return Math.min(max, Math.max(min, value));
}

function isDueToday(task) {
  if (task.enabled === false) {
    return false;
  }
  const now = new Date();
  if (task.recurrence.type === "intervalDays") {
    const anchor = new Date(`${task.recurrence.anchorDate}T00:00:00`);
    const diff = Math.floor((startOfDay(now) - anchor) / 86400000);
    return diff >= 0 && diff % task.recurrence.every === 0;
  }
  if (task.recurrence.type === "weekly") {
    return task.recurrence.daysOfWeek.includes(now.getDay());
  }
  if (task.recurrence.type === "monthlyDate") {
    return task.recurrence.day === now.getDate();
  }
  return false;
}

function startOfDay(date) {
  return new Date(date.getFullYear(), date.getMonth(), date.getDate());
}

function completionKey(taskId) {
  return `${taskId}_${todayKey}`;
}

async function toggleTask(taskId, completed) {
  await store.setCompletion(taskId, todayKey, !completed, isDevicePage ? "device-h5" : "app-h5");
  render();
}

function state() {
  return store.getState();
}

function todaysTasks() {
  return state().tasks.filter(isDueToday);
}

function render() {
  renderSyncMode();
  renderAssigneeOptions();
  renderSummary();
  renderToday();
  renderTasks();
  renderMembers();
  renderDevice();
  bindTaskButtons();
}

function renderSyncMode() {
  document.querySelectorAll("[data-sync-mode]").forEach((item) => {
    item.textContent = store.mode === "lark" ? "飞书多维表格已连接" : "本地模式";
  });
  document.querySelectorAll("[data-home-id]").forEach((item) => {
    item.textContent = getHomeId();
  });
}

function renderAssigneeOptions() {
  const select = document.querySelector("#assigneeSelect");
  const cycleContainer = document.querySelector("#cycleMembersContainer");
  const members = state().members;
  if (select) {
    const previous = select.value;
    const memberOptions = members
      .map((member) => `<option value="${member.id}">${escapeHtml(member.name)}</option>`)
      .join("");
    select.innerHTML = memberOptions;
    if (members.some((member) => member.id === previous)) {
      select.value = previous;
    }
  }
  if (cycleContainer) {
    const selected = new Set(
      [...cycleContainer.querySelectorAll("input[name='cycleMemberId']:checked")].map((input) => input.value)
    );
    cycleContainer.innerHTML = members
      .map((member) => `
        <label class="checkbox-label">
          <input type="checkbox" name="cycleMemberId" value="${member.id}" ${selected.size === 0 || selected.has(member.id) ? "checked" : ""}>
          <span class="dot" style="--accent:${member.color}"></span>${escapeHtml(member.name)}
        </label>
      `)
      .join("");
  }
  updateAssigneeModeFields();
}

function renderSummary() {
  const container = document.querySelector("#summaryStrip");
  if (!container) return;
  const due = todaysTasks();
  const done = due.filter((task) => state().completions[completionKey(task.id)]).length;
  container.innerHTML = [metric(due.length, "今日事项"), metric(done, "已完成"), metric(due.length - done, "待处理")].join("");
}

function metric(value, label) {
  return `<div class="metric"><strong>${value}</strong><span>${label}</span></div>`;
}

function renderToday() {
  const container = document.querySelector("#todayList");
  if (!container) return;
  const due = todaysTasks();
  container.innerHTML = state()
    .members.map((member) => {
      const tasks = due.filter((task) => task.assigneeId === member.id);
      return memberSection(member, tasks);
    })
    .join("");
}

function memberSection(member, tasks) {
  const count = tasks.filter((task) => !state().completions[completionKey(task.id)]).length;
  return `
    <section class="member-section">
      <div class="member-head">
        <div class="member-name"><span class="dot" style="--accent:${member.color}"></span>${escapeHtml(member.name)}</div>
        <span class="task-meta">${count} 个待完成</span>
      </div>
      ${tasks.length ? tasks.map(taskButton).join("") : `<p class="task-meta">今天没有安排</p>`}
    </section>
  `;
}

function taskButton(task) {
  const done = Boolean(state().completions[completionKey(task.id)]);
  const isStrong = !done && (task.missedCount >= 2 || task.overdueType === "today_strong");
  const isFront = !done && task.overdueType === "pending_front";
  
  let badges = "";
  if (isStrong) {
    badges += `<span class="strong-badge">🔥 连漏 ${task.missedCount || 1} 次 · 强提醒</span>`;
  }
  if (isFront) {
    badges += `<span class="pending-front-badge">⏳ 上次未做 (${task.lastReminderDate || "前次"}) · 前置</span>`;
  }
  if (task.cycleInfo?.enabled) {
    badges += `<span class="cycle-badge">${cycleSummary(task)}</span>`;
  }

  return `
    <button class="task-row ${done ? "done" : ""} ${isStrong ? "strong-alert" : ""} ${isFront ? "overdue-front" : ""}" type="button" data-completion-task-id="${task.id}" data-completed="${done}">
      <span class="check">✓</span>
      <span>
        <strong class="task-title">${escapeHtml(task.title)}</strong>${badges}<br>
        <span class="task-meta">${escapeHtml(task.label)}</span>
      </span>
      <span class="task-meta">${done ? "恢复" : "完成"}</span>
    </button>
  `;
}

function renderTasks() {
  const container = document.querySelector("#allTasks");
  if (!container) return;
  container.innerHTML = state()
    .tasks.map((task) => {
      const member = state().members.find((item) => item.id === task.assigneeId) || state().members[0];
      let metaText = `${escapeHtml(member?.name || "未分配")} · ${escapeHtml(task.label)}`;
      if (task.cycleInfo?.enabled) {
        metaText = `${cycleSummary(task)} · ${escapeHtml(task.label)}`;
      }
      return `
        <section class="member-section">
          <div class="member-head">
            <div class="member-name"><span class="dot" style="--accent:${member?.color || "#ef7f65"}"></span>${escapeHtml(task.title)}</div>
            <span class="task-meta">${task.enabled === false ? "已停用" : "启用中"}</span>
          </div>
          <span class="task-meta">${metaText}</span>
          <div class="row-actions">
            <button class="secondary-button" type="button" data-task-action="toggle-enabled" data-task-id="${task.id}">
              ${task.enabled === false ? "启用" : "停用"}
            </button>
            <button class="danger-button" type="button" data-task-action="delete" data-task-id="${task.id}">删除</button>
          </div>
        </section>
      `;
    })
    .join("");
  bindTaskManagementButtons();
}

function cycleSummary(task) {
  const names = task.cycleInfo.options.map((id) => state().members.find((m) => m.id === id)?.name || id);
  const current = state().members.find((m) => m.id === task.assigneeId)?.name || task.assigneeId;
  const next = state().members.find((m) => m.id === task.cycleInfo.nextAssigneeId)?.name || task.cycleInfo.nextAssigneeId;
  return `轮流 ${names.join(" → ")} · 本次 ${current} · 下次 ${next}`;
}

function renderMembers() {
  const container = document.querySelector("#memberManager");
  if (!container) return;
  container.innerHTML = state()
    .members.map((member) => {
      const taskCount = state().tasks.filter((task) => task.assigneeId === member.id).length;
      return `
        <section class="member-section">
          <div class="member-head">
            <div class="member-name"><span class="dot" style="--accent:${member.color}"></span>${escapeHtml(member.name)}</div>
            <span class="task-meta">${taskCount} 个任务</span>
          </div>
          <label class="inline-field">
            <span>标签颜色</span>
            <input type="color" value="${escapeHtml(member.color)}" data-member-action="color" data-member-id="${member.id}">
          </label>
          <div class="row-actions">
            <button class="danger-button" type="button" data-member-action="delete" data-member-id="${member.id}" ${taskCount ? "disabled" : ""}>
              ${taskCount ? "有任务不可删" : "删除"}
            </button>
          </div>
        </section>
      `;
    })
    .join("");
  bindMemberManagementButtons();
}

function renderDevice() {
  if (isDevicePage) {
    renderDeviceScreen();
    return;
  }
  renderDeviceManager();
}

function renderDeviceScreen() {
  const screen = document.querySelector("#deviceScreen");
  if (!screen) return;
  const due = todaysTasks();
  const remaining = due.filter((task) => !state().completions[completionKey(task.id)]).length;
  screen.innerHTML = `
    <div class="device-head">
      <strong>今日</strong>
      <span>${todayKey}</span>
    </div>
    <div class="device-grid">
      ${state()
        .members.map((member) => devicePerson(member, due.filter((task) => task.assigneeId === member.id)))
        .join("")}
    </div>
    <div class="device-foot">
      <span>玄关屏 · <span data-sync-mode>${store.mode === "lark" ? "飞书多维表格已连接" : "本地模式"}</span></span>
      <strong>${remaining} 未完成</strong>
    </div>
  `;
}

function renderDeviceManager() {
  const container = document.querySelector("#deviceManager");
  if (!container) return;
  const devices = state().devices || [];
  if (!devices.length) {
    container.innerHTML = `<section class="member-section"><p class="task-meta">还没有接入设备。添加设备 ID 后，设备访问对应地址就会显示在线状态。</p></section>`;
    return;
  }
  container.innerHTML = devices.map(deviceCard).join("");
  bindDeviceManagementButtons();
}

function deviceCard(device) {
  const online = isDeviceOnline(device);
  const deviceUrl = `${location.origin}${location.pathname.replace(/index\.html$/, "")}device.html?home=${encodeURIComponent(getHomeId())}&device=${encodeURIComponent(device.id)}`;
  return `
    <section class="member-section device-card">
      <div class="member-head">
        <div class="member-name">
          <span class="status-dot ${online ? "online" : "offline"}"></span>
          ${escapeHtml(device.name)}
        </div>
        <span class="status-pill ${online ? "online" : "offline"}">${online ? "在线" : "离线"}</span>
      </div>
      <label class="device-name-field">
        <span>设备名称</span>
        <input value="${escapeHtml(device.name)}" data-device-action="rename" data-device-id="${escapeHtml(device.id)}">
      </label>
      <div class="device-meta">ID: ${escapeHtml(device.id)}</div>
      <div class="device-meta">位置: ${escapeHtml(device.location || "未设置")}</div>
      <div class="device-meta">型号: ${escapeHtml(device.model || "未设置")}</div>
      <div class="device-meta">最后在线: ${escapeHtml(formatLastSeen(device.lastSeenAt))}</div>
      <label class="device-url compact">
        <span>设备访问地址</span>
        <input readonly value="${escapeHtml(deviceUrl)}">
      </label>
      <div class="row-actions">
        <button class="danger-button" type="button" data-device-action="delete" data-device-id="${escapeHtml(device.id)}">删除</button>
      </div>
    </section>
  `;
}

function isDeviceOnline(device) {
  if (!device.lastSeenAt) {
    return device.status === "online";
  }
  const lastSeen = new Date(device.lastSeenAt).getTime();
  return Number.isFinite(lastSeen) && Date.now() - lastSeen < 2 * 60 * 1000;
}

function formatLastSeen(value) {
  if (!value) return "从未连接";
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return "未知";
  const diffMinutes = Math.floor((Date.now() - date.getTime()) / 60000);
  if (diffMinutes < 1) return "刚刚";
  if (diffMinutes < 60) return `${diffMinutes} 分钟前`;
  return date.toLocaleString("zh-CN", { hour12: false });
}

function devicePerson(member, tasks) {
  return `
    <section class="device-person">
      <div class="member-name"><span class="dot" style="--accent:${member.color}"></span>${escapeHtml(member.name)}</div>
      ${tasks.length ? tasks.map(deviceTask).join("") : `<p class="task-meta">无</p>`}
    </section>
  `;
}

function deviceTask(task) {
  const done = Boolean(state().completions[completionKey(task.id)]);
  const isStrong = !done && (task.missedCount >= 2 || task.overdueType === "today_strong");
  const isFront = !done && task.overdueType === "pending_front";
  let tag = "";
  if (isStrong) tag = ` <span class="strong-badge">🚨连漏${task.missedCount || 1}次</span>`;
  else if (isFront) tag = ` <span class="pending-front-badge">⏳前置</span>`;
  return `<button class="device-task ${done ? "done" : ""} ${isStrong ? "strong-alert" : ""}" type="button" data-completion-task-id="${task.id}" data-completed="${done}"><span>${escapeHtml(task.title)}${tag}</span><span>${done ? "恢复" : "完成"}</span></button>`;
}

function bindTaskButtons() {
  document.querySelectorAll("[data-completion-task-id]").forEach((button) => {
    button.addEventListener(
      "click",
      () => toggleTask(button.dataset.completionTaskId, button.dataset.completed === "true"),
      { once: true }
    );
  });
}

function bindTaskManagementButtons() {
  document.querySelectorAll("[data-task-action]").forEach((button) => {
    button.addEventListener("click", async () => {
      const taskId = button.dataset.taskId;
      const task = state().tasks.find((item) => item.id === taskId);
      if (!task) return;
      if (button.dataset.taskAction === "delete") {
        await store.deleteTask(taskId);
      } else {
        await store.updateTask(taskId, { enabled: task.enabled === false });
      }
      render();
    });
  });
}

function bindMemberManagementButtons() {
  document.querySelectorAll("[data-member-action='color']").forEach((input) => {
    input.addEventListener("change", async () => {
      await store.updateMember(input.dataset.memberId, { color: input.value });
      render();
    });
  });

  document.querySelectorAll("[data-member-action='delete']").forEach((button) => {
    button.addEventListener("click", async () => {
      if (button.disabled) return;
      await store.deleteMember(button.dataset.memberId);
      render();
    });
  });
}

function bindDeviceManagementButtons() {
  document.querySelectorAll("[data-device-action='rename']").forEach((input) => {
    input.addEventListener("change", async () => {
      await store.updateDevice(input.dataset.deviceId, { name: input.value.trim() || input.dataset.deviceId });
      render();
    });
  });

  document.querySelectorAll("[data-device-action='delete']").forEach((button) => {
    button.addEventListener("click", async () => {
      await store.deleteDevice(button.dataset.deviceId);
      render();
    });
  });
}

function startDeviceHeartbeat() {
  if (!isDevicePage || !currentDeviceId) return;
  const heartbeat = async () => {
    const patch = {
      status: "online",
      lastSeenAt: new Date().toISOString()
    };
    const existing = state().devices?.find((device) => device.id === currentDeviceId);
    if (existing) {
      await store.updateDevice(currentDeviceId, patch);
      return;
    }
    await store.addDevice({
      id: currentDeviceId,
      name: currentDeviceId,
      location: "",
      model: "ESP32-S3-Touch-LCD-4.3",
      ...patch
    });
  };
  heartbeat();
  setInterval(heartbeat, 60000);
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}
