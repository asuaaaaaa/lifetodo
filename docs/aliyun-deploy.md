# 阿里云部署清单

域名：

```text
lifetodo.xyz
```

## 推荐部署结构

当前推荐使用 GitHub Actions 部署到阿里云轻量应用服务器/Nginx。OSS + CDN 仍可作为后续更低成本的静态资源方案。

服务器公网 IP：

```text
120.55.46.251
```

正式 URL：

```text
https://lifetodo.xyz/app/
https://lifetodo.xyz/device/?home=demo-home&device=entry
```

## GitHub 自动部署

仓库已提供：

```text
.github/workflows/deploy-aliyun.yml
```

配置方式见：

```text
docs/github-aliyun-deploy.md
```

推送到 GitHub `main` 分支后，GitHub Actions 会自动生成 `dist/site` 并上传到轻量应用服务器。

## 本地生成静态目录

```bash
node scripts/prepare-static-deploy.mjs
```

生成：

```text
dist/site/app/
dist/site/device/
dist/site/design-system/
```

把 `dist/site` 作为站点根目录上传即可。

## Nginx 示例

```nginx
server {
  listen 443 ssl http2;
  server_name lifetodo.xyz;

  root /var/www/lifetodo/dist/site;
  index index.html;

  location /app/ {
    try_files $uri $uri/ /app/index.html;
  }

  location /device/ {
    try_files $uri $uri/ /device/index.html;
  }

  location /api/ {
    proxy_pass http://127.0.0.1:8787/;
  }
}
```

## Firebase 状态

- Anonymous 匿名登录：已开启。
- Authorized domains：已添加 `lifetodo.xyz`。

部署到 `lifetodo.xyz` 后，Firebase Auth 应可正常匿名登录。若页面显示 `本地模式`，优先检查 Firestore Rules、浏览器控制台报错和域名 HTTPS。

## APK WebView 地址

```text
https://lifetodo.xyz/app/?home=demo-home
```

## ESP 设备地址

开发期：

```text
https://lifetodo.xyz/device/?home=demo-home&device=entry
```

正式版：

```text
https://lifetodo.xyz/device/?home={homeId}&device={deviceId}&token={shortLivedToken}
```
