# 飞书多维表格接入说明

LifeTodo 现在通过本项目的 Node API 访问飞书多维表格，网页端和 ESP32 都不直接持有飞书凭证。

## 已创建资源

- Base：`LifeTodo`
- Base URL：https://zcnwtgslngt4.feishu.cn/base/L6kdbumDKa8QFosicOIcItBjncb
- Base Token：`L6kdbumDKa8QFosicOIcItBjncb`
- TODO Table：`demo-home TODOs`
- TODO Table ID：`tblrbHrBuoijFiXK`

## 本地运行

```bash
export LIFETODO_LARK_BASE_TOKEN=L6kdbumDKa8QFosicOIcItBjncb
export LIFETODO_LARK_TODO_TABLE_ID=tblrbHrBuoijFiXK
export LIFETODO_HOME_ID=demo-home
npm start
```

访问：

```text
http://localhost:8787/
http://localhost:8787/device.html?home=demo-home&device=entry-screen
```

线上使用时，网页端和 ESP32 都访问：

```text
https://lifetodo.xyz/
https://lifetodo.xyz/device/?home=demo-home&device=entry-screen
```

## API

- `GET /api/state?home=demo-home`：读取 `demo-home TODOs` 表并还原为前端状态。
- `PUT /api/state?home=demo-home`：网页端保存 TODO 变更，按 `Task ID` upsert 每条记录。
- `POST /api/completions?home=demo-home`：网页或 ESP32 提交单条完成状态，更新对应 TODO 行的完成字段。
- `POST /api/devices/heartbeat?home=demo-home`：设备在线心跳预留接口。

## ESP32

固件只需要配置：

```c
#define LIFETODO_HOME_ID "demo-home"
#define LIFETODO_DEVICE_ID "entry-screen"
#define LIFETODO_BASE_URL "https://lifetodo.xyz"
```

设备同步走 `GET /api/state`，点击完成走 `POST /api/completions`。

ESP32 不使用 `localhost`。开发和正式使用都可以直接连 `https://lifetodo.xyz`，前提是线上服务已经部署本项目的 Node API，并设置：

```bash
LIFETODO_LARK_BASE_TOKEN=L6kdbumDKa8QFosicOIcItBjncb
LIFETODO_LARK_TODO_TABLE_ID=tblrbHrBuoijFiXK
LIFETODO_HOME_ID=demo-home
```
