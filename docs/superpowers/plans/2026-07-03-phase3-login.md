# NovaChat V1 — 阶段三实现计划：登录模块

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成完整的注册/登录/自动登录/退出登录流程：服务端存储用户并生成 Token，客户端完成 LoginWindow UI、AuthService 业务逻辑、UserRepository 持久化，启动时自动检测本地 Token 并跳过登录界面。

**Architecture:** AuthService 作为登录业务的协调者，持有 TcpClient*（网络）和 UserRepository&（持久化）。登录成功后将 Token 写入 SQLite；下次启动时从 DB 读取活跃账号直接进入 MainWindow，无需重新输入密码。MainWindow 在本阶段增加用户名显示和退出登录入口。

**Tech Stack:** C++20 · Qt6 Widgets · Protobuf（auth.proto / common.proto）· SQLite · spdlog

---

## 本阶段新增/修改文件

```
NovaChat/
├── server/
│   ├── CMakeLists.txt                   ← 修改：添加 handler/ storage/ 子目录
│   ├── src/
│   │   ├── main.cpp                     ← 修改：路由 Auth 包到 AuthHandler
│   │   ├── handler/
│   │   │   ├── AuthHandler.h            ← 新增
│   │   │   └── AuthHandler.cpp          ← 新增
│   │   └── storage/
│   │       ├── ServerStorage.h          ← 新增
│   │       └── ServerStorage.cpp        ← 新增
└── client/
    ├── CMakeLists.txt                   ← 修改：添加新模块
    ├── src/
    │   ├── main.cpp                     ← 修改：自动登录逻辑
    │   ├── storage/
    │   │   ├── UserRepository.h         ← 新增
    │   │   └── UserRepository.cpp       ← 新增
    │   ├── service/
    │   │   ├── AuthService.h            ← 新增
    │   │   └── AuthService.cpp          ← 新增
    │   └── ui/
    │       ├── MainWindow.h             ← 修改：接受 User 参数，添加退出登录
    │       ├── MainWindow.cpp           ← 修改
    │       ├── LoginWindow.h            ← 新增
    │       ├── LoginWindow.cpp          ← 新增
    │       ├── RegisterDialog.h         ← 新增
    │       └── RegisterDialog.cpp       ← 新增
    └── tests/
        ├── CMakeLists.txt               ← 修改：添加新测试
        └── storage/
            └── test_user_repository.cpp ← 新增
```

---

## Task 13：服务端 ServerStorage + AuthHandler

**Files:**
- Create: `server/src/storage/ServerStorage.h`
- Create: `server/src/storage/ServerStorage.cpp`
- Create: `server/src/handler/AuthHandler.h`
- Create: `server/src/handler/AuthHandler.cpp`
- Modify: `server/CMakeLists.txt`
- Modify: `server/src/main.cpp`

- [ ] **Step 1：写 server/src/storage/ServerStorage.h**

```cpp
#pragma once
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

// 服务端内存用户存储（测试用，重启后数据丢失）
// 所有方法线程安全
class ServerStorage {
public:
    struct UserRecord {
        std::string user_id;
        std::string username;
        std::string display_name;
        std::string password;   // 测试服务端存明文，生产环境应存哈希
    };

    // 注册新用户，用户名已存在则返回 std::nullopt
    std::optional<UserRecord> registerUser(const std::string& username,
                                           const std::string& password,
                                           const std::string& display_name);

    // 验证用户名/密码，成功返回 UserRecord
    std::optional<UserRecord> authenticate(const std::string& username,
                                           const std::string& password);

    // 为已认证用户创建会话 Token
    std::string createSession(const std::string& user_id);

    // 通过 Token 查找用户（用于后续请求鉴权）
    std::optional<UserRecord> findByToken(const std::string& token);

    // 注销会话
    void removeSession(const std::string& token);

private:
    std::mutex mutex_;
    std::unordered_map<std::string, UserRecord> users_by_id_;
    std::unordered_map<std::string, std::string> username_to_id_;
    std::unordered_map<std::string, std::string> token_to_user_id_;

    static std::string generateId();
    static std::string generateToken();
};
```

- [ ] **Step 2：写 server/src/storage/ServerStorage.cpp**

