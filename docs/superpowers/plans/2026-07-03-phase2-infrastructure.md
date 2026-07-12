# NovaChat V1 — 阶段二实现计划：基础设施

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成所有基础设施层：领域模型、AppConfig、DatabaseManager、Protobuf 协议定义、ProtocolCodec 报文编解码、服务端 TcpServer/ClientSession、客户端 TcpClient，最终验证心跳包端到端收发正常。

**Architecture:** proto/ 目录产出 `NovaChat_Proto` 静态库，包含 Protobuf 生成代码、MessageType 枚举和 ProtocolCodec；客户端的 `NovaChat_Core` 和服务端可执行文件均链接此库，实现 DRY。Asio 运行于独立线程，通过 Qt 信号（QueuedConnection）将事件投递回主线程。

**Tech Stack:** C++20 · Standalone Asio · Protobuf 3 · SQLite3 (WAL) · yaml-cpp · spdlog · Qt6::Core

---

## 本阶段新增/修改文件

```
NovaChat/
├── CMakeLists.txt                          ← 修改：添加 Protobuf/Asio/SQLite/yaml-cpp
├── proto/                                  ← 全部新增
│   ├── CMakeLists.txt
│   ├── MessageType.h
│   ├── ProtocolCodec.h
│   ├── ProtocolCodec.cpp
│   ├── common.proto
│   ├── auth.proto
│   ├── contact.proto
│   └── chat.proto
├── client/
│   ├── CMakeLists.txt                      ← 修改：添加新模块到 NovaChat_Core
│   ├── src/
│   │   ├── core/                           ← 新增领域模型头文件
│   │   │   ├── LocalAccount.h
│   │   │   ├── User.h
│   │   │   ├── Contact.h
│   │   │   ├── Message.h
│   │   │   └── Conversation.h
│   │   ├── config/
│   │   │   ├── AppConfig.h                 ← 新增
│   │   │   └── AppConfig.cpp               ← 新增
│   │   ├── storage/
│   │   │   ├── DatabaseManager.h           ← 新增
│   │   │   └── DatabaseManager.cpp         ← 新增
│   │   └── network/
│   │       ├── TcpClient.h                 ← 新增
│   │       └── TcpClient.cpp               ← 新增
│   └── tests/
│       ├── CMakeLists.txt                  ← 修改：添加新测试目标
│       ├── config/
│       │   └── test_appconfig.cpp          ← 新增
│       ├── storage/
│       │   └── test_database.cpp           ← 新增
│       └── network/
│           └── test_codec.cpp              ← 新增
└── server/
    ├── CMakeLists.txt                      ← 修改：添加 Asio/网络层
    └── src/
        ├── main.cpp                        ← 修改：接入真实 TcpServer
        └── network/
            ├── TcpServer.h                 ← 新增
            ├── TcpServer.cpp               ← 新增
            ├── ClientSession.h             ← 新增
            └── ClientSession.cpp           ← 新增
```

---

## Task 5：定义领域模型（Domain Models）

**Files:**
- Create: `client/src/core/LocalAccount.h`
- Create: `client/src/core/User.h`
- Create: `client/src/core/Contact.h`
- Create: `client/src/core/Message.h`
- Create: `client/src/core/Conversation.h`

> 纯数据结构，无任何框架依赖，无需单元测试。

- [ ] **Step 1：写 client/src/core/LocalAccount.h**

```cpp
#pragma once
#include <string>
#include <cstdint>

// 本地已登录账号信息，对应 local_accounts 表一行
struct LocalAccount {
    int64_t     id{0};
    std::string server_id;
    std::string username;
    std::string display_name;
    std::string avatar_url;
    std::string token;
    bool        is_active{false};
};
```

- [ ] **Step 2：写 client/src/core/User.h**

```cpp
#pragma once
#include <string>

// 当前登录用户的运行时信息（从 LocalAccount 提取，存于内存）
struct User {
    std::string server_id;
    std::string username;
    std::string display_name;
    std::string avatar_url;
    std::string token;
};
```

- [ ] **Step 3：写 client/src/core/Contact.h**

```cpp
#pragma once
#include <string>

// 联系人条目，对应 contacts 表一行
struct Contact {
    std::string user_id;
    std::string owner_id;       // 所属本地账号的 server_id
    std::string username;
    std::string display_name;
    std::string avatar_url;
};
```

- [ ] **Step 4：写 client/src/core/Message.h**

```cpp
#pragma once
#include <string>
#include <cstdint>

enum class MessageStatus : int {
    Sending = 0,  // 发送中（默认）
    Sent    = 1,  // 已发送（服务端确认）
    Failed  = 2,  // 发送失败
};

struct Message {
    std::string   message_id;
    std::string   conversation_id;
    std::string   sender_id;
    std::string   content;
    int64_t       timestamp{0};     // Unix 毫秒时间戳
    MessageStatus status{MessageStatus::Sending};
};
```

- [ ] **Step 5：写 client/src/core/Conversation.h**

```cpp
#pragma once
#include <string>
#include <cstdint>

struct Conversation {
    std::string conversation_id;
    std::string owner_id;
    std::string target_id;
    std::string last_message_preview;
    int64_t     last_message_time{0};
    int         unread_count{0};
};
```

- [ ] **Step 6：提交**

```
git add client/src/core/
git commit -m "feat(core): 添加领域模型头文件（LocalAccount/User/Contact/Message/Conversation）"
```

---

