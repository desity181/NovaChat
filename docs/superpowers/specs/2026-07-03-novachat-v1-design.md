# NovaChat V1 架构设计文档

**日期：** 2026-07-03
**版本：** V1
**状态：** 已确认

---

## 一、项目概述

NovaChat V1 是一款基于 **C++20 + Qt6 Widgets** 的企业级 IM 桌面客户端，配套一个用于联调测试的简单 C++ 服务端。

**V1 功能范围：**
- 用户系统：注册、登录、自动登录、Token 本地保存、退出登录
- 联系人系统：好友列表、添加/删除/搜索好友
- 聊天系统：单聊、文本消息收发、历史消息、最近会话列表、未读数、消息时间
- 本地数据：SQLite 持久化，重启恢复数据

**V1 明确不做：** 群聊、图片/文件、音视频、消息撤回/已读、云同步、插件系统、QML、移动端。

---

## 二、技术栈

| 用途 | 技术 |
|------|------|
| 语言 | C++20 |
| GUI | Qt6 Widgets（系统安装，非 vcpkg） |
| 构建 | CMake + vcpkg |
| 网络 | Standalone Asio |
| 通信协议 | Protocol Buffers（自定义二进制协议，不使用 JSON） |
| 数据库 | SQLite（WAL 模式） |
| 日志 | spdlog |
| 配置 | yaml-cpp |
| 辅助 JSON | nlohmann/json（仅用于调试/配置） |
| 依赖管理 | vcpkg（Qt6 除外） |
| 目标平台 | 跨平台，V1 优先保证 Windows 可用 |

---

## 三、整体架构

### 分层架构（方案 A）

```
┌──────────────────────────────────────────────────────┐
│                     UI Layer                         │
│   LoginWindow  MainWindow  ChatView  ContactPanel    │
│                  SettingsDialog                      │
└──────────────────────┬───────────────────────────────┘
                       │ Qt signals / 直接调用 Service
┌──────────────────────▼───────────────────────────────┐
│                  Service Layer                       │
│   AuthService  ChatService  ContactService           │
│   SessionService                                     │
└────────┬──────────────────────────────┬──────────────┘
         │ 操作 Domain 对象              │ 调用 Repository / Network
┌────────▼────────┐         ┌───────────▼──────────────┐
│  Domain Layer   │         │   Infrastructure Layer   │
│  User           │         │   NetworkClient (Asio)   │
│  Message        │         │   ProtocolCodec (Protobuf)│
│  Conversation   │         │   DatabaseManager (SQLite)│
│  Contact        │         │   Repository (CRUD)      │
└─────────────────┘         │   Config (yaml-cpp)      │
                            │   Logger (spdlog)        │
                            └──────────────────────────┘
```

**核心约束：**
- UI 只调 Service，不直接操作网络或数据库
- Domain 层（User/Message/Conversation/Contact）为纯 C++ 值类型，无任何框架依赖
- Network 层通过 Qt 信号向 Service 层通知事件，Service 不感知 TCP 细节

---

## 四、工程目录结构