```cpp
#include "storage/ServerStorage.h"
#include <iomanip>
#include <random>
#include <sstream>

// 生成随机十六进制字符串
static std::string generateHex(size_t bytes) {
    static std::mutex        rng_mutex;
    static std::random_device rd;
    static std::mt19937_64   gen(rd());
    std::uniform_int_distribution<uint8_t> dis(0, 255);

    std::lock_guard<std::mutex> lock(rng_mutex);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytes; ++i) {
        oss << std::setw(2) << static_cast<unsigned>(dis(gen));
    }
    return oss.str();
}

std::string ServerStorage::generateId()    { return "u_" + generateHex(8);  }
std::string ServerStorage::generateToken() { return "tk_" + generateHex(16); }

std::optional<ServerStorage::UserRecord>
ServerStorage::registerUser(const std::string& username,
                             const std::string& password,
                             const std::string& display_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (username_to_id_.count(username)) {
        return std::nullopt;  // 用户名已存在
    }
    UserRecord rec;
    rec.user_id      = generateId();
    rec.username     = username;
    rec.display_name = display_name;
    rec.password     = password;

    users_by_id_[rec.user_id]  = rec;
    username_to_id_[username]  = rec.user_id;
    return rec;
}

std::optional<ServerStorage::UserRecord>
ServerStorage::authenticate(const std::string& username,
                              const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = username_to_id_.find(username);
    if (it == username_to_id_.end()) return std::nullopt;
    const auto& rec = users_by_id_.at(it->second);
    if (rec.password != password) return std::nullopt;
    return rec;
}

std::string ServerStorage::createSession(const std::string& user_id) {
    std::string token = generateToken();
    std::lock_guard<std::mutex> lock(mutex_);
    token_to_user_id_[token] = user_id;
    return token;
}

std::optional<ServerStorage::UserRecord>
ServerStorage::findByToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = token_to_user_id_.find(token);
    if (it == token_to_user_id_.end()) return std::nullopt;
    auto uit = users_by_id_.find(it->second);
    if (uit == users_by_id_.end()) return std::nullopt;
    return uit->second;
}

void ServerStorage::removeSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    token_to_user_id_.erase(token);
}
```

- [ ] **Step 3：写 server/src/handler/AuthHandler.h**

```cpp
#pragma once
#include "network/ClientSession.h"
#include "storage/ServerStorage.h"
#include <memory>

class AuthHandler {
public:
    explicit AuthHandler(ServerStorage& storage);

    // 处理来自某个 session 的 Auth 相关数据包
    void handle(std::shared_ptr<ClientSession> session, const Packet& packet);

private:
    void onRegisterRequest(std::shared_ptr<ClientSession> session,
                           const Packet& packet);
    void onLoginRequest   (std::shared_ptr<ClientSession> session,
                           const Packet& packet);
    void onLogoutRequest  (std::shared_ptr<ClientSession> session,
                           const Packet& packet);

    // 发送 ErrorResponse
    void sendError(std::shared_ptr<ClientSession> session,
                   uint32_t seq_id, int code, const std::string& message);

    ServerStorage& storage_;
};
```

- [ ] **Step 4：写 server/src/handler/AuthHandler.cpp**

```cpp
#include "handler/AuthHandler.h"
#include "proto/MessageType.h"
#include "proto/ProtocolCodec.h"
#include "auth.pb.h"
#include "common.pb.h"
#include <spdlog/spdlog.h>

AuthHandler::AuthHandler(ServerStorage& storage)
    : storage_(storage) {}

void AuthHandler::handle(std::shared_ptr<ClientSession> session,
                          const Packet& packet) {
    switch (packet.type) {
    case MessageType::RegisterRequest:
        onRegisterRequest(std::move(session), packet); break;
    case MessageType::LoginRequest:
        onLoginRequest(std::move(session), packet);    break;
    case MessageType::LogoutRequest:
        onLogoutRequest(std::move(session), packet);   break;
    default:
        break;
    }
}

void AuthHandler::sendError(std::shared_ptr<ClientSession> session,
                             uint32_t seq_id, int code,
                             const std::string& message) {
    novachat::ErrorResponse er;
    er.set_code(static_cast<novachat::ErrorCode>(code));
    er.set_message(message);

    Packet resp;
    resp.type   = MessageType::ErrorResponse;
    resp.seq_id = seq_id;
    resp.payload.resize(er.ByteSizeLong());
    er.SerializeToArray(resp.payload.data(),
                        static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));
}

void AuthHandler::onRegisterRequest(std::shared_ptr<ClientSession> session,
                                     const Packet& packet) {
    novachat::RegisterRequest req;
    if (!req.ParseFromArray(packet.payload.data(),
                            static_cast<int>(packet.payload.size()))) {
        sendError(session, packet.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }

    auto record = storage_.registerUser(req.username(), req.password(),
                                         req.display_name());
    if (!record) {
        sendError(session, packet.seq_id,
                  novachat::USER_ALREADY_EXISTS, "用户名已存在");
        return;
    }

    novachat::RegisterResponse resp_pb;
    resp_pb.set_user_id(record->user_id);
    resp_pb.set_display_name(record->display_name);

    Packet resp;
    resp.type   = MessageType::RegisterResponse;
    resp.seq_id = packet.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(),
                             static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));

    spdlog::info("User registered: {} ({})", record->username, record->user_id);
}

void AuthHandler::onLoginRequest(std::shared_ptr<ClientSession> session,
                                  const Packet& packet) {
    novachat::LoginRequest req;
    if (!req.ParseFromArray(packet.payload.data(),
                            static_cast<int>(packet.payload.size()))) {
        sendError(session, packet.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }

    auto record = storage_.authenticate(req.username(), req.password());
    if (!record) {
        sendError(session, packet.seq_id,
                  novachat::AUTH_FAILED, "用户名或密码错误");
        return;
    }

    const std::string token = storage_.createSession(record->user_id);
    session->setUserId(record->user_id);

    novachat::LoginResponse resp_pb;
    resp_pb.set_token(token);
    resp_pb.set_user_id(record->user_id);
    resp_pb.set_display_name(record->display_name);

    Packet resp;
    resp.type   = MessageType::LoginResponse;
    resp.seq_id = packet.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(),
                             static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));

    spdlog::info("User logged in: {} ({})", record->username, record->user_id);
}

void AuthHandler::onLogoutRequest(std::shared_ptr<ClientSession> session,
                                   const Packet& packet) {
    novachat::LogoutRequest req;
    if (!req.ParseFromArray(packet.payload.data(),
                            static_cast<int>(packet.payload.size()))) {
        return;
    }
    storage_.removeSession(req.token());
    session->setUserId("");
    spdlog::info("User logged out, token removed");
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
    src/storage/ServerStorage.cpp    # ← 新增
    src/handler/AuthHandler.cpp      # ← 新增
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

- [ ] **Step 6：更新 server/src/main.cpp（路由 Auth 包）**

```cpp
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "network/TcpServer.h"
#include "handler/AuthHandler.h"
#include "storage/ServerStorage.h"
#include "proto/MessageType.h"
#include <asio.hpp>
#include <atomic>
#include <csignal>

