# NovaChat

一款基于 **C++20 + Qt6 Widgets** 的企业级 IM 桌面客户端，配套 C++ TCP 服务端。

项目定位参考企业微信 / 飞书，目标是实现一个具有**良好工程架构、清晰模块边界、可持续迭代**的 IM 基础版本，而非功能堆砌的原型。

---

## 功能一览

| 模块 | 功能 |
|------|------|
| **用户系统** | 注册、登录、自动登录（Token 持久化）、退出登录 |
| **联系人** | 好友列表、搜索用户、添加 / 删除好友 |
| **聊天** | 单聊文本收发、历史消息分页加载、最近会话列表、未读数徽标 |
| **本地持久化** | SQLite WAL 模式，重启后完整恢复数据（消息、会话、联系人） |
| **网络协议** | 自定义二进制帧 + Protobuf 序列化，TCP 长连接，心跳保活 |

---

## 技术栈

| 用途 | 技术 |
|------|------|
| 语言 | C++20 |
| GUI | Qt6 Widgets |
| 构建 | CMake + vcpkg |
| 网络 | Standalone Asio（无 Boost） |
| 通信协议 | Protocol Buffers（自定义二进制帧，非 JSON） |
| 数据库 | SQLite（WAL 模式） |
| 日志 | spdlog |
| 配置 | yaml-cpp |
| 测试 | Catch2 |
| 依赖管理 | vcpkg |
| 目标平台 | Windows（MSVC 2022） |

---

## 架构设计

项目采用经典四层架构，各层严格单向依赖，业务逻辑与 UI、网络、存储完全解耦。

```
┌──────────────────────────────────────────────────────┐
│                     UI Layer                         │
│   LoginWindow · MainWindow · ChatView · ContactPanel │
│   ConversationList · SettingsDialog                  │
└──────────────────────┬───────────────────────────────┘
                       │  Qt signals / 直接调用
┌──────────────────────▼───────────────────────────────┐
│                  Service Layer                       │
│   AuthService · ChatService · ContactService         │
└────────┬──────────────────────────────┬──────────────┘
         │ 操作 Domain 对象              │ 调用 Repository / Network
┌────────▼────────┐         ┌───────────▼──────────────┐
│  Domain Layer   │         │   Infrastructure Layer   │
│  User · Message │         │   TcpClient (Asio)       │
│  Conversation   │         │   ProtocolCodec (Protobuf)│
│  Contact        │         │   DatabaseManager (SQLite)│
└─────────────────┘         │   Repository (CRUD)      │
                            │   Logger (spdlog)        │
                            └──────────────────────────┘
```

**核心约束：**
- UI 只调 Service，不直接操作网络或数据库
- Domain 层为纯 C++ 值类型，无任何框架依赖
- Service 层只在主线程（Qt 事件循环）运行

---

## 线程模型

```
┌─────────────────────────────────────────────────────┐
│              Main Thread (Qt 事件循环)               │
│   UI 渲染 · Service 层 · 业务逻辑                    │
└──────┬──────────────────────────────────────────────┘
       │  asio::post(io_ctx, lambda)
       ▼
┌──────────────────────┐
│   Network Thread     │
│  (Asio io_context)   │  ── Qt 信号 ──▶  Main Thread
│  TCP 读写 · 编解码   │
│  心跳定时器 · 重连   │
└──────────────────────┘
```

| 跨线程方向 | 机制 |
|-----------|------|
| Network → Main | `emit` Qt 信号（自动 `QueuedConnection`） |
| Main → Network | `asio::post(io_ctx, lambda)` |

---

## 通信协议

自定义 12 字节定长帧头 + Protobuf 变长载荷：

```
┌─────────┬────────────┬──────────┬──────────┬─────────────────┐
│Magic(2B)│Payload(4B) │MsgId(2B) │SeqId(4B) │Payload(nB)      │
│ 0x4E43  │  length    │  type    │ seq num  │ Protobuf bytes  │
└─────────┴────────────┴──────────┴──────────┴─────────────────┘
```

`ProtocolCodec` 维护内部接收缓冲区，处理 TCP 粘包 / 分包，Big-Endian 字节序。