```
NovaChat/
├── CMakeLists.txt
├── vcpkg.json                         # Asio、Protobuf、SQLite、spdlog、yaml-cpp
├── cmake/
│   └── vcpkg.cmake
│
├── proto/                             # 客户端/服务端共用
│   ├── common.proto
│   ├── auth.proto
│   ├── contact.proto
│   └── chat.proto
│
├── client/
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp
│       ├── core/                      # 领域层（纯 C++）
│       │   ├── User.h
│       │   ├── Message.h
│       │   ├── Conversation.h
│       │   └── Contact.h
│       ├── network/                   # 网络层
│       │   ├── TcpClient.h/cpp
│       │   ├── ProtocolCodec.h/cpp
│       │   └── PacketDispatcher.h/cpp
│       ├── storage/                   # 存储层
│       │   ├── DatabaseManager.h/cpp
│       │   ├── UserRepository.h/cpp
│       │   ├── MessageRepository.h/cpp
│       │   ├── ContactRepository.h/cpp
│       │   └── ConversationRepository.h/cpp
│       ├── service/                   # 服务层
│       │   ├── AuthService.h/cpp
│       │   ├── ChatService.h/cpp
│       │   ├── ContactService.h/cpp
│       │   └── SessionService.h/cpp
│       ├── ui/                        # UI 层
│       │   ├── LoginWindow.h/cpp
│       │   ├── MainWindow.h/cpp
│       │   ├── ChatView.h/cpp
│       │   ├── ContactPanel.h/cpp
│       │   ├── ConversationList.h/cpp
│       │   └── SettingsDialog.h/cpp
│       ├── config/
│       │   └── AppConfig.h/cpp
│       └── logger/
│           └── Logger.h/cpp
│
└── server/
    ├── CMakeLists.txt
    └── src/
        ├── main.cpp
        ├── network/
        │   ├── TcpServer.h/cpp
        │   └── ClientSession.h/cpp
        ├── handler/
        │   ├── AuthHandler.h/cpp
        │   ├── ChatHandler.h/cpp
        │   └── ContactHandler.h/cpp
        └── storage/
            └── ServerStorage.h/cpp
```

**说明：**
- `proto/` 在根目录，客户端/服务端 CMake 共同引用，避免协议不一致
- Qt6 通过官方安装器安装，`Qt6_DIR` 指向系统 Qt；vcpkg 只管理其余依赖
- 数据库路径使用 `QStandardPaths::AppLocalDataLocation`，跨平台兼容

---

## 五、通信协议设计

### 数据包格式

```
┌──────────┬────────────┬──────────┬──────────┬─────────────────┐
│ Magic(2B)│Payload(4B) │MsgId(2B) │ SeqId(4B)│ Payload(nB)     │
│  0x4E43  │  length    │  type    │  seq num │ Protobuf bytes  │
└──────────┴────────────┴──────────┴──────────┴─────────────────┘
       固定头部 12 字节                          变长
```

- **Magic** `0x4E43`（"NC"）：帧同步标记
- **Payload Length**：Payload 字节数，不含头部，大端 uint32
- **MsgId**：消息类型枚举 uint16
- **SeqId**：客户端自增 uint32；服务端响应原样回传；服务端主动推送时为 0

### 消息类型

```
0x00xx  Auth      LoginReq/Resp  RegisterReq/Resp  LogoutReq
0x01xx  Contact   GetContactsReq/Resp  AddFriendReq/Resp
                  DeleteFriendReq/Resp  SearchUserReq/Resp
0x02xx  Chat      SendMsgReq/Resp  MsgPush(server→client)
                  GetHistoryReq/Resp  GetConversationsReq/Resp
0x0Fxx  System    HeartbeatReq/Resp  ErrorResp
```

### 连接生命周期

```
Client                          Server
  │─── TCP Connect ──────────────▶│
  │─── LoginRequest ─────────────▶│
  │◀── LoginResponse(token) ──────│
  │─── HeartbeatReq(每 30s) ─────▶│
  │◀── HeartbeatResp ─────────────│
  │─── SendMsgRequest ───────────▶│
  │◀── SendMsgResponse ───────────│
  │◀── MsgPush（新消息）───────────│
  │─── LogoutRequest ────────────▶│
  │─── TCP Close ────────────────▶│
```

### 核心 Proto 定义

SeqId 已在二进制帧头中携带，Protobuf 消息层不重复定义。Token 嵌入每条需要鉴权的请求消息的第一个字段：

```protobuf
// common.proto
message ErrorResponse {
    int32  code    = 1;
    string message = 2;
}

// auth.proto（示例）
message LoginRequest {
    string username = 1;
    string password = 2;
    // 登录/注册请求不携带 token
}
message LoginResponse {
    string token       = 1;
    string user_id     = 2;
    string display_name = 3;
}

// chat.proto（示例）
message SendMessageRequest {
    string token           = 1;  // 所有需鉴权的请求第一个字段均为 token
    string conversation_id = 2;
    string content         = 3;
}
```

**SeqId 回调机制：** 客户端维护 `unordered_map<uint32_t, ResponseCallback>`，响应到达时按帧头 SeqId 查找并触发回调，天然支持并发请求。