namespace {
    std::atomic<bool> g_running{true};
    asio::io_context* g_io_ctx_ptr{nullptr};
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

    ServerStorage storage;
    AuthHandler   auth_handler(storage);

    auto handler = [&auth_handler](std::shared_ptr<ClientSession> session,
                                    Packet pkt) {
        switch (pkt.type) {
        case MessageType::HeartbeatRequest: {
            Packet resp;
            resp.type   = MessageType::HeartbeatResponse;
            resp.seq_id = pkt.seq_id;
            session->sendPacket(std::move(resp));
            break;
        }
        case MessageType::RegisterRequest:
        case MessageType::LoginRequest:
        case MessageType::LogoutRequest:
            auth_handler.handle(std::move(session), pkt);
            break;
        default:
            spdlog::warn("Unknown packet 0x{:04X}",
                         static_cast<uint16_t>(pkt.type));
        }
    };

    TcpServer server(io_ctx, 9527, handler);
    server.start();
    spdlog::info("NovaChatServer v0.1.0 ready (port 9527)");

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

预期：编译无报错。

- [ ] **Step 8：提交**

```
git add server/
git commit -m "feat(server): 添加 ServerStorage + AuthHandler，支持注册/登录/登出"
```

---

## Task 14：客户端 UserRepository + 单元测试

**Files:**
- Create: `client/src/storage/UserRepository.h`
- Create: `client/src/storage/UserRepository.cpp`
- Create: `client/tests/storage/test_user_repository.cpp`

- [ ] **Step 1：写 client/src/storage/UserRepository.h**

```cpp
#pragma once
#include "core/LocalAccount.h"
#include "storage/DatabaseManager.h"
#include <optional>
#include <string>

// CRUD 操作针对 local_accounts 表
// 所有方法必须在 DB 线程调用
class UserRepository {
public:
    explicit UserRepository(DatabaseManager& db);

    // 插入或更新（按 server_id 冲突时更新 username/display_name/avatar_url/token）
    void save(const LocalAccount& account);

    // 更新指定账号的 token
    void updateToken(const std::string& server_id, const std::string& token);

    // 将指定账号设为 active（同时清除其他账号的 active 标记）
    void setActive(const std::string& server_id);

    // 清除所有账号的 active 标记
    void clearActive();

    // 查找当前活跃账号
    std::optional<LocalAccount> findActive();

    // 按 server_id 查找
    std::optional<LocalAccount> findById(const std::string& server_id);

private:
    DatabaseManager& db_;

    static std::optional<LocalAccount> rowToAccount(sqlite3_stmt* stmt);
};
```

- [ ] **Step 2：写 client/src/storage/UserRepository.cpp**

```cpp
#include "storage/UserRepository.h"
#include <sqlite3.h>

UserRepository::UserRepository(DatabaseManager& db) : db_(db) {}

void UserRepository::save(const LocalAccount& acc) {
    const char* sql = R"(
        INSERT INTO local_accounts
            (server_id, username, display_name, avatar_url, token, is_active)
        VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(server_id) DO UPDATE SET
            username     = excluded.username,
            display_name = excluded.display_name,
            avatar_url   = excluded.avatar_url,
            token        = excluded.token
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, acc.server_id.c_str(),    -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, acc.username.c_str(),     -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, acc.display_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, acc.avatar_url.c_str(),   -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, acc.token.c_str(),        -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 6, acc.is_active ? 1 : 0);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void UserRepository::updateToken(const std::string& server_id,
                                  const std::string& token) {
    const char* sql =
        "UPDATE local_accounts SET token = ? WHERE server_id = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, token.c_str(),     -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, server_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void UserRepository::setActive(const std::string& server_id) {
    db_.execute("UPDATE local_accounts SET is_active = 0");
    const char* sql =
        "UPDATE local_accounts SET is_active = 1 WHERE server_id = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, server_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void UserRepository::clearActive() {
    db_.execute("UPDATE local_accounts SET is_active = 0");
}

std::optional<LocalAccount>
UserRepository::rowToAccount(sqlite3_stmt* stmt) {
    LocalAccount acc;
    acc.id           = sqlite3_column_int64(stmt, 0);
    acc.server_id    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    acc.username     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    acc.display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    acc.avatar_url   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    acc.token        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    acc.is_active    = sqlite3_column_int(stmt, 6) != 0;
    return acc;
}

std::optional<LocalAccount> UserRepository::findActive() {
    const char* sql = R"(
        SELECT id, server_id, username, display_name, avatar_url, token, is_active
        FROM local_accounts WHERE is_active = 1 LIMIT 1
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    std::optional<LocalAccount> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = rowToAccount(stmt);
    sqlite3_finalize(stmt);
    return result;
}

std::optional<LocalAccount>
UserRepository::findById(const std::string& server_id) {
    const char* sql = R"(
        SELECT id, server_id, username, display_name, avatar_url, token, is_active
        FROM local_accounts WHERE server_id = ? LIMIT 1
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, server_id.c_str(), -1, SQLITE_STATIC);
    std::optional<LocalAccount> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = rowToAccount(stmt);
    sqlite3_finalize(stmt);
    return result;
}
```

- [ ] **Step 3：写 client/tests/storage/test_user_repository.cpp**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "storage/DatabaseManager.h"
#include "storage/UserRepository.h"

static LocalAccount makeAccount(const std::string& id,
                                 const std::string& name,
                                 const std::string& token,
                                 bool active = false) {
    LocalAccount acc;
    acc.server_id    = id;
    acc.username     = name;
    acc.display_name = name + "_display";
    acc.avatar_url   = "";
    acc.token        = token;
    acc.is_active    = active;
    return acc;
}

TEST_CASE("UserRepository save 后可通过 findById 查询", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "tok_a"));
    auto found = repo.findById("u001");

    REQUIRE(found.has_value());
    CHECK(found->username     == "alice");
    CHECK(found->display_name == "alice_display");
    CHECK(found->token        == "tok_a");
    CHECK(!found->is_active);
}