## Task 6：更新根 CMakeLists.txt，添加全部依赖

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1：替换根 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.25)
project(NovaChat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Windows：Asio 需要最低 Windows 10 API
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

find_package(Qt6          REQUIRED COMPONENTS Core Widgets)
find_package(spdlog       CONFIG REQUIRED)
find_package(Catch2 3     CONFIG REQUIRED)
find_package(protobuf     CONFIG REQUIRED)
find_package(SQLite3      REQUIRED)
find_package(yaml-cpp     CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(asio         CONFIG REQUIRED)

enable_testing()

add_subdirectory(proto)    # 先于 client/server，产出 NovaChat_Proto
add_subdirectory(client)
add_subdirectory(server)
```

- [ ] **Step 2：验证配置通过**

```
cmake --preset windows-debug
```

预期：所有 find_package 均成功，无 NOTFOUND 报错。

- [ ] **Step 3：提交**

```
git add CMakeLists.txt
git commit -m "build: 添加 Protobuf/SQLite/yaml-cpp/Asio 依赖"
```

---

## Task 7：AppConfig 模块 + 单元测试

**Files:**
- Create: `client/src/config/AppConfig.h`
- Create: `client/src/config/AppConfig.cpp`
- Create: `client/tests/config/test_appconfig.cpp`

- [ ] **Step 1：写 client/src/config/AppConfig.h**

```cpp
#pragma once
#include <string>
#include <cstdint>

struct ServerConfig {
    std::string host{"127.0.0.1"};
    uint16_t    port{9527};
    int         heartbeat_interval_sec{30};
    int         reconnect_interval_sec{5};
};

struct LogConfig {
    std::string file{"novachat.log"};
    std::string level{"debug"};
};

// 单例配置对象，从 config.yaml 加载，启动时调用一次 load()
class AppConfig {
public:
    static AppConfig& instance();

    void load(const std::string& config_file);
    void save(const std::string& config_file) const;

    ServerConfig server;
    LogConfig    log;

private:
    AppConfig() = default;
};
```

- [ ] **Step 2：写 client/src/config/AppConfig.cpp**

```cpp
#include "config/AppConfig.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

AppConfig& AppConfig::instance() {
    static AppConfig instance;
    return instance;
}

void AppConfig::load(const std::string& config_file) {
    try {
        YAML::Node cfg = YAML::LoadFile(config_file);

        if (auto s = cfg["server"]) {
            server.host                   = s["host"].as<std::string>(server.host);
            server.port                   = s["port"].as<uint16_t>(server.port);
            server.heartbeat_interval_sec = s["heartbeat_interval_sec"].as<int>(server.heartbeat_interval_sec);
            server.reconnect_interval_sec = s["reconnect_interval_sec"].as<int>(server.reconnect_interval_sec);
        }

        if (auto l = cfg["log"]) {
            log.file  = l["file"].as<std::string>(log.file);
            log.level = l["level"].as<std::string>(log.level);
        }
    } catch (const YAML::Exception&) {
        // 文件不存在或格式错误时使用默认值，不中断启动
    }
}

void AppConfig::save(const std::string& config_file) const {
    YAML::Node cfg;
    cfg["server"]["host"]                   = server.host;
    cfg["server"]["port"]                   = server.port;
    cfg["server"]["heartbeat_interval_sec"] = server.heartbeat_interval_sec;
    cfg["server"]["reconnect_interval_sec"] = server.reconnect_interval_sec;
    cfg["log"]["file"]                      = log.file;
    cfg["log"]["level"]                     = log.level;

    std::ofstream out(config_file);
    out << cfg;
}
```

- [ ] **Step 3：写 client/tests/config/test_appconfig.cpp**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "config/AppConfig.h"
#include <fstream>
#include <cstdio>

TEST_CASE("AppConfig 默认值正确", "[config]") {
    // 重置为默认值（单例在测试间共享）
    AppConfig::instance().server = ServerConfig{};
    AppConfig::instance().log    = LogConfig{};

    CHECK(AppConfig::instance().server.host == "127.0.0.1");
    CHECK(AppConfig::instance().server.port == 9527);
    CHECK(AppConfig::instance().server.heartbeat_interval_sec == 30);
}

TEST_CASE("AppConfig 从 YAML 文件加载覆盖默认值", "[config]") {
    const char* tmp = "test_cfg_load.yaml";
    {
        std::ofstream f(tmp);
        f << "server:\n"
          << "  host: 192.168.1.100\n"
          << "  port: 8080\n";
    }

    AppConfig::instance().load(tmp);
    CHECK(AppConfig::instance().server.host == "192.168.1.100");
    CHECK(AppConfig::instance().server.port == 8080);

    std::remove(tmp);

    // 恢复默认
    AppConfig::instance().server.host = "127.0.0.1";
    AppConfig::instance().server.port = 9527;
}

TEST_CASE("AppConfig 文件不存在时使用默认值不抛异常", "[config]") {
    REQUIRE_NOTHROW(AppConfig::instance().load("nonexistent_config.yaml"));
    CHECK(AppConfig::instance().server.host == "127.0.0.1");
}

TEST_CASE("AppConfig save/load 往返一致", "[config]") {
    const char* tmp = "test_cfg_roundtrip.yaml";
    AppConfig::instance().server.port = 12345;
    AppConfig::instance().save(tmp);

    AppConfig::instance().server.port = 9527;        // 重置
    AppConfig::instance().load(tmp);
    CHECK(AppConfig::instance().server.port == 12345);

    std::remove(tmp);
    AppConfig::instance().server.port = 9527;        // 恢复
}
```

- [ ] **Step 4：把 AppConfig.cpp 加入 NovaChat_Core，把测试加入 NovaChat_Tests**

修改 `client/CMakeLists.txt`，在 NovaChat_Core 的 STATIC 源文件列表中添加：
```cmake
src/config/AppConfig.cpp
```
并添加 yaml-cpp 链接：
```cmake
target_link_libraries(NovaChat_Core PUBLIC
    Qt6::Core
    spdlog::spdlog
    yaml-cpp::yaml-cpp
)
```

修改 `client/tests/CMakeLists.txt`，在 NovaChat_Tests 源文件列表中添加：
```cmake
config/test_appconfig.cpp
```

- [ ] **Step 5：运行测试**

```
cmake --build --preset windows-debug
ctest --preset windows-debug -V
```

预期：4 个 AppConfig 测试全部 PASS（加上之前 2 个 Logger 测试共 6 个）。

- [ ] **Step 6：提交**

```
git add client/src/config/ client/tests/config/ client/CMakeLists.txt client/tests/CMakeLists.txt
git commit -m "feat(config): 添加 AppConfig 模块（yaml-cpp），通过 4 个单元测试"
```

---

## Task 8：DatabaseManager + Schema + 单元测试

**Files:**
- Create: `client/src/storage/DatabaseManager.h`
- Create: `client/src/storage/DatabaseManager.cpp`
- Create: `client/tests/storage/test_database.cpp`

- [ ] **Step 1：写 client/src/storage/DatabaseManager.h**

```cpp
#pragma once
#include <sqlite3.h>
#include <stdexcept>
#include <string>

// 数据库异常
class DatabaseException : public std::runtime_error {
public:
    explicit DatabaseException(const std::string& msg)
        : std::runtime_error("DatabaseException: " + msg) {}
};

// RAII 封装 SQLite 连接，构造时自动建表
// 所有方法必须从 DB 线程调用（调试模式有 assert 保护）
class DatabaseManager {
public:
    explicit DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&)            = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    sqlite3* handle() const noexcept { return db_; }

    // 执行不返回数据的 SQL（DDL / UPDATE / DELETE 等）
    void execute(const std::string& sql);

private:
    sqlite3* db_{nullptr};
    void createSchema();
};
```

- [ ] **Step 2：写 client/src/storage/DatabaseManager.cpp**

```cpp
#include "storage/DatabaseManager.h"

DatabaseManager::DatabaseManager(const std::string& db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = db_ ? sqlite3_errmsg(db_) : "sqlite3_open failed";
        sqlite3_close(db_);
        db_ = nullptr;
        throw DatabaseException(err);
    }
    // WAL 模式提升并发读性能（文件数据库生效，:memory: 回退为 memory 模式）
    execute("PRAGMA journal_mode=WAL");
    execute("PRAGMA foreign_keys=ON");
    createSchema();
}

DatabaseManager::~DatabaseManager() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void DatabaseManager::execute(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "unknown error";
        sqlite3_free(err_msg);
        throw DatabaseException(err);
    }
}

void DatabaseManager::createSchema() {
    execute(R"(
        CREATE TABLE IF NOT EXISTS local_accounts (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            server_id    TEXT    NOT NULL UNIQUE,
            username     TEXT    NOT NULL,
            display_name TEXT    NOT NULL,
            avatar_url   TEXT    NOT NULL DEFAULT '',
            token        TEXT    NOT NULL,
            is_active    INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS contacts (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            owner_id     TEXT    NOT NULL,
            user_id      TEXT    NOT NULL,
            username     TEXT    NOT NULL,
            display_name TEXT    NOT NULL,
            avatar_url   TEXT    NOT NULL DEFAULT '',
            UNIQUE(owner_id, user_id)
        );

        CREATE TABLE IF NOT EXISTS conversations (
            id                   INTEGER PRIMARY KEY AUTOINCREMENT,
            owner_id             TEXT    NOT NULL,
            conversation_id      TEXT    NOT NULL,
            target_id            TEXT    NOT NULL,
            last_message_preview TEXT    NOT NULL DEFAULT '',
            last_message_time    INTEGER NOT NULL DEFAULT 0,
            unread_count         INTEGER NOT NULL DEFAULT 0,
            UNIQUE(owner_id, conversation_id)
        );

        CREATE TABLE IF NOT EXISTS messages (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            message_id      TEXT    NOT NULL UNIQUE,
            conversation_id TEXT    NOT NULL,
            sender_id       TEXT    NOT NULL,
            content         TEXT    NOT NULL,
            msg_type        INTEGER NOT NULL DEFAULT 1,
            timestamp       INTEGER NOT NULL,
            status          INTEGER NOT NULL DEFAULT 0
        );

        CREATE INDEX IF NOT EXISTS idx_messages_conv
            ON messages(conversation_id, timestamp);

        CREATE TABLE IF NOT EXISTS app_config (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )");
}
```

- [ ] **Step 3：写 client/tests/storage/test_database.cpp**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "storage/DatabaseManager.h"
#include <sqlite3.h>
#include <string>

TEST_CASE("DatabaseManager 内存数据库创建 schema 不抛异常", "[db]") {
    REQUIRE_NOTHROW(DatabaseManager(":memory:"));
}

TEST_CASE("DatabaseManager 无效 SQL 抛出 DatabaseException", "[db]") {
    DatabaseManager db(":memory:");
    REQUIRE_THROWS_AS(db.execute("THIS IS NOT SQL"), DatabaseException);
}

TEST_CASE("DatabaseManager 所有预期表均已创建", "[db]") {
    DatabaseManager db(":memory:");

    auto table_exists = [&](const std::string& name) -> bool {
        sqlite3_stmt* stmt;
        const std::string sql =
            "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='" + name + "'";
        sqlite3_prepare_v2(db.handle(), sql.c_str(), -1, &stmt, nullptr);
        sqlite3_step(stmt);
        int count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return count > 0;
    };

    CHECK(table_exists("local_accounts"));
    CHECK(table_exists("contacts"));
    CHECK(table_exists("conversations"));
    CHECK(table_exists("messages"));
    CHECK(table_exists("app_config"));
}

TEST_CASE("DatabaseManager messages 表 idx_messages_conv 索引存在", "[db]") {
    DatabaseManager db(":memory:");

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db.handle(),
        "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name='idx_messages_conv'",
        -1, &stmt, nullptr);
    sqlite3_step(stmt);
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    CHECK(count == 1);
}

TEST_CASE("DatabaseManager 可重复打开同一内存路径（幂等 schema）", "[db]") {
    // 两次构造均不应抛异常（IF NOT EXISTS 保证幂等）
    REQUIRE_NOTHROW(DatabaseManager(":memory:"));
    REQUIRE_NOTHROW(DatabaseManager(":memory:"));
}
```

- [ ] **Step 4：把 DatabaseManager.cpp 加入 NovaChat_Core，添加 SQLite3 链接，测试文件加入 NovaChat_Tests**

修改 `client/CMakeLists.txt`：
```cmake
# NovaChat_Core 源文件列表追加
src/storage/DatabaseManager.cpp

# 链接追加
target_link_libraries(NovaChat_Core PUBLIC
    ...
    SQLite::SQLite3
)
```

修改 `client/tests/CMakeLists.txt`：
```cmake
storage/test_database.cpp
```

- [ ] **Step 5：运行测试**

```
cmake --build --preset windows-debug
ctest --preset windows-debug -V
```

预期：5 个 DB 测试 PASS，累计 11 个测试全部 PASS。

- [ ] **Step 6：提交**

```
git add client/src/storage/DatabaseManager.h client/src/storage/DatabaseManager.cpp
git add client/tests/storage/ client/CMakeLists.txt client/tests/CMakeLists.txt
git commit -m "feat(storage): 添加 DatabaseManager，WAL 模式，建表 schema，5 个单元测试通过"
```

---

## Task 9：定义 Proto 文件 + CMake 集成

**Files:**
- Create: `proto/CMakeLists.txt`
- Create: `proto/MessageType.h`
- Create: `proto/ProtocolCodec.h`
- Create: `proto/ProtocolCodec.cpp`
- Create: `proto/common.proto`
- Create: `proto/auth.proto`
- Create: `proto/contact.proto`
- Create: `proto/chat.proto`

- [ ] **Step 1：写 proto/MessageType.h**

```cpp
#pragma once
#include <cstdint>

// 二进制帧头中的消息类型字段（MsgId，2字节）
enum class MessageType : uint16_t {
    // 0x00xx  Auth
    RegisterRequest     = 0x0001,
    RegisterResponse    = 0x0002,
    LoginRequest        = 0x0003,
    LoginResponse       = 0x0004,
    LogoutRequest       = 0x0005,

    // 0x01xx  Contact
    GetContactsRequest   = 0x0101,
    GetContactsResponse  = 0x0102,
    AddFriendRequest     = 0x0103,
    AddFriendResponse    = 0x0104,
    DeleteFriendRequest  = 0x0105,
    DeleteFriendResponse = 0x0106,
    SearchUserRequest    = 0x0107,
    SearchUserResponse   = 0x0108,

    // 0x02xx  Chat
    SendMessageRequest       = 0x0201,
    SendMessageResponse      = 0x0202,
    MessagePush              = 0x0203,  // 服务端主动推送
    GetHistoryRequest        = 0x0204,
    GetHistoryResponse       = 0x0205,
    GetConversationsRequest  = 0x0206,
    GetConversationsResponse = 0x0207,

    // 0x0Fxx  System
    HeartbeatRequest  = 0x0F01,
    HeartbeatResponse = 0x0F02,
    ErrorResponse     = 0x0FFF,
};
```

- [ ] **Step 2：写 proto/ProtocolCodec.h**

```cpp
#pragma once
#include "proto/MessageType.h"
#include <cstdint>
#include <functional>
#include <vector>

// 二进制帧格式（固定 12 字节头 + 变长 Payload）
// ┌─────────┬────────────┬──────────┬──────────┬─────────────────┐
// │Magic(2B)│Payload(4B) │MsgId(2B) │SeqId(4B) │Payload(nB)      │
// │ 0x4E43  │  length    │  type    │ seq num  │ Protobuf bytes  │
// └─────────┴────────────┴──────────┴──────────┴─────────────────┘

struct Packet {
    MessageType          type{MessageType::HeartbeatRequest};
    uint32_t             seq_id{0};
    std::vector<uint8_t> payload;
};

class ProtocolCodec {
public:
    static constexpr uint16_t MAGIC       = 0x4E43;  // "NC"
    static constexpr size_t   HEADER_SIZE = 12;

    using PacketCallback = std::function<void(Packet)>;

    // 将 Packet 序列化为可直接发送的字节流（大端序）
    static std::vector<uint8_t> encode(const Packet& packet);

    // 将收到的字节喂入内部缓冲区，解码出完整帧后调用 callback
    // 同一 ProtocolCodec 实例只在单线程使用
    void feed(const uint8_t* data, size_t len, const PacketCallback& callback);

private:
    std::vector<uint8_t> buffer_;
};

// 注册为 Qt 元类型，以便跨线程信号传递
#include <QMetaType>
Q_DECLARE_METATYPE(Packet)
```

- [ ] **Step 3：写 proto/ProtocolCodec.cpp**

```cpp
#include "proto/ProtocolCodec.h"
#include <cstring>

// ── 字节序工具函数 ─────────────────────────────────────────────
static uint32_t to_be32(uint32_t v) {
    return ((v & 0xFFU) << 24) | (((v >> 8) & 0xFFU) << 16) |
           (((v >> 16) & 0xFFU) << 8) | ((v >> 24) & 0xFFU);
}

static uint16_t to_be16(uint16_t v) {
    return static_cast<uint16_t>(((v & 0xFFU) << 8) | ((v >> 8) & 0xFFU));
}

static uint32_t from_be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
            static_cast<uint32_t>(p[3]);
}

