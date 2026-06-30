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
  device: document.querySelector("#deviceView")
};

bootstrap();

async function bootstrap() {
  store = await createStore(seed);
  bindEvents();
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
    const recurrence = parseRecurrence(form.get("recurrence"));
    await store.addTask({
      id: crypto.randomUUID(),
      title: String(form.get("title")).trim(),
      assigneeId: form.get("assigneeId"),
      recurrence: recurrence.value,
      label: recurrence.label
    });
    event.currentTarget.reset();
    render();
  });
}

function parseRecurrence(value) {
  if (value === "interval-3") {
    return { label: "每 3 天", value: { type: "intervalDays", every: 3, anchorDate: todayKey } };
  }
  if (value === "weekly-0") {
    return { label: "每周日", value: { type: "weekly", daysOfWeek: [0] } };
  }
  return { label: "每月 1 日", value: { type: "monthlyDate", day: 1 } };
}

function isDueToday(task) {
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

async function toggleTask(taskId) {
  await store.toggleCompletion(taskId, todayKey, isDevicePage ? "device-h5" : "app-h5");
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
  renderDevice();
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
    .members.map((member) => `<option value="${member.id}">${member.name}</option>`)
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
  bindTaskButtons();
}

function memberSection(member, tasks) {
  const count = tasks.filter((task) => !state().completions[completionKey(task.id)]).length;
  return `
    <section class="member-section">
      <div class="member-head">
        <div class="member-name"><span class="dot" style="--accent:${member.color}"></span>${member.name}</div>
        <span class="task-meta">${count} 个待完成</span>
      </div>
      ${tasks.length ? tasks.map(taskButton).join("") : `<p class="task-meta">今天没有安排</p>`}
    </section>
  `;
}

function taskButton(task) {
  const done = Boolean(state().completions[completionKey(task.id)]);
  return `
    <button class="task-row ${done ? "done" : ""}" type="button" data-task-id="${task.id}">
      <span class="check">✓</span>
      <span><strong class="task-title">${task.title}</strong><br><span class="task-meta">${task.label}</span></span>
      <span class="task-meta">${done ? "完成" : "待做"}</span>
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
            <div class="member-name"><span class="dot" style="--accent:${member.color}"></span>${task.title}</div>
            <span class="task-meta">${task.label}</span>
          </div>
          <span class="task-meta">${member.name}</span>
        </section>
      `;
    })
    .join("");
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
  bindTaskButtons();
}

function devicePerson(member, tasks) {
  return `
    <section class="device-person">
      <div class="member-name"><span class="dot" style="--accent:${member.color}"></span>${member.name}</div>
      ${tasks.length ? tasks.map(deviceTask).join("") : `<p class="task-meta">无</p>`}
    </section>
  `;
}

function deviceTask(task) {
  const done = Boolean(state().completions[completionKey(task.id)]);
  return `<button class="device-task ${done ? "done" : ""}" type="button" data-task-id="${task.id}"><span>${task.title}</span><span>${done ? "✓" : "○"}</span></button>`;
}

function bindTaskButtons() {
  document.querySelectorAll("[data-task-id]").forEach((button) => {
    button.addEventListener("click", () => toggleTask(button.dataset.taskId), { once: true });
  });
}
