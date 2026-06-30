# Firestore 数据模型草案

## 当前原型模型

为了最快验证手机 H5 和设备 H5 的同步，当前代码先使用一个紧凑文档：

```text
homes/demo-home
```

字段：

```json
{
  "name": "家",
  "timezone": "Asia/Shanghai",
  "members": [],
  "tasks": [],
  "completions": {},
  "updatedAt": "serverTimestamp",
  "updatedBy": "anonymous uid",
  "updatedSource": "app-h5|device-h5|seed"
}
```

这个模型适合原型，不适合长期多人并发。正式版建议迁移到下面的子集合模型。

## `homes/{homeId}`

```json
{
  "name": "家",
  "timezone": "Asia/Shanghai",
  "createdBy": "uid",
  "createdAt": "timestamp"
}
```

## `homes/{homeId}/members/{memberId}`

```json
{
  "displayName": "妈妈",
  "avatarColor": "#ffb35c",
  "userId": "uid-or-null",
  "role": "owner|adult|child",
  "active": true
}
```

## `homes/{homeId}/tasks/{taskId}`

```json
{
  "title": "铲猫砂盆",
  "notes": "",
  "assigneeIds": ["memberId"],
  "recurrence": {
    "type": "intervalDays",
    "every": 3,
    "anchorDate": "2026-06-30"
  },
  "preferredTime": "morning",
  "enabled": true,
  "createdAt": "timestamp",
  "updatedAt": "timestamp"
}
```

## recurrence 类型

- `intervalDays`: 每 N 天，基于 `anchorDate`。
- `weekly`: 每周固定星期几，例如 `daysOfWeek: [0, 3]`。
- `monthlyDate`: 每月固定日期，例如 `day: 1`。
- `monthlyWeekday`: 每月第几个星期几，例如第二个周六。

## `homes/{homeId}/taskCompletions/{completionId}`

`completionId` 建议使用 `${taskId}_${date}`，多人任务可扩展为 `${taskId}_${memberId}_${date}`。

```json
{
  "taskId": "taskId",
  "memberId": "memberId",
  "date": "2026-06-30",
  "completedBy": "uid-or-deviceId",
  "completedAt": "timestamp",
  "source": "app|device|voice"
}
```

## `homes/{homeId}/devices/{deviceId}`

```json
{
  "name": "玄关屏",
  "model": "esp32-s3-touch-lcd-4.3",
  "status": "active",
  "lastSeenAt": "timestamp",
  "firmwareVersion": "0.1.0",
  "screenProfile": "entry|bedroom|kitchen"
}
```