static uint16_t from_be16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) |
                                   static_cast<uint16_t>(p[1]));
}

// ── 编码 ──────────────────────────────────────────────────────
std::vector<uint8_t> ProtocolCodec::encode(const Packet& packet) {
    const size_t total = HEADER_SIZE + packet.payload.size();
    std::vector<uint8_t> out(total);
    uint8_t* p = out.data();

    const uint16_t magic  = to_be16(MAGIC);
    const uint32_t length = to_be32(static_cast<uint32_t>(packet.payload.size()));
    const uint16_t msg_id = to_be16(static_cast<uint16_t>(packet.type));
    const uint32_t seq_id = to_be32(packet.seq_id);

    std::memcpy(p, &magic,  2); p += 2;
    std::memcpy(p, &length, 4); p += 4;
    std::memcpy(p, &msg_id, 2); p += 2;
    std::memcpy(p, &seq_id, 4); p += 4;

    if (!packet.payload.empty()) {
        std::memcpy(p, packet.payload.data(), packet.payload.size());
    }
    return out;
}

// ── 解码 ──────────────────────────────────────────────────────
void ProtocolCodec::feed(const uint8_t* data, size_t len, const PacketCallback& callback) {
    buffer_.insert(buffer_.end(), data, data + len);

    while (buffer_.size() >= HEADER_SIZE) {
        const uint8_t* h = buffer_.data();

        // 校验 Magic
        if (from_be16(h) != MAGIC) {
            buffer_.clear();  // 帧同步丢失，清空缓冲区
            return;
        }

        const uint32_t payload_len = from_be32(h + 2);
        const size_t   total       = HEADER_SIZE + payload_len;

        if (buffer_.size() < total) {
            break;  // 等待更多数据
        }

        Packet pkt;
        pkt.type   = static_cast<MessageType>(from_be16(h + 6));
        pkt.seq_id = from_be32(h + 8);
        if (payload_len > 0) {
            pkt.payload.assign(h + HEADER_SIZE, h + total);
        }

        buffer_.erase(buffer_.begin(),
                      buffer_.begin() + static_cast<ptrdiff_t>(total));
        callback(std::move(pkt));
    }
}
```

- [ ] **Step 4：写 proto/common.proto**

```protobuf
syntax = "proto3";
package novachat;

