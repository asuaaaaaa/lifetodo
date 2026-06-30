# GitHub Actions 部署到阿里云

目标：推送到 GitHub `main` 分支后，自动生成 H5 产物并部署到阿里云轻量应用服务器的 Nginx 站点目录。

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
ALIYUN_USER          部署用户
ALIYUN_SSH_KEY       部署用户的私钥内容
ALIYUN_DEPLOY_PATH   /var/www/lifetodo/dist/site
```

`ALIYUN_PORT` 和 `ALIYUN_DEPLOY_PATH` 有默认值，但建议显式配置。

## 3. SSH Key 建议

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

注意：GitHub Secrets 里填私钥，轻量应用服务器里放公钥。不要把私钥提交到仓库。

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
