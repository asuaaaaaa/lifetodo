const todayKey = new Date().toISOString().slice(0, 10);

export const defaultSeed = {
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
      label: "每 3 天",
      enabled: true
    },
    {
      id: "toilet",
      title: "清洁马桶",
      assigneeId: "xingyue",
      recurrence: { type: "monthlyDate", day: new Date().getDate() },
      label: `每月 ${new Date().getDate()} 日`,
      enabled: true
    },
    {
      id: "plants",
      title: "给阳台植物浇水",
      assigneeId: "piggy",
      recurrence: { type: "intervalDays", every: 2, anchorDate: todayKey },
      label: "每 2 天",
      enabled: true
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