**鉴权：** 登录成功后，所有需鉴权请求的 Protobuf 消息第一字段携带 Token，服务端验证；Token 失效时服务端返回 `AUTH_EXPIRED` 错误码，客户端清除本地 Token 并跳回登录界面。

---

## 六、数据库设计

数据库路径：`<AppLocalData>/NovaChat/<server_user_id>/data.db`

SQLite 启用 WAL 模式：`PRAGMA journal_mode=WAL`

### 表结构

```sql
-- 本地账号（支持多账号）
CREATE TABLE local_accounts (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    server_id    TEXT    NOT NULL UNIQUE,
    username     TEXT    NOT NULL,
    display_name TEXT    NOT NULL,
    avatar_url   TEXT,
    token        TEXT    NOT NULL,
    is_active    INTEGER NOT NULL DEFAULT 0
);

-- 联系人
CREATE TABLE contacts (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    owner_id     TEXT    NOT NULL,
    user_id      TEXT    NOT NULL,
    username     TEXT    NOT NULL,
    display_name TEXT    NOT NULL,
    avatar_url   TEXT,
    UNIQUE(owner_id, user_id)
);

-- 最近会话
CREATE TABLE conversations (
    id                   INTEGER PRIMARY KEY AUTOINCREMENT,
    owner_id             TEXT    NOT NULL,
    conversation_id      TEXT    NOT NULL,
    target_id            TEXT    NOT NULL,
    last_message_preview TEXT,
    last_message_time    INTEGER NOT NULL DEFAULT 0,
    unread_count         INTEGER NOT NULL DEFAULT 0,
    UNIQUE(owner_id, conversation_id)
);

-- 聊天记录
CREATE TABLE messages (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    message_id      TEXT    NOT NULL UNIQUE,
    conversation_id TEXT    NOT NULL,
    sender_id       TEXT    NOT NULL,
    content         TEXT    NOT NULL,
    msg_type        INTEGER NOT NULL DEFAULT 1,
    timestamp       INTEGER NOT NULL,
    status          INTEGER NOT NULL DEFAULT 0   -- 0=发送中 1=已发送 2=失败
);
CREATE INDEX idx_messages_conv ON messages(conversation_id, timestamp);

-- 客户端配置
CREATE TABLE app_config (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
```

**设计要点：**
- ID 使用 TEXT 存储服务端 ID（兼容 UUID/雪花 ID）
- 时间戳统一使用 Unix 毫秒整数，避免时区问题
- 多账号数据通过 `owner_id` 隔离，数据库文件按 user_id 分目录
- 消息按 `(conversation_id, timestamp)` 建索引，支持高效历史查询

---

## 七、线程模型

### 线程分工

```
┌─────────────────────────────────────────────────────────────┐
│                    Main Thread (Qt)                         │
│  Qt 事件循环 · UI 渲染 · Service 层 · 业务逻辑              │
└──────┬──────────────────────────────────────┬──────────────┘
       │  asio::post(io_ctx, lambda)          │  DbTask enqueue
       ▼                                      ▼
┌──────────────────────┐          ┌───────────────────────────┐
│   Network Thread     │          │      DB Thread            │
│  (Asio io_context)   │          │  (单线程 + 任务队列)       │
│  · TCP 读写          │          │  · SQLite 读写             │
│  · 协议编解码        │          │  · 序列化访问，无锁        │
│  · 心跳定时器        │          │  · 结果回调主线程          │
│  · 重连逻辑          │          └───────────────────────────┘
└──────────────────────┘
```

### 跨线程通信

| 方向 | 机制 |
|------|------|
| Network → Main | `emit` Qt 信号（自动 QueuedConnection） |
| Main → Network | `asio::post(io_ctx, lambda)` |
| Main → DB | 投递 `std::function` 到 DB 线程任务队列 |
| DB → Main | `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` |

### 线程安全规则

