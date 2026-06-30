# ESP32-S3-Touch-LCD-4.3 设备端方案

## 固件职责

- 首次启动配网。
- 保存 Wi-Fi、`deviceId`、设备页面链接或 token。
- 打开阿里云托管的设备 H5 页面。
- 保持页面在线，必要时自动刷新。
- 提供触摸输入给 H5 页面。
- 离线时显示本地错误页或上次缓存页面。

## 推荐技术栈

- ESP-IDF 或 Arduino-ESP32。
- NVS 保存 Wi-Fi 与设备链接。
- 设备端优先走浏览器/WebView/图形库可承载的 H5 方案。
- 如果硬件或固件环境无法稳定跑 H5，再退回 LVGL + HTTPS API 的原生渲染方案。

## 屏幕交互

- 顶部：日期、设备名、同步状态。
- 主体：按成员分列或分组展示任务。
- 任务：大触摸区域，显示标题和完成状态。
- 底部：同步时间、未完成数量。

4.3 寸屏适合保持大字号和低层级，不在设备端提供任务编辑。设备 H5 页面应独立于手机管理界面，避免加载不必要的管理模块。

## 同步策略

- H5 页面每 1 到 5 分钟拉取一次今日任务。
- 用户完成任务后立即 POST。
- POST 成功后页面状态立即更新。
- POST 失败时在页面内保留待同步状态，下次联网重试。

## API 草案

```http
POST /devices/register
POST /devices/claim
GET /device/:deviceId
GET /devices/today
POST /devices/tasks/:taskId/complete
POST /devices/heartbeat
```

## 风险提示

ESP32-S3 直接跑完整现代浏览器能力有限。实际固件要验证该屏幕方案是否支持足够的 HTML/CSS/JS 渲染；如果不支持，应保留同一套接口，设备端改用 LVGL 原生渲染。