TEST_CASE("UserRepository findActive 在无活跃账号时返回 nullopt", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "tok_a"));
    CHECK(!repo.findActive().has_value());
}

TEST_CASE("UserRepository setActive 后 findActive 返回对应账号", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "tok_a"));
    repo.save(makeAccount("u002", "bob",   "tok_b"));

    repo.setActive("u001");
    auto active = repo.findActive();

    REQUIRE(active.has_value());
    CHECK(active->server_id == "u001");
    CHECK(active->is_active);
}

TEST_CASE("UserRepository setActive 切换时只有一个账号为 active", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "tok_a"));
    repo.save(makeAccount("u002", "bob",   "tok_b"));

    repo.setActive("u001");
    repo.setActive("u002");   // 切换到 bob

    auto active = repo.findActive();
    REQUIRE(active.has_value());
    CHECK(active->server_id == "u002");  // alice 不再是 active
}

TEST_CASE("UserRepository clearActive 清除活跃标记", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "tok_a"));
    repo.setActive("u001");
    REQUIRE(repo.findActive().has_value());

    repo.clearActive();
    CHECK(!repo.findActive().has_value());
}

TEST_CASE("UserRepository updateToken 更新已存在账号的 token", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "old_token"));
    repo.updateToken("u001", "new_token");

    auto found = repo.findById("u001");
    REQUIRE(found.has_value());
    CHECK(found->token == "new_token");
}

TEST_CASE("UserRepository save 重复 server_id 更新而非插入新行", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "tok_a"));
    // 更新 display_name 和 token
    LocalAccount updated = makeAccount("u001", "alice_updated", "tok_b");
    repo.save(updated);

    auto found = repo.findById("u001");
    REQUIRE(found.has_value());
    CHECK(found->display_name == "alice_updated_display");
    CHECK(found->token == "tok_b");
}
```

- [ ] **Step 4：更新 client/CMakeLists.txt 和 client/tests/CMakeLists.txt**

在 `client/CMakeLists.txt` 的 NovaChat_Core 源列表中追加：
```cmake
src/storage/UserRepository.cpp
```

在 `client/tests/CMakeLists.txt` 的 NovaChat_Tests 源列表中追加：
```cmake
storage/test_user_repository.cpp
```

- [ ] **Step 5：运行测试**

```
cmake --build --preset windows-debug
ctest --preset windows-debug -V
```

预期：7 个 UserRepository 测试 PASS，累计 24 个测试全部 PASS。

- [ ] **Step 6：提交**

```
git add client/src/storage/UserRepository.h client/src/storage/UserRepository.cpp
git add client/tests/storage/test_user_repository.cpp
git add client/CMakeLists.txt client/tests/CMakeLists.txt
git commit -m "feat(storage): 添加 UserRepository，7 个单元测试通过"
```

---

## Task 15：客户端 AuthService

**Files:**
- Create: `client/src/service/AuthService.h`
- Create: `client/src/service/AuthService.cpp`

- [ ] **Step 1：写 client/src/service/AuthService.h**

```cpp
#pragma once
#include "core/User.h"
#include "network/TcpClient.h"
#include "storage/UserRepository.h"
#include <QObject>
#include <QString>

