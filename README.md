# HUST_fs / 简易文件系统（Linux 内核模块）

[English](#english) | [中文](#中文)

---

# English

HUST_fs is a tiny block-based filesystem implemented as a Linux kernel module, plus a userspace `mkfs` tool to format a disk image. It is intended for learning and experimentation.

## Features

- Linux kernel filesystem driver (`HUST_fs.ko`)
- Userspace formatter (`mkfs`) for creating an on-disk image
- Basic file/dir operations and a simple on-disk layout
- Lightweight unit test for `mkfs` output (`make test`)

## Repository Layout

- `HUST_fs.h`: shared structures and function declarations
- `mkfs.c`: userspace formatter for the image
- `super.c` / `inode.c` / `file.c` / `block.c` / `map.c`: filesystem implementation
- `Makefile`: build targets for the kernel module, `mkfs`, and unit tests
- `test_mkfs.c`: validates `mkfs`-generated disk layout (no kernel module required)

## Requirements

- Linux with kernel headers installed (this project builds a kernel module)
- A working toolchain: `make`, `gcc`, `sudo`

Notes:

- The module is built against your current running kernel: `/lib/modules/$(uname -r)/build`.
- Running the module requires root privileges.

## Build

```bash
make
```

This produces:

- `HUST_fs.ko` (kernel module)
- `mkfs` (userspace tool)

## Quick Start (loopback image)

### 1) Create an empty image

```bash
dd if=/dev/zero of=image bs=4096 count=100
```

### 2) Format it with `mkfs`

```bash
./mkfs ./image
```

### 3) Load the kernel module

```bash
sudo insmod HUST_fs.ko
```

### 4) Mount and try basic I/O

```bash
mkdir -p ./mnt
sudo mount -o loop -t HUST_fs image ./mnt
sudo chmod -R 0777 ./mnt

cd ./mnt
cat file
echo "Hello World!" > file
cat file
```

### 5) Unmount and unload

```bash
cd ..
sudo umount ./mnt
sudo rmmod HUST_fs
```

## Disk Layout

On-disk layout (conceptual):

```text
block0  | dummy/reserved
block1  | super block
block2+ | bmap (block bitmap)
...     | imap (inode bitmap)
...     | inode table
...     | data blocks
```

For exact layout computation and initialization, see `mkfs.c`.

## Tests

Run unit tests (only checks `mkfs` output; does not load the kernel module):

```bash
make test
```

## Troubleshooting

- Build fails with missing headers:
  - Install kernel headers for your running kernel, then re-run `make`.
- Mount fails:
  - Check `dmesg` for kernel logs.
  - Ensure the image was formatted by `./mkfs` and mounted with `-t HUST_fs`.

## License

See `LICENSE`.

---

# 中文

HUST_fs 是一个用于学习/实验的简易块设备文件系统：内核态实现为 Linux 文件系统模块 `HUST_fs.ko`，并提供用户态格式化工具 `mkfs` 用于初始化磁盘镜像。

## 功能特性

- Linux 内核文件系统驱动（`HUST_fs.ko`）
- 用户态格式化工具（`mkfs`）用于创建/初始化镜像
- 基础文件/目录操作与简化的磁盘布局
- `mkfs` 的轻量单测（`make test`，无需加载内核模块）

## 代码结构

- `HUST_fs.h`：共享结构体与函数声明
- `mkfs.c`：用户态格式化工具
- `super.c` / `inode.c` / `file.c` / `block.c` / `map.c`：文件系统核心实现
- `Makefile`：构建内核模块、`mkfs`、单测
- `test_mkfs.c`：校验 `mkfs` 生成的磁盘布局（不依赖内核模块）

## 环境要求

- Linux + 已安装当前内核对应的 headers（本项目需要编译内核模块）
- 基础工具链：`make`、`gcc`、`sudo`

说明：

- 模块默认使用 `/lib/modules/$(uname -r)/build` 对当前运行内核进行编译。
- 加载模块/挂载文件系统需要 root 权限。

## 构建

```bash
make
```

产物：

- `HUST_fs.ko`：内核模块
- `mkfs`：用户态工具

## 快速开始（loop 镜像）

### 1）创建空镜像

```bash
dd if=/dev/zero of=image bs=4096 count=100
```

### 2）使用 `mkfs` 格式化

```bash
./mkfs ./image
```

### 3）加载内核模块

```bash
sudo insmod HUST_fs.ko
```

### 4）挂载并进行读写验证

```bash
mkdir -p ./mnt
sudo mount -o loop -t HUST_fs image ./mnt
sudo chmod -R 0777 ./mnt

cd ./mnt
cat file
echo "Hello World!" > file
cat file
```

### 5）卸载与移除模块

```bash
cd ..
sudo umount ./mnt
sudo rmmod HUST_fs
```

## 磁盘布局

概念布局如下（具体以 `mkfs.c` 的计算与初始化为准）：

```text
block0  | dummy/reserved
block1  | super block
block2+ | bmap（数据块位图）
...     | imap（inode 位图）
...     | inode table（inode 表）
...     | data blocks（数据区）
```

## 测试

运行单测（只校验 `mkfs` 生成的镜像布局，不会加载内核模块）：

```bash
make test
```

## 常见问题

- 编译报缺少内核头文件：
  - 安装与你当前运行内核版本匹配的 kernel headers，然后重新 `make`。
- 挂载失败：
  - 先执行 `dmesg` 查看内核日志。
  - 确保镜像已被 `./mkfs` 正确格式化，挂载时指定 `-t HUST_fs`。

## 许可协议

见 `LICENSE`。