enum ErrorCode {
    SUCCESS              = 0;
    UNKNOWN_ERROR        = 1;
    AUTH_FAILED          = 2;
    AUTH_EXPIRED         = 3;
    USER_NOT_FOUND       = 4;
    USER_ALREADY_EXISTS  = 5;
    FRIEND_NOT_FOUND     = 6;
    FRIEND_ALREADY_EXISTS = 7;
    MESSAGE_FAILED       = 8;
}

message ErrorResponse {
    ErrorCode code    = 1;
    string    message = 2;
}
```

- [ ] **Step 5：写 proto/auth.proto**

```protobuf
syntax = "proto3";
package novachat;

message RegisterRequest {
    string username     = 1;
    string password     = 2;
    string display_name = 3;
}

message RegisterResponse {
    string user_id      = 1;
    string display_name = 2;
}

message LoginRequest {
    string username = 1;
    string password = 2;
}

message LoginResponse {
    string token        = 1;
    string user_id      = 2;
    string display_name = 3;
    string avatar_url   = 4;
}

message LogoutRequest {
    string token = 1;
}
```

- [ ] **Step 6：写 proto/contact.proto**

```protobuf
syntax = "proto3";
package novachat;

message ContactInfo {
    string user_id      = 1;
    string username     = 2;
    string display_name = 3;
    string avatar_url   = 4;
}