消息类型涵盖：认证（0x00xx）、联系人（0x01xx）、聊天（0x02xx）、系统（0x0Fxx）。

---

## 工程结构

```
NovaChat/
├── proto/                   # 客户端/服务端共用协议
│   ├── *.proto              # Protobuf 定义（auth / contact / chat / common）
│   ├── ProtocolCodec.h/cpp  # 帧编解码器
│   └── MessageType.h        # 消息类型枚举
│
├── client/
│   └── src/
│       ├── core/            # Domain 层（纯 C++ 值类型）
│       ├── network/         # TcpClient（Asio 异步 TCP）
│       ├── storage/         # DatabaseManager + 4 个 Repository
│       ├── service/         # AuthService / ChatService / ContactService
│       ├── ui/              # Qt6 Widgets UI（9 个组件）
│       ├── config/          # AppConfig（yaml-cpp）
│       └── logger/          # Logger（spdlog 封装）
│
├── server/
│   └── src/
│       ├── network/         # TcpServer + ClientSession（Asio）
│       ├── handler/         # AuthHandler / ChatHandler / ContactHandler
│       └── storage/         # ServerStorage（JSON 持久化，用于联调）
│
├── CMakeLists.txt
├── CMakePresets.json        # windows-debug preset（Ninja + MSVC）
└── vcpkg.json               # 依赖清单
```

---

## 本地构建

### 前置条件

| 工具 | 版本 |
|------|------|
| Visual Studio 2022 | 含 C++ Desktop 工作负载 |
| Qt6 | 6.8.x（MSVC 2022 x64），安装至 `D:\software\qt6` |
| CMake | ≥ 3.25 |
| Ninja | 随 VS2022 安装 |
| vcpkg | 安装至 `C:\vcpkg`，并设置 `VCPKG_ROOT` 环境变量 |

### 构建步骤

```bat
:: 第一次：完整配置（会触发 vcpkg 依赖下载和编译，约 10-20 分钟）
build_configure.bat

:: 后续增量编译（仅重新编译变化的文件，约 10 秒）
build_build.bat

:: 运行单元测试
build_test.bat
```

> **注意：** `build_configure.bat` 会删除 `build/debug` 目录并重新配置，
> 仅在首次搭建或 CMakeLists.txt 发生重大变更时运行。

---

## 运行

```bat
:: 1. 启动服务端（保持运行）
run_server.bat

:: 2. 启动客户端 Alice（新窗口）
run_client_alice.bat

:: 3. 启动客户端 Bob（新窗口）
run_client_bob.bat
```

两个客户端通过 `--profile alice` / `--profile bob` 隔离本地数据库，可在同一台机器上模拟双用户收发消息。

---

## 设计亮点

### RAII 资源管理

所有资源（数据库连接、网络句柄）均通过析构函数自动释放，无裸 `new/delete`，无资源泄漏。

### 值语义的 Domain 对象

`User` / `Message` / `Contact` 均为可拷贝的值类型，跨线程传递时直接复制而非共享指针，消除数据竞争。

### 自定义二进制协议

相较于 JSON over TCP，帧解析 O(1) 定长头 + 状态机缓冲区，避免字符串扫描开销；Protobuf 序列化比 JSON 紧凑约 3–5 倍。

### 分层测试

`client/tests/` 下按模块独立组织单元测试，存储层、协议层、配置层可脱离 UI 单独验证。

---

## 开发过程中修复的典型 Bug

**悬空引用导致 abort() 崩溃**

`ContactService` 和 `ChatService` 构造时接收 `const User&`，并将其存为成员引用。当 `launch_main()` 返回后，局部 `user` 变量析构，两个 Service 持有的引用变为悬空指针。后续 TCP 连接建立时访问 `current_user_.token`，触发 MSVC debug CRT 堆检测，抛出 `abort()`。

```cpp
// 修复前：存储引用，生命周期不受控
const User& current_user_;

// 修复后：持有副本，生命周期由 Service 自身管理
User current_user_;  // Owned copy — never a dangling reference
```

---

## License

MIT
