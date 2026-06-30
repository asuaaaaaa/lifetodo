# H5 包装 APK 路线

## 推荐方式

第一版建议用 Android WebView 壳包装阿里云上的 H5 地址：

```text
https://lifetodo.xyz/app/
```

壳 App 只负责：

- 加载固定 H5 地址。
- 保持登录态和本地缓存。
- 处理状态栏、安全区、返回键。
- 后续如有需要，桥接本地通知。

## 可选工具

- Android Studio 原生 WebView 壳：控制力最强。
- Capacitor：适合以后接本地通知、扫码、蓝牙、文件等能力。
- HBuilderX/uni-app WebView 壳：上手快，但长期可控性略弱。

## 与 Firebase 的关系

Firebase SDK 运行在 H5 内，APK 只是容器。用户登录、任务读写、设备绑定仍然由 H5 和后端完成。

## 设备页

ESP 屏幕不需要 APK，直接访问设备入口：

```text
https://lifetodo.xyz/device/?home={homeId}&device={deviceId}&token=...
```

正式版不要长期暴露明文 token，建议设备定期向阿里云 API 换取短期访问凭证。