message GetContactsRequest  { string token = 1; }
message GetContactsResponse { repeated ContactInfo contacts = 1; }

message AddFriendRequest    { string token = 1; string target_user_id = 2; }
message AddFriendResponse   { ContactInfo contact = 1; }

message DeleteFriendRequest  { string token = 1; string target_user_id = 2; }
message DeleteFriendResponse {}

message SearchUserRequest   { string token = 1; string keyword = 2; }
message SearchUserResponse  { repeated ContactInfo users = 1; }
```

- [ ] **Step 7：写 proto/chat.proto**

```protobuf
syntax = "proto3";
package novachat;

enum MessageStatusProto {
    MSG_SENDING = 0;
    MSG_SENT    = 1;
    MSG_FAILED  = 2;
}

message MessageInfo {
    string            message_id      = 1;
    string            conversation_id = 2;
    string            sender_id       = 3;
    string            content         = 4;
    int64             timestamp       = 5;
    MessageStatusProto status         = 6;
}

message ConversationInfo {
    string conversation_id      = 1;
    string target_id            = 2;
    string target_display_name  = 3;
    string last_message_preview = 4;
    int64  last_message_time    = 5;
    int32  unread_count         = 6;
}

message SendMessageRequest {
    string token           = 1;
    string conversation_id = 2;
    string receiver_id     = 3;
    string content         = 4;
}

message SendMessageResponse {
    string message_id = 1;
    int64  timestamp  = 2;
}

message MessagePush { MessageInfo message = 1; }

message GetHistoryRequest {
    string token            = 1;
    string conversation_id  = 2;
    int64  before_timestamp = 3;  // 分页：取该时间戳之前的消息
    int32  limit            = 4;  // 每页条数，推荐 50
}

message GetHistoryResponse {
    repeated MessageInfo messages = 1;
    bool has_more = 2;
}

message GetConversationsRequest  { string token = 1; }
message GetConversationsResponse { repeated ConversationInfo conversations = 1; }
```

- [ ] **Step 8：写 proto/CMakeLists.txt**

```cmake
# proto/CMakeLists.txt
# 产出 NovaChat_Proto 静态库：Protobuf 生成代码 + ProtocolCodec
# 客户端和服务端均链接此库

set(PROTO_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/common.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/auth.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/contact.proto
    ${CMAKE_CURRENT_SOURCE_DIR}/chat.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

add_library(NovaChat_Proto STATIC
    ${PROTO_SRCS}
    ProtocolCodec.cpp
)

target_include_directories(NovaChat_Proto PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}   # MessageType.h / ProtocolCodec.h
    ${CMAKE_CURRENT_BINARY_DIR}   # 生成的 .pb.h 文件
)

target_link_libraries(NovaChat_Proto PUBLIC
    protobuf::libprotobuf
    Qt6::Core   # Q_DECLARE_METATYPE 需要 QMetaType
)
```

- [ ] **Step 9：更新 client/CMakeLists.txt，链接 NovaChat_Proto**

在 `NovaChat_Core` 的 `target_link_libraries` 中追加：
```cmake
NovaChat_Proto
asio::asio
```

- [ ] **Step 10：编译验证（含 Protobuf 代码生成）**

```
cmake --preset windows-debug
cmake --build --preset windows-debug
```

预期：proto 目录下生成 `common.pb.h/cc`、`auth.pb.h/cc`、`contact.pb.h/cc`、`chat.pb.h/cc`，编译无报错。

- [ ] **Step 11：提交**

```
git add proto/ client/CMakeLists.txt
git commit -m "feat(proto): 添加 Protobuf 消息定义、MessageType 枚举、ProtocolCodec 编解码库"
```

---

## Task 10：ProtocolCodec 单元测试

**Files:**
- Create: `client/tests/network/test_codec.cpp`

- [ ] **Step 1：写 client/tests/network/test_codec.cpp**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "proto/ProtocolCodec.h"

// ── 辅助：构造带 payload 的 Packet ───────────────────────────
static Packet make_packet(MessageType type, uint32_t seq,
                           std::vector<uint8_t> payload = {}) {
    Packet p;
    p.type    = type;
    p.seq_id  = seq;
    p.payload = std::move(payload);
    return p;
}

// ── 基本往返 ──────────────────────────────────────────────────
TEST_CASE("ProtocolCodec encode/decode 往返一致", "[codec]") {
    auto original = make_packet(MessageType::HeartbeatRequest, 42, {0x01, 0x02, 0x03});
    auto encoded  = ProtocolCodec::encode(original);

    ProtocolCodec codec;
    std::vector<Packet> decoded;
    codec.feed(encoded.data(), encoded.size(),
               [&decoded](Packet p) { decoded.push_back(std::move(p)); });

    REQUIRE(decoded.size() == 1);
    CHECK(decoded[0].type    == original.type);
    CHECK(decoded[0].seq_id  == original.seq_id);
    CHECK(decoded[0].payload == original.payload);
}

TEST_CASE("ProtocolCodec 空 payload 的包正常编解码", "[codec]") {
    auto original = make_packet(MessageType::HeartbeatResponse, 0);
    auto encoded  = ProtocolCodec::encode(original);

    CHECK(encoded.size() == ProtocolCodec::HEADER_SIZE);

    ProtocolCodec codec;
    std::vector<Packet> decoded;
    codec.feed(encoded.data(), encoded.size(),
               [&decoded](Packet p) { decoded.push_back(std::move(p)); });

    REQUIRE(decoded.size() == 1);
    CHECK(decoded[0].payload.empty());
}

// ── 分包处理 ─────────────────────────────────────────────────
TEST_CASE("ProtocolCodec 逐字节喂入（极端分包）", "[codec]") {
    auto original = make_packet(MessageType::LoginRequest, 7, {0xAB, 0xCD});
    auto encoded  = ProtocolCodec::encode(original);

    ProtocolCodec codec;
    std::vector<Packet> decoded;
    auto cb = [&decoded](Packet p) { decoded.push_back(std::move(p)); };

    for (size_t i = 0; i < encoded.size(); ++i) {
        codec.feed(&encoded[i], 1, cb);
        // 只有喂完最后一个字节才应该解出一个包
        if (i < encoded.size() - 1) {
            CHECK(decoded.empty());
        }
    }
    REQUIRE(decoded.size() == 1);
    CHECK(decoded[0].seq_id == 7);
}

// ── 粘包处理 ─────────────────────────────────────────────────
TEST_CASE("ProtocolCodec 两个包连续喂入（粘包）", "[codec]") {
    auto p1 = make_packet(MessageType::HeartbeatRequest,  1);
    auto p2 = make_packet(MessageType::HeartbeatResponse, 2);

    auto enc1 = ProtocolCodec::encode(p1);
    auto enc2 = ProtocolCodec::encode(p2);

    std::vector<uint8_t> combined;
    combined.insert(combined.end(), enc1.begin(), enc1.end());
    combined.insert(combined.end(), enc2.begin(), enc2.end());

    ProtocolCodec codec;
    std::vector<Packet> decoded;
    codec.feed(combined.data(), combined.size(),
               [&decoded](Packet p) { decoded.push_back(std::move(p)); });

    REQUIRE(decoded.size() == 2);
    CHECK(decoded[0].seq_id == 1);
    CHECK(decoded[1].seq_id == 2);
}

// ── Magic 校验 ───────────────────────────────────────────────
TEST_CASE("ProtocolCodec 非法 Magic 导致缓冲区清空，不解出任何包", "[codec]") {
    // 构造一个 Magic 错误的帧（0xFFFF 而不是 0x4E43）
    std::vector<uint8_t> bad = {
        0xFF, 0xFF,              // wrong magic
        0x00, 0x00, 0x00, 0x00, // length = 0
        0x0F, 0x01,              // HeartbeatRequest
        0x00, 0x00, 0x00, 0x01  // seq_id = 1
    };

    ProtocolCodec codec;
    std::vector<Packet> decoded;
    codec.feed(bad.data(), bad.size(),
               [&decoded](Packet p) { decoded.push_back(std::move(p)); });

    CHECK(decoded.empty());
}
```