- Domain 对象跨线程传递用值拷贝或 `shared_ptr<const T>`
- SQLite 连接只在 DB 线程创建和使用（调试模式加 assert 线程 ID 检查）
- Asio 回调内不访问 Qt 对象，只 emit 信号或 post 回主线程
- Service 层只在主线程运行

### 启动/关闭顺序

```
启动：Logger → Config → DB Thread → Network Thread → Qt UI
关闭：Qt UI → Network Thread(stop io_ctx) → DB Thread(flush) → Logger flush
```

---

## 八、开发阶段规划

### 阶段一：工程初始化

**目标：** 空壳项目成功构建，客户端打开空白窗口

- CMake 根文件 + vcpkg.json 声明所有依赖（Qt6 除外）
- client/ 和 server/ 子项目各自 CMakeLists.txt
- 建立完整目录骨架
- spdlog Logger 全局初始化
- 验证：`cmake --build` 通过，客户端无崩溃启动

### 阶段二：基础设施

**目标：** 基础设施层可独立运行，网络心跳收发正常

| 子阶段 | 内容 |
|--------|------|
| 2a 配置 | AppConfig 读写 config.yaml |
| 2b 数据库 | DatabaseManager 建表 + Repository 基础 CRUD |
| 2c 协议 | .proto 文件全部定义，生成代码集成到构建 |
| 2d 网络 | TcpClient + TcpServer 连通，Heartbeat 收发验证 |

### 阶段三：登录模块

**目标：** 注册、登录、自动登录、退出登录完整流程

- Auth proto 消息 + 服务端 AuthHandler
- AuthService + LoginWindow UI
- Token 写入 SQLite，启动时自动登录

### 阶段四：联系人模块

**目标：** 查看/添加/删除/搜索好友

- Contact proto 消息 + 服务端 ContactHandler
- ContactService + ContactRepository
- ContactPanel UI（列表 + 搜索框 + 操作按钮）
- 登录后自动同步联系人到本地

### 阶段五：聊天模块

**目标：** 实时收发文本消息，历史记录持久化，未读数正确

- Chat proto 消息 + 服务端 ChatHandler（在线推送，离线落库）
- ChatService + MessageRepository + ConversationRepository
- ConversationList UI + ChatView UI（QListView + 自定义 Delegate 气泡）
- 消息历史分页加载（每次 50 条）

### 阶段六：完善收尾

**目标：** 完整可用的 V1 客户端

- SettingsDialog（账号信息、退出登录）
- 断线重连逻辑
- 统一错误提示（ErrorResponse → UI）
- UI 风格统一
- 完整集成验证：注册→登录→加好友→聊天→重启恢复

---

## 九、风险评估与优化建议

### 高风险

**Qt + Asio 跨线程信号崩溃**
所有 Network → Main 信号连接必须显式指定 `Qt::QueuedConnection`。在 Asio 回调中严禁直接操作 Qt 对象。

**TCP 粘包/分包处理**
ProtocolCodec 必须维护接收缓冲区 + 状态机。使用 `asio::async_read` 精确读取固定字节数，而非 `async_read_some`。

### 中风险

**SQLite 跨线程访问**
DatabaseManager 构造时记录线程 ID，所有公开方法在调试模式下 assert 线程匹配。

**Qt Widgets 聊天气泡复杂度**
使用 `QListView` + 自定义 `QStyledItemDelegate`，在 `sizeHint()` 中按文本宽度计算气泡高度。禁止用 QLabel 堆叠实现气泡。

### 低风险

**vcpkg 编译 Qt6 耗时**
Qt6 通过官方安装器安装，vcpkg 只管理其余依赖。

### 优化建议

| 建议 | 说明 |
|------|------|
| SQLite WAL 模式 | `PRAGMA journal_mode=WAL` 提升读写并发性能 |
| 消息历史分页 | 每次加载最近 50 条，上滑加载更多 |
| 联系人搜索 | 使用 `QSortFilterProxyModel`，无需手写过滤逻辑 |
| Token 失效处理 | AUTH_EXPIRED 错误码 → 清除本地 Token → 跳回登录界面 |
| 数据库路径 | 使用 `QStandardPaths::AppLocalDataLocation` 跨平台兼容 |
