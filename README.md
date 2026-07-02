# LifeTodo

家庭周期任务 + 多屏提醒系统的第一版原型。

目标是把家里的周期性琐事分配给家庭成员，并同步到多个 ESP32-S3 触摸屏设备上。前后端采用 H5 Web App，手机端通过 H5 包装成 APK 安装使用；ESP 设备端通过家庭 Wi-Fi 直接访问专用设备链接，每天展示今天每个人要做的事，并支持轻触完成。

## 当前内容

- `apps/pwa-prototype/`: 零依赖 H5 原型，包含手机管理视图和设备屏视图
- `docs/product-requirements.md`: 产品需求和阶段规划
- `docs/architecture.md`: 飞书多维表格、LifeTodo API、ESP32 的系统架构
- `docs/lark-base-schema.md`: 飞书多维表格数据模型
- `docs/lark-base-setup.md`: 飞书多维表格接入和运行说明
- `docs/esp32-s3-touch-lcd-4.3.md`: ESP32-S3-Touch-LCD-4.3 设备端方案
- `docs/apk-packaging.md`: H5 包装 APK 的建议路线
- `docs/aliyun-deploy.md`: `lifetodo.xyz` 阿里云部署清单
- `docs/github-aliyun-deploy.md`: GitHub Actions 自动部署到阿里云轻量应用服务器
- `docs/project-status.md`: 当前完成项和下一步
- `config/app-config.json`: 当前域名和入口 URL 配置
- `scripts/prepare-static-deploy.mjs`: 生成静态部署目录
- `design-system/`: 从 `recent.design` 抽取的 starter tokens

## 本地预览

手机管理视图：

```bash
open apps/pwa-prototype/index.html
```

设备屏视图：

```bash
open apps/pwa-prototype/device.html
```

或者用任意静态服务器指向 `apps/pwa-prototype/`。

## 部署

当前域名配置为 `lifetodo.xyz`。生成静态站点目录：

```bash
node scripts/prepare-static-deploy.mjs
```

然后把 `dist/site` 上传到阿里云 OSS/CDN 或 Nginx 站点根目录。详见 `docs/aliyun-deploy.md`。

## 飞书多维表格

原型通过本项目的 Node API 访问飞书多维表格。成功时页面显示 `飞书多维表格已连接`，失败时回落到 `localStorage` 并显示 `本地模式`。

运行前需要用 `lark-cli` 创建/配置 Base，并设置 `LIFETODO_LARK_BASE_TOKEN` 和 `LIFETODO_LARK_TODO_TABLE_ID`。详见 `docs/lark-base-setup.md`。

## 设计记录

已安装并运行 `extract-design-system` skill，对 `https://recent.design/` 做了 v1 抽取。结果稳定识别到 `Inter` 字体和少量间距 token，没有可靠色板，因此本原型只借鉴其干净、克制、信息密度适中的表达方式，不把抽取结果当成完整设计系统。