- [ ] **Step 2：把测试加入 NovaChat_Tests，并链接 NovaChat_Proto**

修改 `client/tests/CMakeLists.txt`：
```cmake
add_executable(NovaChat_Tests
    logger/test_logger.cpp
    config/test_appconfig.cpp
    storage/test_database.cpp
    network/test_codec.cpp    # ← 新增
)
target_link_libraries(NovaChat_Tests PRIVATE
    NovaChat_Core
    NovaChat_Proto            # ← 新增（ProtocolCodec 在 Proto 库中）
    Catch2::Catch2WithMain
)
```

- [ ] **Step 3：运行测试**

```
cmake --build --preset windows-debug
ctest --preset windows-debug -V
```

预期：6 个 Codec 测试 PASS，累计 17 个测试全部 PASS。

- [ ] **Step 4：提交**

```
git add client/tests/network/ client/tests/CMakeLists.txt
git commit -m "test(codec): ProtocolCodec 6 个单元测试（往返/分包/粘包/Magic 校验）全部通过"
```

---

## Task 11：服务端 TcpServer + ClientSession

**Files:**
- Create: `server/network/TcpServer.h`
- Create: `server/network/TcpServer.cpp`
- Create: `server/network/ClientSession.h`
- Create: `server/network/ClientSession.cpp`
- Modify: `server/CMakeLists.txt`
- Modify: `server/src/main.cpp`

- [ ] **Step 1：写 server/network/ClientSession.h**

```cpp
#pragma once
#include "proto/ProtocolCodec.h"
#include <asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    using PacketHandler =
        std::function<void(std::shared_ptr<ClientSession>, Packet)>;

    ClientSession(asio::ip::tcp::socket socket, PacketHandler handler);

    void start();
    void sendPacket(Packet packet);

    const std::string& userId() const { return user_id_; }
    void setUserId(const std::string& id) { user_id_ = id; }

private:
    void doRead();

    asio::ip::tcp::socket socket_;
    PacketHandler         handler_;
    ProtocolCodec         codec_;
    std::vector<uint8_t>  read_buf_;
    std::string           user_id_;

    static constexpr size_t READ_BUF_SIZE = 65536;
};
```

- [ ] **Step 2：写 server/network/ClientSession.cpp**

```cpp
#include "network/ClientSession.h"
#include <spdlog/spdlog.h>

ClientSession::ClientSession(asio::ip::tcp::socket socket,
                             PacketHandler         handler)
    : socket_(std::move(socket))
    , handler_(std::move(handler))
    , read_buf_(READ_BUF_SIZE)
{}

void ClientSession::start() {
    doRead();
}

void ClientSession::sendPacket(Packet packet) {
    auto data = std::make_shared<std::vector<uint8_t>>(
        ProtocolCodec::encode(packet));

    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(*data),
        [self, data](const asio::error_code& ec, size_t /*bytes*/) {
            if (ec) {
                spdlog::warn("ClientSession send error: {}", ec.message());
            }
        });
}

void ClientSession::doRead() {
    auto self = shared_from_this();
    socket_.async_read_some(asio::buffer(read_buf_),
        [self](const asio::error_code& ec, size_t bytes) {
            if (ec) {
                spdlog::info("Client disconnected: {}", ec.message());
                return;  // session 自然结束（shared_ptr 引用归零）
            }
            self->codec_.feed(self->read_buf_.data(), bytes,
                [self](Packet pkt) {
                    self->handler_(self, std::move(pkt));
                });
            self->doRead();
        });
}
```

- [ ] **Step 3：写 server/network/TcpServer.h**

```cpp
#pragma once
#include "network/ClientSession.h"
#include <asio.hpp>
#include <cstdint>

class TcpServer {
public:
    TcpServer(asio::io_context&            io_ctx,
              uint16_t                     port,
              ClientSession::PacketHandler handler);

    void start();

private:
    void doAccept();

    asio::io_context&            io_ctx_;
    asio::ip::tcp::acceptor      acceptor_;
    ClientSession::PacketHandler handler_;
};
```

