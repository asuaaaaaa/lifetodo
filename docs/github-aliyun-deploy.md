# GitHub Actions 部署到阿里云

目标：推送到 GitHub `main` 分支后，自动生成 H5 产物，上传 Node API 运行时代码，并部署到阿里云轻量应用服务器。

当前服务器：

```text
120.55.46.251
```

## 1. 阿里云轻量应用服务器准备

在服务器上准备站点目录：

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

  location /api/ {
    proxy_pass http://127.0.0.1:8787;
    proxy_http_version 1.1;
    proxy_set_header Host $host;
    proxy_set_header X-Real-IP $remote_addr;
    proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
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
ALIYUN_HOST          120.55.46.251
ALIYUN_PORT          SSH 端口，通常是 22
ALIYUN_USER          admin
ALIYUN_SSH_KEY       部署用户的私钥内容
ALIYUN_DEPLOY_PATH   /var/www/lifetodo/dist/site
ALIYUN_APP_PATH      /var/www/lifetodo
```

`ALIYUN_PORT`、`ALIYUN_DEPLOY_PATH` 和 `ALIYUN_APP_PATH` 有默认值，但建议显式配置。

## 3. Node API 服务

GitHub Actions 会上传：

```text
api/
package.json
```

到 `ALIYUN_APP_PATH`。服务器上还需要先配置一次 systemd 服务和飞书 CLI 授权。

环境变量：

```bash
sudo tee /etc/lifetodo-api.env >/dev/null <<'EOF'
PORT=8787
LIFETODO_HOME_ID=demo-home
LIFETODO_LARK_BASE_TOKEN=L6kdbumDKa8QFosicOIcItBjncb
LIFETODO_LARK_TODO_TABLE_ID=tblrbHrBuoijFiXK
# 可选：飞书群机器人 webhook，用于 ESP 连续同步失败时通知
LIFETODO_LARK_ALERT_WEBHOOK=
EOF
```

systemd 服务，把 `<deploy-user>` 替换成部署用户：

```bash
sudo tee /etc/systemd/system/lifetodo-api.service >/dev/null <<'EOF'
[Unit]
Description=LifeTodo Node API
After=network.target

[Service]
Type=simple
User=<deploy-user>
WorkingDirectory=/var/www/lifetodo
EnvironmentFile=/etc/lifetodo-api.env
ExecStart=/usr/bin/node api/server.mjs
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now lifetodo-api
```

服务器上还要让同一个部署用户完成飞书授权：

```bash
lark-cli auth login --domain base
lark-cli auth status
```

## 4. SSH Key 建议

在本机生成一把专门用于部署的 key，或在轻量应用服务器上生成后取回私钥：

```bash
ssh-keygen -t ed25519 -C "github-actions-lifetodo"
```

你当前准备使用的公钥名称：

```text
github
```

公钥：

```text
ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCMkPAzaQTqf6AoNio3LsV/QUKNip5zZt2NEY0XTrcAg0xlCXlo+PQ8331dlqqVmFLdfdzLWsSFhkcQWqQ+KFKxoEfZHHPcUPY090zi7tBVfX4/rmlYFJnCU7RlkdeJaQTJd+rxxFI4FP8DWZksmVCg5qqQt6fGcZb+VhPb0yzsv9E4FZM3Pgzc/ZyYpMjH7XmsFPYZfQeGfVaauOfGlaP7mvzSGbJRG+7yvADcn9suROqLe27BpjdnEdYAxQ+cntozFQaXLF97FZMKG9o0bKSKLFc3vy+kqr3e2zKVNGcpX+a3d6GMGlu6C9PvsG9uFNj27KyCUQnbkwuG4CNpBx6p skp-bp1hwuiwm3c5fd3eevyg
```

把公钥加入轻量应用服务器部署用户的：

```text
~/.ssh/authorized_keys
```

示例：

```bash
mkdir -p ~/.ssh
chmod 700 ~/.ssh
echo 'ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCMkPAzaQTqf6AoNio3LsV/QUKNip5zZt2NEY0XTrcAg0xlCXlo+PQ8331dlqqVmFLdfdzLWsSFhkcQWqQ+KFKxoEfZHHPcUPY090zi7tBVfX4/rmlYFJnCU7RlkdeJaQTJd+rxxFI4FP8DWZksmVCg5qqQt6fGcZb+VhPb0yzsv9E4FZM3Pgzc/ZyYpMjH7XmsFPYZfQeGfVaauOfGlaP7mvzSGbJRG+7yvADcn9suROqLe27BpjdnEdYAxQ+cntozFQaXLF97FZMKG9o0bKSKLFc3vy+kqr3e2zKVNGcpX+a3d6GMGlu6C9PvsG9uFNj27KyCUQnbkwuG4CNpBx6p skp-bp1hwuiwm3c5fd3eevyg' >> ~/.ssh/authorized_keys
chmod 600 ~/.ssh/authorized_keys
```

然后把对应的私钥完整内容填入 GitHub Secret：

```text
ALIYUN_SSH_KEY
```

注意：GitHub Secrets 里填私钥，轻量应用服务器里放公钥。不要把私钥提交到仓库，也不要把私钥发到聊天或文档中。

## 5. 触发部署

把代码推到 GitHub `main` 分支：

```bash
git push origin main
```

或在 GitHub Actions 页面手动运行：

```text
Deploy to Aliyun -> Run workflow
```

## 6. 验证

部署成功后访问：

```text
http://120.55.46.251/api/state?home=demo-home
http://120.55.46.251/device/?home=demo-home&device=entry
```

API 返回 JSON，页面显示 `飞书多维表格已连接`，并且手机页完成任务后设备页同步变化，就说明链路成功。
