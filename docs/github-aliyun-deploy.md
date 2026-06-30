# GitHub Actions 部署到阿里云

目标：推送到 GitHub `main` 分支后，自动生成 H5 产物并部署到阿里云 ECS 的 Nginx 站点目录。

## 1. 阿里云 ECS 准备

在 ECS 上准备站点目录：

```bash
sudo mkdir -p /var/www/lifetodo/dist/site
sudo chown -R <deploy-user>:<deploy-user> /var/www/lifetodo
```

`<deploy-user>` 建议是专门部署用户，不建议直接用 `root`。

Nginx 站点示例：

```nginx
server {
  listen 80;
  server_name lifetodo.xyz;

  root /var/www/lifetodo/dist/site;
  index index.html;

  location /app/ {
    try_files $uri $uri/ /app/index.html;
  }

  location /device/ {
    try_files $uri $uri/ /device/index.html;
  }
}
```

配置 HTTPS 后，把 `listen 80` 升级到 443，或让 80 跳转到 HTTPS。

## 2. GitHub Secrets

进入 GitHub 仓库：

```text
Settings -> Secrets and variables -> Actions -> New repository secret
```

添加：

```text
ALIYUN_HOST          ECS 公网 IP 或域名
ALIYUN_PORT          SSH 端口，通常是 22
ALIYUN_USER          部署用户
ALIYUN_SSH_KEY       部署用户的私钥内容
ALIYUN_DEPLOY_PATH   /var/www/lifetodo/dist/site
```

`ALIYUN_PORT` 和 `ALIYUN_DEPLOY_PATH` 有默认值，但建议显式配置。

## 3. SSH Key 建议

在本机或 ECS 上生成一把专门用于部署的 key：

```bash
ssh-keygen -t ed25519 -C "github-actions-lifetodo"
```

把公钥加入 ECS 部署用户的：

```text
~/.ssh/authorized_keys
```

把私钥完整内容填入 GitHub Secret：

```text
ALIYUN_SSH_KEY
```

## 4. 触发部署

把代码推到 GitHub `main` 分支：

```bash
git push origin main
```

或在 GitHub Actions 页面手动运行：

```text
Deploy to Aliyun -> Run workflow
```

## 5. 验证

部署成功后访问：

```text
https://lifetodo.xyz/app/?home=demo-home
https://lifetodo.xyz/device/?home=demo-home&device=entry
```

页面显示 `Firebase 已连接`，并且手机页完成任务后设备页同步变化，就说明链路成功。