// 登录业务服务：协调 TcpClient（网络）和 UserRepository（持久化）
// 所有方法从主线程调用；回调/信号也在主线程触发
class AuthService : public QObject {
    Q_OBJECT
public:
    explicit AuthService(TcpClient*      client,
                         UserRepository& user_repo,
                         QObject*        parent = nullptr);

    // 发起登录请求
    void login(const std::string& username, const std::string& password);

    // 发起注册请求
    void registerUser(const std::string& username,
                      const std::string& password,
                      const std::string& display_name);

    // 发起登出请求，并清除本地 active 标记
    void logout();

    const User& currentUser() const { return current_user_; }

signals:
    void loginSucceeded(User user);
    void loginFailed   (const QString& reason);
    void registerSucceeded();
    void registerFailed(const QString& reason);
    void loggedOut();

private:
    TcpClient*      client_;
    UserRepository& user_repo_;
    User            current_user_;
};
```

- [ ] **Step 2：写 client/src/service/AuthService.cpp**

```cpp
#include "service/AuthService.h"
#include "logger/Logger.h"
#include "proto/MessageType.h"
#include "auth.pb.h"
#include "common.pb.h"

AuthService::AuthService(TcpClient*      client,
                          UserRepository& user_repo,
                          QObject*        parent)
    : QObject(parent)
    , client_(client)
    , user_repo_(user_repo)
{}

void AuthService::login(const std::string& username,
                         const std::string& password) {
    novachat::LoginRequest req;
    req.set_username(username);
    req.set_password(password);

    Packet pkt;
    pkt.type = MessageType::LoginRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(),
                         static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this, username](Packet resp) {
        if (resp.type == MessageType::LoginResponse) {
            novachat::LoginResponse lr;
            if (!lr.ParseFromArray(resp.payload.data(),
                                   static_cast<int>(resp.payload.size()))) {
                emit loginFailed("服务器响应解析失败");
                return;
            }
            // 持久化登录信息
            LocalAccount acc;
            acc.server_id    = lr.user_id();
            acc.username     = username;
            acc.display_name = lr.display_name();
            acc.avatar_url   = lr.avatar_url();
            acc.token        = lr.token();
            acc.is_active    = true;

            user_repo_.save(acc);
            user_repo_.setActive(lr.user_id());

            current_user_.server_id    = lr.user_id();
            current_user_.username     = username;
            current_user_.display_name = lr.display_name();
            current_user_.avatar_url   = lr.avatar_url();
            current_user_.token        = lr.token();

            Logger::info("Login succeeded: {} ({})", username, lr.user_id());
            emit loginSucceeded(current_user_);

        } else if (resp.type == MessageType::ErrorResponse) {
            novachat::ErrorResponse er;
            er.ParseFromArray(resp.payload.data(),
                              static_cast<int>(resp.payload.size()));
            Logger::warn("Login failed: {}", er.message());
            emit loginFailed(QString::fromStdString(er.message()));
        }
    });
}

void AuthService::registerUser(const std::string& username,
                                 const std::string& password,
                                 const std::string& display_name) {
    novachat::RegisterRequest req;
    req.set_username(username);
    req.set_password(password);
    req.set_display_name(display_name.empty() ? username : display_name);

    Packet pkt;
    pkt.type = MessageType::RegisterRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(),
                         static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this](Packet resp) {
        if (resp.type == MessageType::RegisterResponse) {
            Logger::info("Register succeeded");
            emit registerSucceeded();
        } else if (resp.type == MessageType::ErrorResponse) {
            novachat::ErrorResponse er;
            er.ParseFromArray(resp.payload.data(),
                              static_cast<int>(resp.payload.size()));
            emit registerFailed(QString::fromStdString(er.message()));
        }
    });
}

void AuthService::logout() {
    if (current_user_.token.empty()) return;

    novachat::LogoutRequest req;
    req.set_token(current_user_.token);

    Packet pkt;
    pkt.type = MessageType::LogoutRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(),
                         static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt));  // fire-and-forget
    user_repo_.clearActive();
    current_user_ = {};

    Logger::info("Logged out");
    emit loggedOut();
}
```

- [ ] **Step 3：更新 client/CMakeLists.txt，把 AuthService.cpp 加入 NovaChat_Core**

```cmake
src/service/AuthService.cpp   # ← 追加到 NovaChat_Core 源列表
```

- [ ] **Step 4：编译验证**

```
cmake --build --preset windows-debug
```

预期：编译无报错。

- [ ] **Step 5：提交**

```
git add client/src/service/ client/CMakeLists.txt
git commit -m "feat(service): 添加 AuthService，封装登录/注册/登出业务流程"
```

---

## Task 16：客户端 LoginWindow + RegisterDialog UI

**Files:**
- Create: `client/src/ui/LoginWindow.h`
- Create: `client/src/ui/LoginWindow.cpp`
- Create: `client/src/ui/RegisterDialog.h`
- Create: `client/src/ui/RegisterDialog.cpp`

- [ ] **Step 1：写 client/src/ui/RegisterDialog.h**

```cpp
#pragma once
#include "service/AuthService.h"
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

