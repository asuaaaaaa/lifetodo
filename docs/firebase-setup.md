# Firebase 接入说明

当前 H5 原型已接入你的 Firebase Web 配置。

## 已使用的 Firebase 能力

- Firebase Auth: 匿名登录。
- Firestore: 保存一个演示家庭空间 `homes/demo-home`。
- Analytics: 暂未启用。H5 包 APK、`file://` 本地预览和 ESP 设备页都不适合在第一版强依赖 Analytics。

## 本地和部署行为

应用启动时会尝试加载 Firebase CDN SDK：

- 成功：状态显示 `Firebase 已连接`，任务和完成状态写入 Firestore。
- 失败：自动回落到 `localStorage`，状态显示 `本地模式`。

手机端可以访问：

```text
/index.html?home=demo-home
```

设备端可以访问：

```text
/device.html?home=demo-home
```

部署到正式域名后：

```text
https://lifetodo.xyz/app/?home=demo-home
https://lifetodo.xyz/device/?home=demo-home&device=entry
```

Firebase Console -> Authentication -> Settings -> Authorized domains 已添加：

```text
lifetodo.xyz
```

后续正式版建议把 `home` 换成真实家庭 ID，并用阿里云 API 签发设备专用 token，不让设备页面只靠 URL 参数访问数据。

## 开发期 Firestore Rules

先在 Firebase Console 开启 Anonymous Auth，然后用较宽松的开发规则验证同步。仓库里已经放了同样内容的 `infra/firestore.rules`：

```js
rules_version = '2';
service cloud.firestore {
  match /databases/{database}/documents {
    match /homes/{homeId} {
      allow read, write: if request.auth != null;
    }
  }
}
```

这只是开发期规则。正式版应改成：

- 用户只能访问自己所属的 `homeId`。
- 设备不能直接使用 Firebase 用户权限。
- 设备完成任务走阿里云 API，由后端校验设备 token 后写入 Firestore。