- [ ] **Step 4：写 server/network/TcpServer.cpp**

```cpp
#include "network/TcpServer.h"
#include <spdlog/spdlog.h>

TcpServer::TcpServer(asio::io_context&            io_ctx,
                     uint16_t                     port,
                     ClientSession::PacketHandler handler)
    : io_ctx_(io_ctx)
    , acceptor_(io_ctx,
                asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    , handler_(std::move(handler))
{}

void TcpServer::start() {
    spdlog::info("TcpServer listening on port {}",
                 acceptor_.local_endpoint().port());
    doAccept();
}

void TcpServer::doAccept() {
    acceptor_.async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                spdlog::info("New connection from {}",
                             socket.remote_endpoint().address().to_string());
                auto session = std::make_shared<ClientSession>(
                    std::move(socket), handler_);
                session->start();
            } else {
                spdlog::error("Accept error: {}", ec.message());
            }
            doAccept();  // 持续监听
        });
}
```

- [ ] **Step 5：更新 server/CMakeLists.txt**

```cmake
find_package(spdlog CONFIG REQUIRED)
find_package(asio   CONFIG REQUIRED)

add_executable(NovaChatServer
    src/main.cpp
    src/network/TcpServer.cpp
    src/network/ClientSession.cpp
)

target_include_directories(NovaChatServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(NovaChatServer PRIVATE
    spdlog::spdlog
    asio::asio
    NovaChat_Proto
)
```

- [ ] **Step 6：更新 server/src/main.cpp（接入真实 TcpServer + Heartbeat 处理）**

```cpp
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "network/TcpServer.h"
#include "proto/MessageType.h"
#include <asio.hpp>
#include <atomic>
#include <csignal>
#include <memory>
#include <thread>

namespace {
    std::atomic<bool>      g_running{true};
    asio::io_context*      g_io_ctx_ptr{nullptr};
}

static void signal_handler(int) {
    g_running = false;
    if (g_io_ctx_ptr) g_io_ctx_ptr->stop();
}

int main() {
    auto logger = spdlog::stdout_color_mt("server");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    asio::io_context io_ctx;
    g_io_ctx_ptr = &io_ctx;

    // 数据包处理器：阶段二仅处理 Heartbeat
    auto handler = [](std::shared_ptr<ClientSession> session, Packet pkt) {
        if (pkt.type == MessageType::HeartbeatRequest) {
            spdlog::debug("Heartbeat from user '{}'", session->userId());
            Packet resp;
            resp.type   = MessageType::HeartbeatResponse;
            resp.seq_id = pkt.seq_id;
            session->sendPacket(std::move(resp));
        } else {
            spdlog::warn("Unknown packet type: 0x{:04X}",
                         static_cast<uint16_t>(pkt.type));
        }
    };

    TcpServer server(io_ctx, 9527, handler);
    server.start();

    spdlog::info("NovaChatServer v0.1.0 ready");
    io_ctx.run();

    spdlog::info("Server stopped");
    spdlog::shutdown();
    return 0;
}
```

- [ ] **Step 7：编译验证**

```
cmake --build --preset windows-debug
```

预期：NovaChatServer.exe 编译无报错。

- [ ] **Step 8：提交**

```
git add server/
git commit -m "feat(server): 添加 TcpServer + ClientSession，处理 Heartbeat 请求"
```

---

## Task 12：客户端 TcpClient + 端到端心跳验证

**Files:**
- Create: `client/src/network/TcpClient.h`
- Create: `client/src/network/TcpClient.cpp`
- Modify: `client/CMakeLists.txt`

- [ ] **Step 1：写 client/src/network/TcpClient.h**

```cpp
#pragma once
#include "proto/ProtocolCodec.h"
#include <QObject>
#include <asio.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

// 异步 TCP 客户端
// · Asio io_context 运行在独立线程
// · 收到数据包后通过 Qt QueuedConnection 信号投递到主线程
// · 调用方只需在主线程调用 connectToServer / sendPacket / disconnect
class TcpClient : public QObject {
    Q_OBJECT

public:
    using ResponseCallback = std::function<void(Packet)>;

    explicit TcpClient(QObject* parent = nullptr);
    ~TcpClient() override;

    // 以下方法均从主线程调用
    void connectToServer(const std::string& host, uint16_t port);
    void disconnect();

    // fire-and-forget 发送
    void sendPacket(Packet packet);

    // 带响应回调的发送（回调在主线程执行）
    void sendPacket(Packet packet, ResponseCallback callback);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
    // 未注册 callback 的服务端主动推送包通过此信号分发
    void packetReceived(Packet packet);

private:
    void doRead();
    void scheduleHeartbeat();
    void handlePacket(Packet packet);
    uint32_t nextSeqId();

    asio::io_context                                             io_ctx_;
    asio::executor_work_guard<asio::io_context::executor_type>  work_guard_;
    asio::ip::tcp::socket                                        socket_;
    std::unique_ptr<asio::steady_timer>                          heartbeat_timer_;
    std::thread                                                  io_thread_;

    ProtocolCodec            codec_;
    std::vector<uint8_t>     read_buf_;
    static constexpr size_t  READ_BUF_SIZE = 65536;

    std::atomic<uint32_t>                                 seq_counter_{0};
    std::mutex                                            callbacks_mutex_;
    std::unordered_map<uint32_t, ResponseCallback>        callbacks_;
};
```

- [ ] **Step 2：写 client/src/network/TcpClient.cpp**

