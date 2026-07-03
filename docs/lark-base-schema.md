# 飞书多维表格数据模型

当前版本使用“每个家庭一张 TODO 表 + 一张完成状态表”的结构化模型，不再把整个家庭状态塞进一条 JSON 快照记录。

## Base

- 名称：`LifeTodo`
- Base Token：`L6kdbumDKa8QFosicOIcItBjncb`
- URL：https://zcnwtgslngt4.feishu.cn/base/L6kdbumDKa8QFosicOIcItBjncb

## 家庭 TODO 表：demo-home TODOs

- Table ID：`tblrbHrBuoijFiXK`
- 语义：`demo-home` 家庭的 TODO 表；每条记录是一条 TODO。

字段：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `Title` | text | TODO 内容 |
| `Task ID` | text | 应用内稳定 ID，用于 upsert |
| `Assignee ID` | text | 负责人 ID |
| `Assignee Name` | text | 负责人显示名 |
| `Assignee Color` | text | 负责人颜色 |
| `Enabled` | checkbox | 是否启用 |
| `Recurrence Type` | select | `intervalDays`、`weekly`、`monthlyDate` |
| `Interval Every` | number | 每 N 天循环的 N |
| `Anchor Date` | text | 每 N 天循环的锚点日期 |
| `Weekdays` | text | 每周循环的星期，逗号分隔，0 表示周日 |
| `Monthly Day` | number | 每月固定日期 |
| `Next Date` | text | 下次应做日期，由 API 写入 |
| `Label` | text | 页面展示用循环描述 |
| `Last Completed Date` | text | 最近完成日期 |
| `Last Completed At` | datetime | 最近完成时间 |
| `Last Completion Source` | text | 最近完成来源，例如 `app-h5`、`device-esp32` |

`Last Completed *` 字段是派生缓存，用于快速计算下次日期；真正的每日完成状态以完成状态表为准。

## 家庭完成状态表：demo-home Completions

- Table ID：`tbluon5aF9IrHMIv`
- 环境变量：`LIFETODO_LARK_COMPLETION_TABLE_ID=tbluon5aF9IrHMIv`
- 语义：每条记录是一条任务在某一天的完成状态，网页端和 ESP 设备端都通过 API 同步到这里。

字段：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `Completion ID` | text | 稳定 ID，格式为 `${Task ID}_${Date}` |
| `Task ID` | text | 对应 TODO 的 `Task ID` |
| `Date` | text | 完成状态对应日期，格式 `YYYY-MM-DD` |
| `Completed` | checkbox | 是否完成；取消完成时 API 会删除对应记录 |
| `Completed At` | datetime | 完成时间 |
| `Source` | text | 完成来源，例如 `app-h5`、`device-esp32` |

## 旧表

`Snapshots` / `tblCkosf9Q7TKlXu` 是第一轮迁移时创建的快照表，已经不作为运行时数据源。保留它只是为了回滚和参考。
