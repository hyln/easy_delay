# easy-delay

`easy-delay`用于判断两台 Linux 主机的时钟偏差是否满足指定阈值。程序通过
SSH向目标机部署临时 UDP 服务端，并使用类似 NTP 的四时间戳方法完成测量；
实际计时和统计均由 C++程序执行。

## 安装

```bash
pip install easy-delay
```

## 配置

首次运行：

```bash
easy-delay
```

如果当前目录没有 `easy-delay.toml`，程序会生成示例配置并退出。编辑配置：

```toml
[target]
host = "192.168.1.20"
user = "root"
password = "your-password"
port = 49220
ssh_port = 22
host_key_policy = "auto_add"

[measurement]
threshold_ms = 50.0
safety_margin_ms = 10.0
samples = 100
interval_ms = 10
```

配置文件包含明文 SSH密码，应限制文件权限：

```bash
chmod 600 easy-delay.toml
```

测量其他配置文件可使用：

```bash
easy-delay --config /path/to/easy-delay.toml
```

## 测量

配置完成后运行：

```bash
easy-delay
```

程序输出 JSON报告。`result`可能为：

- `pass`：保守估算的时钟偏差小于阈值并留有安全余量；
- `fail`：时钟偏差超过阈值；
- `uncertain`：结果位于阈值附近，需要增加测量或同步时钟；
- `error`：配置、SSH、部署或采样失败。

退出码为 `0`表示通过，`1`表示失败或结果不确定，`2`表示执行错误。

## 构建

在 Debian或Ubuntu x86-64主机安装构建工具和ARM64交叉编译器：

```bash
sudo apt update
sudo apt install build-essential g++-aarch64-linux-gnu
python -m pip install --upgrade build twine
```

在项目根目录构建源码包和wheel：

```bash
python -m build --outdir release-dist
python -m twine check release-dist/*
```

检查wheel同时包含 amd64和ARM64目标服务端：

```bash
unzip -l release-dist/*.whl | grep easy-delay-server
```

预期看到：

```text
easy_delay/bin/easy-delay-server-amd64
easy_delay/bin/easy-delay-server-arm64
```

安装本地构建结果：

```bash
pip install --force-reinstall release-dist/*.whl
```

## 兼容性与发行

控制端与目标端架构、静态链接和 wheel构建要求见
[compatibility.md](compatibility.md)。
