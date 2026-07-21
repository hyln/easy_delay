# 兼容性说明

## 运行模型

wheel安装在控制主机。控制主机运行 C++客户端，并通过 SSH把与目标架构匹配的
静态 C++服务端上传到目标主机。目标机不需要安装 Python、编译器或运行库。

目标架构通过远程执行 `uname -m`自动检测，不能由配置覆盖：

| `uname -m` | 内部架构 | 服务端文件 |
| --- | --- | --- |
| `x86_64`、`amd64` | `amd64` | `easy-delay-server-amd64` |
| `aarch64`、`arm64` | `arm64` | `easy-delay-server-arm64` |
| `armv7l`、`armv7` | `armv7` | `easy-delay-server-armv7` |

当前正式构建目标是：

- 控制主机：Linux x86-64；
- 目标主机：Linux amd64或ARM64；
- Python：3.11及以上；
- SSH认证：密码认证；
- 网络：控制端能够访问目标端 SSH端口和配置的 UDP端口。

ARMv7协议映射已经存在，但只有 wheel包含
`easy-delay-server-armv7`时才可使用。

## libc与内核

目标服务端采用完全静态链接，不依赖目标机的 glibc、musl或动态加载器版本。
它仅使用 Linux长期稳定的基础接口，包括 socket、poll、clock_gettime、recvfrom
和 sendto。

数据包不直接传输 C++结构体或 `timespec`。协议使用固定宽度整数、显式网络字节序
和固定长度编码，因此不依赖 x86/ARM的内存对齐、字节序或 ABI布局。

## x64构建ARM兼容wheel

Debian或Ubuntu构建机需要 ARM64交叉编译器：

```bash
sudo apt update
sudo apt install g++-aarch64-linux-gnu
```

构建并检查发行文件：

```bash
python -m pip install --upgrade build twine
python -m build --outdir release-dist
python -m twine check release-dist/*
unzip -l release-dist/*.whl | grep easy-delay-server
```

x64发行wheel必须同时包含：

```text
easy_delay/bin/easy-delay-client
easy_delay/bin/easy-delay-server-amd64
easy_delay/bin/easy-delay-server-arm64
```

CMake默认启用 `EASY_DELAY_REQUIRE_ARM64_SERVER`。如果ARM64交叉编译器缺失，
构建会直接失败，不会生成缺少ARM64服务端的不完整wheel。

仅进行本机开发编译时，可以明确关闭ARM64发行约束：

```bash
cmake -S . -B build -DEASY_DELAY_REQUIRE_ARM64_SERVER=OFF
cmake --build build
```

关闭该选项生成的产物不应发布。

## wheel边界

发行wheel包含 x86-64本地客户端，因此wheel本身安装在 Linux x86-64控制主机。
ARM64是远程目标机架构，不表示该wheel可以安装到ARM64控制主机。如果未来需要
ARM64作为控制主机，应另外发行ARM64平台wheel；包名和安装命令可以保持不变，
由 pip根据平台标签自动选择。

