import { createStore } from "./firebase-store.js";

const todayKey = new Date().toISOString().slice(0, 10);
const isDevicePage = document.body.classList.contains("device-only");

const seed = {
  members: [
    { id: "mom", name: "妈妈", color: "#ef7f65" },
    { id: "dad", name: "爸爸", color: "#5d8fb4" },
    { id: "kid", name: "小朋友", color: "#6ba36f" }
  ],
  tasks: [
    {
      id: "litter",
      title: "铲猫砂盆",
      assigneeId: "mom",
      recurrence: { type: "intervalDays", every: 3, anchorDate: todayKey },
      label: "每 3 天"
    },
    {
      id: "toilet",
      title: "清洁马桶",
      assigneeId: "dad",
      recurrence: { type: "monthlyDate", day: new Date().getDate() },
      label: "每月 1 日"
    },
    {
      id: "plants",
      title: "给阳台植物浇水",
      assigneeId: "kid",
      recurrence: { type: "intervalDays", every: 2, anchorDate: todayKey },
      label: "每 2 天"
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
  bindEvents();
  updateRecurrenceFields();
  render();
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
    await store.addTask({
      id: crypto.randomUUID(),
      title: String(form.get("title")).trim(),
      assigneeId: form.get("assigneeId"),
      recurrence: recurrence.value,
      label: recurrence.label,
      enabled: true
    });
    event.currentTarget.reset();
    updateRecurrenceFields();
    render();
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
    item.textContent = store.mode === "firebase" ? "Firebase 已连接" : "本地模式";
  });
}

function renderAssigneeOptions() {
  const select = document.querySelector("#assigneeSelect");
  if (!select) return;
  select.innerHTML = state()
    .members.map((member) => `<option value="${member.id}">${escapeHtml(member.name)}</option>`)
    .join("");
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
  return `
    <button class="task-row ${done ? "done" : ""}" type="button" data-completion-task-id="${task.id}" data-completed="${done}">
      <span class="check">✓</span>
      <span><strong class="task-title">${escapeHtml(task.title)}</strong><br><span class="task-meta">${escapeHtml(task.label)}</span></span>
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
      return `
        <section class="member-section">
          <div class="member-head">
            <div class="member-name"><span class="dot" style="--accent:${member?.color || "#ef7f65"}"></span>${escapeHtml(task.title)}</div>
            <span class="task-meta">${task.enabled === false ? "已停用" : "启用中"}</span>
          </div>
          <span class="task-meta">${escapeHtml(member?.name || "未分配")} · ${escapeHtml(task.label)}</span>
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
      <span>玄关屏 · <span data-sync-mode>${store.mode === "firebase" ? "Firebase 已连接" : "本地模式"}</span></span>
      <strong>${remaining} 未完成</strong>
    </div>
  `;
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
  return `<button class="device-task ${done ? "done" : ""}" type="button" data-completion-task-id="${task.id}" data-completed="${done}"><span>${escapeHtml(task.title)}</span><span>${done ? "恢复" : "完成"}</span></button>`;
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
  document.querySelectorAll("[data-member-action='delete']").forEach((button) => {
    button.addEventListener("click", async () => {
      if (button.disabled) return;
      await store.deleteMember(button.dataset.memberId);
      render();
    });
  });
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}
