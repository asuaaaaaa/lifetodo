# LifeTodo

家庭周期任务 + 多屏提醒系统的第一版原型。

目标是把家里的周期性琐事分配给家庭成员，并同步到多个 ESP32-S3 触摸屏设备上。前后端采用 H5 Web App，手机端通过 H5 包装成 APK 安装使用；ESP 设备端通过家庭 Wi-Fi 直接访问专用设备链接，每天展示今天每个人要做的事，并支持轻触完成。

## 当前内容

- `apps/pwa-prototype/`: 零依赖 H5 原型，包含手机管理视图和设备屏视图
- `docs/product-requirements.md`: 产品需求和阶段规划
- `docs/architecture.md`: Firebase、阿里云、ESP32 的系统架构
- `docs/firebase-schema.md`: Firestore 数据模型草案
- `docs/firebase-setup.md`: Firebase 接入和开发期规则
- `docs/esp32-s3-touch-lcd-4.3.md`: ESP32-S3-Touch-LCD-4.3 设备端方案
- `docs/apk-packaging.md`: H5 包装 APK 的建议路线
- `docs/aliyun-deploy.md`: `lifetodo.xyz` 阿里云部署清单
- `docs/github-aliyun-deploy.md`: GitHub Actions 自动部署到阿里云 ECS
- `docs/project-status.md`: 当前完成项和下一步
- `config/app-config.json`: 当前域名和入口 URL 配置
- `scripts/prepare-static-deploy.mjs`: 生成静态部署目录
- `infra/firestore.rules`: 开发期 Firestore Rules
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

## Firebase

原型会自动尝试连接 Firebase。成功时页面显示 `Firebase 已连接`，失败时回落到 `localStorage` 并显示 `本地模式`。

需要在 Firebase Console 开启 Anonymous Auth，并参考 `docs/firebase-setup.md` 设置开发期 Firestore Rules。

## 设计记录

已安装并运行 `extract-design-system` skill，对 `https://recent.design/` 做了 v1 抽取。结果稳定识别到 `Inter` 字体和少量间距 token，没有可靠色板，因此本原型只借鉴其干净、克制、信息密度适中的表达方式，不把抽取结果当成完整设计系统。