```cpp
#include "network/TcpClient.h"
#include "config/AppConfig.h"
#include "logger/Logger.h"
#include <QMetaObject>
#include <chrono>

TcpClient::TcpClient(QObject* parent)
    : QObject(parent)
    , work_guard_(asio::make_work_guard(io_ctx_))
    , socket_(io_ctx_)
    , read_buf_(READ_BUF_SIZE)
{
    // 注册 Packet 为 Qt 元类型，保证跨线程信号正常传递
    qRegisterMetaType<Packet>("Packet");
    io_thread_ = std::thread([this] { io_ctx_.run(); });
}

TcpClient::~TcpClient() {
    disconnect();
    work_guard_.reset();
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

uint32_t TcpClient::nextSeqId() {
    return ++seq_counter_;
}

void TcpClient::connectToServer(const std::string& host, uint16_t port) {
    asio::post(io_ctx_, [this, host, port] {
        auto resolver = std::make_shared<asio::ip::tcp::resolver>(io_ctx_);
        resolver->async_resolve(host, std::to_string(port),
            [this, resolver](const asio::error_code& ec, auto endpoints) {
                if (ec) {
                    emit errorOccurred(QString::fromStdString(ec.message()));
                    return;
                }
                asio::async_connect(socket_, endpoints,
                    [this](const asio::error_code& ec2,
                           const asio::ip::tcp::endpoint&) {
                        if (ec2) {
                            emit errorOccurred(
                                QString::fromStdString(ec2.message()));
                            return;
                        }
                        Logger::info("Connected to server");
                        emit connected();
                        scheduleHeartbeat();
                        doRead();
                    });
            });
    });
}

void TcpClient::disconnect() {
    asio::post(io_ctx_, [this] {
        asio::error_code ec;
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
        if (heartbeat_timer_) {
            heartbeat_timer_->cancel();
        }
    });
}

void TcpClient::sendPacket(Packet packet) {
    auto data = std::make_shared<std::vector<uint8_t>>(
        ProtocolCodec::encode(packet));
    asio::post(io_ctx_, [this, data] {
        asio::async_write(socket_, asio::buffer(*data),
            [data](const asio::error_code& ec, size_t /*bytes*/) {
                if (ec) {
                    spdlog::error("TcpClient send error: {}", ec.message());
                }
            });
    });
}

void TcpClient::sendPacket(Packet packet, ResponseCallback callback) {
    const uint32_t seq = nextSeqId();
    packet.seq_id = seq;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        callbacks_[seq] = std::move(callback);
    }
    sendPacket(std::move(packet));
}

void TcpClient::doRead() {
    socket_.async_read_some(asio::buffer(read_buf_),
        [this](const asio::error_code& ec, size_t bytes) {
            if (ec) {
                if (ec != asio::error::operation_aborted) {
                    Logger::warn("TcpClient read error: {}", ec.message());
                    emit disconnected();
                }
                return;
            }
            codec_.feed(read_buf_.data(), bytes,
                [this](Packet pkt) { handlePacket(std::move(pkt)); });
            doRead();
        });
}

void TcpClient::scheduleHeartbeat() {
    const int interval =
        AppConfig::instance().server.heartbeat_interval_sec;

    heartbeat_timer_ = std::make_unique<asio::steady_timer>(io_ctx_);
    heartbeat_timer_->expires_after(std::chrono::seconds(interval));
    heartbeat_timer_->async_wait([this](const asio::error_code& ec) {
        if (ec) return;
        Packet hb;
        hb.type   = MessageType::HeartbeatRequest;
        hb.seq_id = nextSeqId();
        sendPacket(hb);
        Logger::debug("Heartbeat sent");
        scheduleHeartbeat();  // 重新调度
    });
}

void TcpClient::handlePacket(Packet packet) {
    // 优先查找 seq_id 对应的一次性回调
    ResponseCallback cb;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        auto it = callbacks_.find(packet.seq_id);
        if (it != callbacks_.end()) {
            cb = std::move(it->second);
            callbacks_.erase(it);
        }
    }

    if (cb) {
        // 将回调投递到主线程执行
        Packet pkt_copy = packet;
        QMetaObject::invokeMethod(this,
            [cb = std::move(cb), pkt = std::move(pkt_copy)]() mutable {
                cb(std::move(pkt));
            }, Qt::QueuedConnection);
    } else {
        // 无回调的包（如服务端主动推送）通过信号分发到主线程
        emit packetReceived(packet);
    }
}
```

- [ ] **Step 3：更新 client/CMakeLists.txt，把 TcpClient.cpp 加入 NovaChat_Core**

```cmake
add_library(NovaChat_Core STATIC
    src/logger/Logger.cpp
    src/config/AppConfig.cpp
    src/storage/DatabaseManager.cpp
    src/network/TcpClient.cpp      # ← 新增
)
target_link_libraries(NovaChat_Core PUBLIC
    Qt6::Core
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    SQLite::SQLite3
    asio::asio
    NovaChat_Proto
)
```

- [ ] **Step 4：编译确认无错**

```
cmake --build --preset windows-debug
```

预期：编译无报错。

- [ ] **Step 5：端到端心跳验证**

开两个终端窗口：

**终端 1 — 启动服务端：**
```
./build/debug/server/NovaChatServer.exe
```

预期输出：
```
[...] [info] TcpServer listening on port 9527
[...] [info] NovaChatServer v0.1.0 ready
```

**终端 2 — 临时修改 main.cpp 连接服务端并打印日志：**

在 `client/src/main.cpp` 的 `window.show()` 前追加：
```cpp
#include "network/TcpClient.h"
// ...
auto* client = new TcpClient(&app);
QObject::connect(client, &TcpClient::connected, [] {
    Logger::info("端到端测试：已连接到服务端");
});
QObject::connect(client, &TcpClient::packetReceived, [](const Packet& pkt) {
    Logger::info("收到包：type=0x{:04X} seq={}",
                 static_cast<uint16_t>(pkt.type), pkt.seq_id);
});
AppConfig::instance().server.heartbeat_interval_sec = 3; // 测试用缩短间隔
client->connectToServer("127.0.0.1", 9527);
```

运行客户端：
```
./build/debug/client/NovaChat.exe
```

预期客户端日志：
```
[...] [info] 端到端测试：已连接到服务端
[...] [debug] Heartbeat sent
[...] [info] 收到包：type=0x0F02 seq=1    ← HeartbeatResponse
```

预期服务端日志：
```
[...] [info] New connection from 127.0.0.1
[...] [debug] Heartbeat from user ''
```

- [ ] **Step 6：还原 main.cpp（移除测试代码），恢复 heartbeat 间隔为默认**

还原 `client/src/main.cpp` 到 Task 3 Step 3 的内容（不含 TcpClient 测试代码），确保应用正常打开空白窗口。

- [ ] **Step 7：最终全量测试**

```
cmake --build --preset windows-debug
ctest --preset windows-debug -V
```

预期：17 个测试全部 PASS。

- [ ] **Step 8：提交**

```
git add client/src/network/ client/CMakeLists.txt client/src/main.cpp
git commit -m "feat(network): 添加 TcpClient（Asio 异步），端到端心跳验证通过"
```

---

## 阶段二完成标准

- [ ] `ctest --preset windows-debug` 17 个测试全部 PASS
- [ ] 编译无错误无警告
- [ ] 服务端与客户端端到端连接，HeartbeatRequest/Response 收发正常（日志可见）
- [ ] 所有代码已提交到 git