// 注册对话框（从 LoginWindow 打开）
class RegisterDialog : public QDialog {
    Q_OBJECT
public:
    explicit RegisterDialog(AuthService* auth_service,
                             QWidget*     parent = nullptr);

private slots:
    void onRegisterClicked();
    void onRegisterSucceeded();
    void onRegisterFailed(const QString& reason);

private:
    AuthService* auth_service_;

    QLineEdit*   username_edit_;
    QLineEdit*   display_name_edit_;
    QLineEdit*   password_edit_;
    QLineEdit*   confirm_edit_;
    QPushButton* register_btn_;
    QLabel*      error_label_;
};
```

- [ ] **Step 2：写 client/src/ui/RegisterDialog.cpp**

```cpp
#include "ui/RegisterDialog.h"
#include <QFormLayout>
#include <QMessageBox>
#include <QVBoxLayout>

RegisterDialog::RegisterDialog(AuthService* auth_service, QWidget* parent)
    : QDialog(parent)
    , auth_service_(auth_service)
{
    setWindowTitle("注册新账号");
    setFixedWidth(360);

    username_edit_     = new QLineEdit(this);
    display_name_edit_ = new QLineEdit(this);
    password_edit_     = new QLineEdit(this);
    confirm_edit_      = new QLineEdit(this);
    register_btn_      = new QPushButton("注册", this);
    error_label_       = new QLabel(this);

    password_edit_->setEchoMode(QLineEdit::Password);
    confirm_edit_->setEchoMode(QLineEdit::Password);
    error_label_->setStyleSheet("color: red;");
    error_label_->hide();

    auto* form = new QFormLayout;
    form->addRow("用户名：",   username_edit_);
    form->addRow("昵称：",     display_name_edit_);
    form->addRow("密码：",     password_edit_);
    form->addRow("确认密码：", confirm_edit_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(error_label_);
    layout->addWidget(register_btn_);

    connect(register_btn_,  &QPushButton::clicked,
            this, &RegisterDialog::onRegisterClicked);
    connect(auth_service_, &AuthService::registerSucceeded,
            this, &RegisterDialog::onRegisterSucceeded);
    connect(auth_service_, &AuthService::registerFailed,
            this, &RegisterDialog::onRegisterFailed);
}

void RegisterDialog::onRegisterClicked() {
    const QString username = username_edit_->text().trimmed();
    const QString password = password_edit_->text();
    const QString confirm  = confirm_edit_->text();
    const QString dispName = display_name_edit_->text().trimmed();

    if (username.isEmpty() || password.isEmpty()) {
        error_label_->setText("用户名和密码不能为空");
        error_label_->show();
        return;
    }
    if (password != confirm) {
        error_label_->setText("两次密码不一致");
        error_label_->show();
        return;
    }

    register_btn_->setEnabled(false);
    error_label_->hide();
    auth_service_->registerUser(username.toStdString(),
                                 password.toStdString(),
                                 dispName.toStdString());
}

void RegisterDialog::onRegisterSucceeded() {
    QMessageBox::information(this, "注册成功", "账号创建成功，请使用新账号登录。");
    accept();  // 关闭对话框，返回 LoginWindow
}

void RegisterDialog::onRegisterFailed(const QString& reason) {
    error_label_->setText(reason);
    error_label_->show();
    register_btn_->setEnabled(true);
}
```

- [ ] **Step 3：写 client/src/ui/LoginWindow.h**

```cpp
#pragma once
#include "service/AuthService.h"
#include "core/User.h"
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

// 登录窗口：用户输入凭据 → 调用 AuthService → 收到 loginSucceeded 信号后关闭自身
// 由 main.cpp 在无活跃账号时创建并 show()
class LoginWindow : public QWidget {
    Q_OBJECT
public:
    explicit LoginWindow(AuthService* auth_service,
                          QWidget*     parent = nullptr);

signals:
    // 登录成功后发出，供 main.cpp / 调用方切换到 MainWindow
    void loginSucceeded(User user);

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onLoginFailed(const QString& reason);

private:
    AuthService* auth_service_;

    QLineEdit*   username_edit_;
    QLineEdit*   password_edit_;
    QPushButton* login_btn_;
    QPushButton* register_btn_;
    QLabel*      error_label_;
};
```

- [ ] **Step 4：写 client/src/ui/LoginWindow.cpp**

```cpp
#include "ui/LoginWindow.h"
#include "ui/RegisterDialog.h"
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

LoginWindow::LoginWindow(AuthService* auth_service, QWidget* parent)
    : QWidget(parent)
    , auth_service_(auth_service)
{
    setWindowTitle("NovaChat — 登录");
    setFixedSize(380, 280);

    username_edit_ = new QLineEdit(this);
    password_edit_ = new QLineEdit(this);
    login_btn_     = new QPushButton("登录", this);
    register_btn_  = new QPushButton("注册账号", this);
    error_label_   = new QLabel(this);

    password_edit_->setEchoMode(QLineEdit::Password);
    error_label_->setStyleSheet("color: red;");
    error_label_->hide();

    auto* title = new QLabel("<h2>NovaChat</h2>", this);
    title->setAlignment(Qt::AlignCenter);

    auto* form = new QFormLayout;
    form->addRow("用户名：", username_edit_);
    form->addRow("密码：",   password_edit_);

    auto* btn_row = new QHBoxLayout;
    btn_row->addWidget(login_btn_);
    btn_row->addWidget(register_btn_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(40, 24, 40, 24);
    layout->setSpacing(12);
    layout->addWidget(title);
    layout->addLayout(form);
    layout->addWidget(error_label_);
    layout->addLayout(btn_row);

    connect(login_btn_,    &QPushButton::clicked,
            this, &LoginWindow::onLoginClicked);
    connect(register_btn_, &QPushButton::clicked,
            this, &LoginWindow::onRegisterClicked);
    connect(auth_service_, &AuthService::loginSucceeded,
            this, [this](User user) {
                emit loginSucceeded(user);
                close();
            });
    connect(auth_service_, &AuthService::loginFailed,
            this, &LoginWindow::onLoginFailed);
}

void LoginWindow::onLoginClicked() {
    const QString username = username_edit_->text().trimmed();
    const QString password = password_edit_->text();

    if (username.isEmpty() || password.isEmpty()) {
        error_label_->setText("请输入用户名和密码");
        error_label_->show();
        return;
    }

    login_btn_->setEnabled(false);
    error_label_->hide();
    auth_service_->login(username.toStdString(), password.toStdString());
}

void LoginWindow::onRegisterClicked() {
    auto* dlg = new RegisterDialog(auth_service_, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

void LoginWindow::onLoginFailed(const QString& reason) {
    error_label_->setText(reason);
    error_label_->show();
    login_btn_->setEnabled(true);
}
```

- [ ] **Step 5：更新 client/CMakeLists.txt，把新 UI 文件加入 NovaChat_UI**

```cmake
add_library(NovaChat_UI STATIC
    src/ui/MainWindow.cpp
    src/ui/LoginWindow.cpp     # ← 新增
    src/ui/RegisterDialog.cpp  # ← 新增
)
```

- [ ] **Step 6：编译验证**

```
cmake --build --preset windows-debug
```

预期：编译无报错。

- [ ] **Step 7：提交**

```
git add client/src/ui/LoginWindow.h client/src/ui/LoginWindow.cpp
git add client/src/ui/RegisterDialog.h client/src/ui/RegisterDialog.cpp
git add client/CMakeLists.txt
git commit -m "feat(ui): 添加 LoginWindow 和 RegisterDialog"
```

---

## Task 17：完整接线 —— 自动登录 + 更新 MainWindow

**Files:**
- Modify: `client/src/ui/MainWindow.h`
- Modify: `client/src/ui/MainWindow.cpp`
- Modify: `client/src/main.cpp`

- [ ] **Step 1：更新 client/src/ui/MainWindow.h**

```cpp
#pragma once
#include "core/User.h"
#include "service/AuthService.h"
#include <QMainWindow>
#include <QLabel>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // 已登录用户信息在构造时传入
    explicit MainWindow(const User&  user,
                         AuthService* auth_service,
                         QWidget*     parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onLogout();

private:
    User         current_user_;
    AuthService* auth_service_;
    QLabel*      status_label_;
};
```

- [ ] **Step 2：更新 client/src/ui/MainWindow.cpp**

```cpp
#include "ui/MainWindow.h"
#include "ui/LoginWindow.h"
#include <QApplication>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(const User& user, AuthService* auth_service,
                        QWidget* parent)
    : QMainWindow(parent)
    , current_user_(user)
    , auth_service_(auth_service)
{
    setWindowTitle(QString("NovaChat — %1")
                   .arg(QString::fromStdString(user.display_name)));
    setMinimumSize(900, 640);

    // 菜单栏
    auto* file_menu   = menuBar()->addMenu("文件");
    auto* logout_act  = file_menu->addAction("退出登录");
    auto* quit_act    = file_menu->addAction("退出程序");
    connect(logout_act, &QAction::triggered, this, &MainWindow::onLogout);
    connect(quit_act,   &QAction::triggered, qApp, &QApplication::quit);

    // 阶段三占位内容（后续替换为真实布局）
    status_label_ = new QLabel(
        QString("欢迎，%1！\n联系人面板和聊天窗口将在后续阶段实现。")
        .arg(QString::fromStdString(user.display_name)), this);
    status_label_->setAlignment(Qt::AlignCenter);
    setCentralWidget(status_label_);

    statusBar()->showMessage(
        QString("已登录：%1").arg(QString::fromStdString(user.username)));
}

void MainWindow::onLogout() {
    auth_service_->logout();

    // 退出登录后重新打开 LoginWindow
    auto* login_win = new LoginWindow(auth_service_);
    connect(auth_service_, &AuthService::loginSucceeded,
            login_win, [this, login_win](User u) {
                current_user_ = u;
                setWindowTitle(QString("NovaChat — %1")
                               .arg(QString::fromStdString(u.display_name)));
                status_label_->setText(
                    QString("欢迎，%1！").arg(
                        QString::fromStdString(u.display_name)));
                statusBar()->showMessage(
                    QString("已登录：%1").arg(
                        QString::fromStdString(u.username)));
                login_win->close();
            });
    login_win->show();
}
```

- [ ] **Step 3：更新 client/src/main.cpp（完整启动流程）**

```cpp
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include "logger/Logger.h"
#include "config/AppConfig.h"
#include "storage/DatabaseManager.h"
#include "storage/UserRepository.h"
#include "network/TcpClient.h"
#include "service/AuthService.h"
#include "ui/LoginWindow.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("NovaChat");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("NovaChat");

    // ── 日志 ────────────────────────────────────────────────────
    Logger::init("novachat.log");
    Logger::info("NovaChat v0.1.0 启动");

    // ── 配置 ────────────────────────────────────────────────────
    AppConfig::instance().load("config.yaml");

    // ── 数据库（放在应用数据目录）────────────────────────────────
    const QString data_dir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(data_dir);
    const std::string db_path =
        (data_dir + "/novachat.db").toStdString();

    auto db        = std::make_unique<DatabaseManager>(db_path);
    auto user_repo = std::make_unique<UserRepository>(*db);

    // ── 网络 ────────────────────────────────────────────────────
    auto* tcp_client  = new TcpClient(&app);
    auto* auth_service = new AuthService(tcp_client, *user_repo, &app);

    // 连接登出后的回调：断开连接
    QObject::connect(auth_service, &AuthService::loggedOut, tcp_client,
                     &TcpClient::disconnect);

    // ── 决定启动界面 ─────────────────────────────────────────────
    auto active = user_repo->findActive();

    MainWindow* main_win   = nullptr;
    LoginWindow* login_win = nullptr;

    if (active) {
        // 自动登录：使用本地存储的账号信息直接进入主界面
        Logger::info("Auto-login: {}", active->username);
        User user;
        user.server_id    = active->server_id;
        user.username     = active->username;
        user.display_name = active->display_name;
        user.avatar_url   = active->avatar_url;
        user.token        = active->token;

        main_win = new MainWindow(user, auth_service);
        main_win->show();
    } else {
        // 无活跃账号：显示登录窗口
        login_win = new LoginWindow(auth_service);
        QObject::connect(login_win, &LoginWindow::loginSucceeded,
                         [&main_win, auth_service](User user) {
                             main_win = new MainWindow(user, auth_service);
                             main_win->show();
                         });
        login_win->show();
    }

    // 后台连接服务器（无论登录还是自动登录都需要）
    tcp_client->connectToServer(AppConfig::instance().server.host,
                                 AppConfig::instance().server.port);

    Logger::info("事件循环启动");
    const int result = app.exec();

    Logger::info("NovaChat 退出");
    Logger::shutdown();
    return result;
}
```

- [ ] **Step 4：编译验证**

```
cmake --build --preset windows-debug
```

预期：编译无报错。

- [ ] **Step 5：端到端登录流程验证**

**终端 1 — 启动服务端：**
```
./build/debug/server/NovaChatServer.exe
```

**终端 2 — 启动客户端（确认本地 DB 中无活跃账号）：**
```
./build/debug/client/NovaChat.exe
```

验证步骤：
1. 弹出 LoginWindow（无已存账号）
2. 点击「注册账号」→ 填写用户名/昵称/密码 → 点注册 → 提示「注册成功」
3. 在 LoginWindow 输入同样用户名/密码 → 点登录
4. LoginWindow 关闭，MainWindow 弹出，标题栏显示「NovaChat — 你的昵称」
5. 服务端日志显示 `User registered` 和 `User logged in`

**重启客户端验证自动登录：**
```
./build/debug/client/NovaChat.exe
```

预期：直接弹出 MainWindow（跳过 LoginWindow）。

- [ ] **Step 6：验证退出登录后回到 LoginWindow**

在 MainWindow 中点击 「文件 → 退出登录」：
- 预期：MainWindow 内容变为欢迎占位，同时 LoginWindow 弹出
- 重新登录后 MainWindow 恢复正常

- [ ] **Step 7：运行全部测试**

```
cmake --build --preset windows-debug
ctest --preset windows-debug -V
```

预期：24 个测试全部 PASS。

- [ ] **Step 8：提交**

```
git add client/src/ui/MainWindow.h client/src/ui/MainWindow.cpp
git add client/src/main.cpp
git commit -m "feat: 完成登录模块——自动登录/注册/登录/退出登录全流程"
```

---

## 阶段三完成标准

- [ ] `ctest --preset windows-debug` 24 个测试全部 PASS
- [ ] 首次启动：显示 LoginWindow，可完成注册和登录
- [ ] 登录后：MainWindow 标题显示昵称，状态栏显示用户名
- [ ] 重启后：自动登录，直接进入 MainWindow
- [ ] 退出登录：清除本地 active，重启后回到 LoginWindow
- [ ] 服务端日志可见注册/登录事件
- [ ] 所有代码已提交到 git
