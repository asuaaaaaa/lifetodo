# 当前状态

更新日期：2026-06-30

## 已完成

- Firebase Anonymous 匿名登录已开启。
- Firebase Authorized domains 已添加 `lifetodo.xyz`。
- H5 原型已接入 Firebase Web SDK。
- 原型支持 Firebase 连接失败时回落到 `localStorage`。
- 已配置正式域名入口：
  - `https://lifetodo.xyz/app/?home=demo-home`
  - `https://lifetodo.xyz/device/?home=demo-home&device=entry`
- 已提供静态部署脚本 `scripts/prepare-static-deploy.mjs`。
- 阿里云轻量应用服务器公网 IP：`120.55.46.251`。

## 下一步

- 在 Firebase Console 应用开发期 Firestore Rules。
- 运行 `node scripts/prepare-static-deploy.mjs` 生成 `dist/site`。
- 将 `dist/site` 通过 GitHub Actions 部署到阿里云轻量应用服务器 Nginx。
- 打开 `https://lifetodo.xyz/app/?home=demo-home` 验证页面显示 `Firebase 已连接`。
- 用另一端打开 `https://lifetodo.xyz/device/?home=demo-home&device=entry` 验证完成状态同步。
