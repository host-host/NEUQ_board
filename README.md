# NEUQ_board

一个用 C/C++ 从零实现的轻量级 Web 后端。它不依赖任何 Web 框架，自带手写的 HTTP/HTTPS 服务器与一套基于 `mmap` 的持久化键值数据库（`ndb`）。

## 构建

依赖：`gcc` / `g++`、`make`，以及 libcurl 开发库。
> 注意：需要x86 cpu和linux系统。

```bash
# 安装依赖
# sudo apt install build-essential libssl-dev libcurl4-openssl-dev
# 编译，产物输出到 build/
make
```

## 运行

```bash
./build/1001
```
> 注意：生产环境置于反向代理（如 Nginx）之后并启用 HTTPS

## API 概览

所有接口以 `/api/` 为前缀，完整说明见 [`API_doc.txt`](API_doc.txt)。

## 许可

仓库未声明许可协议，使用前请联系作者。
