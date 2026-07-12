# NovaChat V1 — 阶段四实现计划：联系人模块

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成联系人模块：服务端支持好友关系管理；客户端实现 ContactRepository 持久化、ContactService 业务逻辑、ContactPanel UI（好友列表 + 搜索过滤 + 添加/删除好友）；登录成功后自动同步联系人到本地；MainWindow 展示联系人面板。

**Architecture:** ContactService 作为联系人业务协调者，登录后调用 `syncContacts()` 从服务端拉取完整好友列表并写入本地 SQLite。ContactPanel 使用 QListWidget + QSortFilterProxyModel 实现本地过滤搜索，新增好友通过 AddFriendDialog 先搜索再添加。V1 好友关系立即双向生效（无需对方确认）。

**Tech Stack:** C++20 · Qt6 Widgets · Protobuf（contact.proto）· SQLite · spdlog

---

## 本阶段新增/修改文件

```
NovaChat/
├── server/
│   └── src/
│       ├── main.cpp                        ← 修改：路由 Contact 包
│       ├── storage/
│       │   ├── ServerStorage.h             ← 修改：添加好友关系方法
│       │   └── ServerStorage.cpp           ← 修改
│       └── handler/
│           ├── ContactHandler.h            ← 新增
│           └── ContactHandler.cpp          ← 新增
└── client/
    ├── CMakeLists.txt                      ← 修改
    ├── src/
    │   ├── main.cpp                        ← 修改：登录后触发 syncContacts
    │   ├── storage/
    │   │   ├── ContactRepository.h         ← 新增
    │   │   └── ContactRepository.cpp       ← 新增
    │   ├── service/
    │   │   ├── ContactService.h            ← 新增
    │   │   └── ContactService.cpp          ← 新增
    │   └── ui/
    │       ├── MainWindow.h                ← 修改：集成 ContactPanel
    │       ├── MainWindow.cpp              ← 修改
    │       ├── ContactPanel.h              ← 新增
    │       ├── ContactPanel.cpp            ← 新增
    │       ├── AddFriendDialog.h           ← 新增
    │       └── AddFriendDialog.cpp         ← 新增
    └── tests/
        ├── CMakeLists.txt                  ← 修改
        └── storage/
            └── test_contact_repository.cpp ← 新增
```

---

## Task 18：扩展 ServerStorage + 添加 ContactHandler

**Files:**
- Modify: `server/src/storage/ServerStorage.h`
- Modify: `server/src/storage/ServerStorage.cpp`
- Create: `server/src/handler/ContactHandler.h`
- Create: `server/src/handler/ContactHandler.cpp`
- Modify: `server/src/main.cpp`
- Modify: `server/CMakeLists.txt`

- [ ] **Step 1：在 server/src/storage/ServerStorage.h 末尾添加好友关系方法**

在 `public:` 区块追加：
```cpp
    // ── 好友关系（V1：添加即双向生效）──────────────────────────
    // 返回 false 表示目标用户不存在或已是好友
    bool addFriend(const std::string& user_id, const std::string& friend_id);
    bool removeFriend(const std::string& user_id, const std::string& friend_id);
    std::vector<UserRecord> getFriends(const std::string& user_id);

    // 按用户名前缀搜索（不区分大小写，最多返回 20 条）
    std::vector<UserRecord> searchUsers(const std::string& keyword);

    // 按 user_id 查找（用于鉴权之外的用户信息查询）
    std::optional<UserRecord> findById(const std::string& user_id);
```

在 `private:` 区块追加：
```cpp
    // key: user_id, value: 该用户的好友 user_id 集合
    std::unordered_map<std::string,
                       std::unordered_set<std::string>> friendships_;
```

同时在文件顶部添加头文件：
```cpp
#include <unordered_set>
#include <vector>
```

- [ ] **Step 2：在 server/src/storage/ServerStorage.cpp 末尾追加实现**

```cpp
bool ServerStorage::addFriend(const std::string& user_id,
                               const std::string& friend_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!users_by_id_.count(user_id) || !users_by_id_.count(friend_id)) {
        return false;  // 用户不存在
    }
    auto [it, inserted] = friendships_[user_id].insert(friend_id);
    if (!inserted) return false;   // 已是好友
    friendships_[friend_id].insert(user_id);  // 双向
    return true;
}

bool ServerStorage::removeFriend(const std::string& user_id,
                                  const std::string& friend_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = friendships_.find(user_id);
    if (it == friendships_.end()) return false;
    bool removed = it->second.erase(friend_id) > 0;
    if (removed) {
        auto it2 = friendships_.find(friend_id);
        if (it2 != friendships_.end()) it2->second.erase(user_id);
    }
    return removed;
}

std::vector<ServerStorage::UserRecord>
ServerStorage::getFriends(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UserRecord> result;
    auto it = friendships_.find(user_id);
    if (it == friendships_.end()) return result;
    for (const auto& fid : it->second) {
        auto uit = users_by_id_.find(fid);
        if (uit != users_by_id_.end()) result.push_back(uit->second);
    }
    return result;
}

std::vector<ServerStorage::UserRecord>
ServerStorage::searchUsers(const std::string& keyword) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<UserRecord> result;
    std::string kw = keyword;
    std::transform(kw.begin(), kw.end(), kw.begin(), ::tolower);

    for (const auto& [id, rec] : users_by_id_) {
        std::string name = rec.username;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name.find(kw) != std::string::npos) {
            result.push_back(rec);
            if (result.size() >= 20) break;
        }
    }
    return result;
}

std::optional<ServerStorage::UserRecord>
ServerStorage::findById(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_by_id_.find(user_id);
    if (it == users_by_id_.end()) return std::nullopt;
    return it->second;
}
```

在文件顶部追加：
```cpp
#include <algorithm>
```

- [ ] **Step 3：写 server/src/handler/ContactHandler.h**

```cpp
#pragma once
#include "network/ClientSession.h"
#include "storage/ServerStorage.h"
#include <memory>

class ContactHandler {
public:
    explicit ContactHandler(ServerStorage& storage);

    void handle(std::shared_ptr<ClientSession> session, const Packet& packet);

private:
    void onGetContacts  (std::shared_ptr<ClientSession> session, const Packet& pkt);
    void onAddFriend    (std::shared_ptr<ClientSession> session, const Packet& pkt);
    void onDeleteFriend (std::shared_ptr<ClientSession> session, const Packet& pkt);
    void onSearchUser   (std::shared_ptr<ClientSession> session, const Packet& pkt);

    // 验证 token 合法性，返回对应用户；失败则向客户端发送 ErrorResponse 并返回 nullopt
    std::optional<ServerStorage::UserRecord>
    validateToken(std::shared_ptr<ClientSession> session,
                  const std::string& token, uint32_t seq_id);

    void sendError(std::shared_ptr<ClientSession> session,
                   uint32_t seq_id, int code, const std::string& msg);

    ServerStorage& storage_;
};
```

- [ ] **Step 4：写 server/src/handler/ContactHandler.cpp**

```cpp
#include "handler/ContactHandler.h"
#include "proto/MessageType.h"
#include "contact.pb.h"
#include "common.pb.h"
#include <spdlog/spdlog.h>

ContactHandler::ContactHandler(ServerStorage& storage)
    : storage_(storage) {}

void ContactHandler::handle(std::shared_ptr<ClientSession> session,
                             const Packet& packet) {
    switch (packet.type) {
    case MessageType::GetContactsRequest:  onGetContacts (session, packet); break;
    case MessageType::AddFriendRequest:    onAddFriend   (session, packet); break;
    case MessageType::DeleteFriendRequest: onDeleteFriend(session, packet); break;
    case MessageType::SearchUserRequest:   onSearchUser  (session, packet); break;
    default: break;
    }
}

std::optional<ServerStorage::UserRecord>
ContactHandler::validateToken(std::shared_ptr<ClientSession> session,
                               const std::string& token, uint32_t seq_id) {
    auto user = storage_.findByToken(token);
    if (!user) {
        sendError(session, seq_id, novachat::AUTH_EXPIRED, "Token 无效或已过期");
    }
    return user;
}

void ContactHandler::sendError(std::shared_ptr<ClientSession> session,
                                uint32_t seq_id, int code,
                                const std::string& msg) {
    novachat::ErrorResponse er;
    er.set_code(static_cast<novachat::ErrorCode>(code));
    er.set_message(msg);

    Packet resp;
    resp.type   = MessageType::ErrorResponse;
    resp.seq_id = seq_id;
    resp.payload.resize(er.ByteSizeLong());
    er.SerializeToArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));
}

// 辅助：将 UserRecord 转为 ContactInfo proto
static novachat::ContactInfo toContactInfo(const ServerStorage::UserRecord& rec) {
    novachat::ContactInfo ci;
    ci.set_user_id(rec.user_id);
    ci.set_username(rec.username);
    ci.set_display_name(rec.display_name);
    return ci;
}

void ContactHandler::onGetContacts(std::shared_ptr<ClientSession> session,
                                    const Packet& pkt) {
    novachat::GetContactsRequest req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()))) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }
    auto user = validateToken(session, req.token(), pkt.seq_id);
    if (!user) return;

    auto friends = storage_.getFriends(user->user_id);

    novachat::GetContactsResponse resp_pb;
    for (const auto& f : friends) {
        *resp_pb.add_contacts() = toContactInfo(f);
    }

    Packet resp;
    resp.type   = MessageType::GetContactsResponse;
    resp.seq_id = pkt.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));

    spdlog::debug("GetContacts: {} returned {} contacts",
                  user->username, friends.size());
}

void ContactHandler::onAddFriend(std::shared_ptr<ClientSession> session,
                                  const Packet& pkt) {
    novachat::AddFriendRequest req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()))) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }
    auto user = validateToken(session, req.token(), pkt.seq_id);
    if (!user) return;

    if (user->user_id == req.target_user_id()) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "不能添加自己为好友");
        return;
    }

    bool ok = storage_.addFriend(user->user_id, req.target_user_id());
    if (!ok) {
        sendError(session, pkt.seq_id,
                  novachat::FRIEND_ALREADY_EXISTS, "已是好友或用户不存在");
        return;
    }

    auto target = storage_.findById(req.target_user_id());

    novachat::AddFriendResponse resp_pb;
    if (target) *resp_pb.mutable_contact() = toContactInfo(*target);

    Packet resp;
    resp.type   = MessageType::AddFriendResponse;
    resp.seq_id = pkt.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));

    spdlog::info("Friend added: {} ↔ {}", user->username,
                 target ? target->username : req.target_user_id());
}

void ContactHandler::onDeleteFriend(std::shared_ptr<ClientSession> session,
                                     const Packet& pkt) {
    novachat::DeleteFriendRequest req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()))) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }
    auto user = validateToken(session, req.token(), pkt.seq_id);
    if (!user) return;

    bool ok = storage_.removeFriend(user->user_id, req.target_user_id());
    if (!ok) {
        sendError(session, pkt.seq_id,
                  novachat::FRIEND_NOT_FOUND, "好友不存在");
        return;
    }

    Packet resp;
    resp.type   = MessageType::DeleteFriendResponse;
    resp.seq_id = pkt.seq_id;
    // DeleteFriendResponse 无 payload
    session->sendPacket(std::move(resp));

    spdlog::info("Friend removed: {} -x- {}", user->username, req.target_user_id());
}

void ContactHandler::onSearchUser(std::shared_ptr<ClientSession> session,
                                   const Packet& pkt) {
    novachat::SearchUserRequest req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()))) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }
    auto user = validateToken(session, req.token(), pkt.seq_id);
    if (!user) return;

    auto results = storage_.searchUsers(req.keyword());

    novachat::SearchUserResponse resp_pb;
    for (const auto& r : results) {
        if (r.user_id != user->user_id) {  // 不返回自己
            *resp_pb.add_users() = toContactInfo(r);
        }
    }

    Packet resp;
    resp.type   = MessageType::SearchUserResponse;
    resp.seq_id = pkt.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));
}
```

- [ ] **Step 5：更新 server/CMakeLists.txt，添加 ContactHandler**

```cmake
add_executable(NovaChatServer
    src/main.cpp
    src/network/TcpServer.cpp
    src/network/ClientSession.cpp
    src/storage/ServerStorage.cpp
    src/handler/AuthHandler.cpp
    src/handler/ContactHandler.cpp    # ← 新增
)
```

- [ ] **Step 6：更新 server/src/main.cpp，路由 Contact 包**

在 `main()` 中，`ServerStorage storage;` 下方添加：
```cpp
ContactHandler contact_handler(storage);
```

在 `handler` lambda 的 switch 中追加：
```cpp
case MessageType::GetContactsRequest:
case MessageType::AddFriendRequest:
case MessageType::DeleteFriendRequest:
case MessageType::SearchUserRequest:
    contact_handler.handle(std::move(session), pkt);
    break;
```

同时在文件顶部添加：
```cpp
#include "handler/ContactHandler.h"
```

- [ ] **Step 7：编译验证**

```
cmake --build --preset windows-debug
```

预期：编译无报错。

- [ ] **Step 8：提交**

```
git add server/src/storage/ server/src/handler/ContactHandler.h server/src/handler/ContactHandler.cpp
git add server/src/main.cpp server/CMakeLists.txt
git commit -m "feat(server): 添加 ContactHandler，支持好友列表/添加/删除/搜索"
```

---

## Task 19：客户端 ContactRepository + 单元测试

**Files:**
- Create: `client/src/storage/ContactRepository.h`
- Create: `client/src/storage/ContactRepository.cpp`
- Create: `client/tests/storage/test_contact_repository.cpp`

- [ ] **Step 1：写 client/src/storage/ContactRepository.h**

```cpp
#pragma once
#include "core/Contact.h"
#include "storage/DatabaseManager.h"
#include <optional>
#include <string>
#include <vector>

// CRUD 操作针对 contacts 表
// 所有方法必须在 DB 线程调用
class ContactRepository {
public:
    explicit ContactRepository(DatabaseManager& db);

    // 插入或更新单条联系人（按 owner_id + user_id 冲突时更新）
    void save(const Contact& contact);

    // 批量替换：先删除 owner_id 的全部联系人，再全量插入
    void replaceAll(const std::string&         owner_id,
                    const std::vector<Contact>& contacts);

    // 删除一条联系人关系
    void remove(const std::string& owner_id, const std::string& user_id);

    // 查询指定 owner 的全部联系人
    std::vector<Contact> findAll(const std::string& owner_id);

    // 按 user_id 查找
    std::optional<Contact> findById(const std::string& owner_id,
                                    const std::string& user_id);

private:
    DatabaseManager& db_;

    static Contact rowToContact(sqlite3_stmt* stmt);
};
```

- [ ] **Step 2：写 client/src/storage/ContactRepository.cpp**

```cpp
#include "storage/ContactRepository.h"
#include <sqlite3.h>

ContactRepository::ContactRepository(DatabaseManager& db) : db_(db) {}

Contact ContactRepository::rowToContact(sqlite3_stmt* stmt) {
    Contact c;
    c.owner_id    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    c.user_id     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    c.username    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    c.display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    c.avatar_url  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    return c;
}

void ContactRepository::save(const Contact& c) {
    const char* sql = R"(
        INSERT INTO contacts (owner_id, user_id, username, display_name, avatar_url)
        VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(owner_id, user_id) DO UPDATE SET
            username     = excluded.username,
            display_name = excluded.display_name,
            avatar_url   = excluded.avatar_url
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, c.owner_id.c_str(),    -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, c.user_id.c_str(),     -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, c.username.c_str(),    -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, c.display_name.c_str(),-1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, c.avatar_url.c_str(),  -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void ContactRepository::replaceAll(const std::string&         owner_id,
                                    const std::vector<Contact>& contacts) {
    db_.execute("BEGIN TRANSACTION");
    try {
        const std::string del = "DELETE FROM contacts WHERE owner_id = '" +
                                owner_id + "'";
        db_.execute(del);
        for (const auto& c : contacts) save(c);
        db_.execute("COMMIT");
    } catch (...) {
        db_.execute("ROLLBACK");
        throw;
    }
}

void ContactRepository::remove(const std::string& owner_id,
                                const std::string& user_id) {
    const char* sql =
        "DELETE FROM contacts WHERE owner_id = ? AND user_id = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, owner_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_id.c_str(),  -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<Contact>
ContactRepository::findAll(const std::string& owner_id) {
    const char* sql = R"(
        SELECT owner_id, user_id, username, display_name, avatar_url
        FROM contacts WHERE owner_id = ?
        ORDER BY display_name COLLATE NOCASE
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, owner_id.c_str(), -1, SQLITE_STATIC);

    std::vector<Contact> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(rowToContact(stmt));
    }
    sqlite3_finalize(stmt);
    return result;
}

std::optional<Contact>
ContactRepository::findById(const std::string& owner_id,
                             const std::string& user_id) {
    const char* sql = R"(
        SELECT owner_id, user_id, username, display_name, avatar_url
        FROM contacts WHERE owner_id = ? AND user_id = ? LIMIT 1
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, owner_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_id.c_str(),  -1, SQLITE_STATIC);

    std::optional<Contact> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = rowToContact(stmt);
    sqlite3_finalize(stmt);
    return result;
}
```

- [ ] **Step 3：写 client/tests/storage/test_contact_repository.cpp**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "storage/DatabaseManager.h"
#include "storage/ContactRepository.h"

static Contact makeContact(const std::string& owner,
                            const std::string& uid,
                            const std::string& name) {
    Contact c;
    c.owner_id    = owner;
    c.user_id     = uid;
    c.username    = name;
    c.display_name = name + "_display";
    c.avatar_url  = "";
    return c;
}

TEST_CASE("ContactRepository save 后可通过 findById 查询", "[contact_repo]") {
    DatabaseManager    db(":memory:");
    ContactRepository  repo(db);

    repo.save(makeContact("owner1", "u001", "alice"));
    auto found = repo.findById("owner1", "u001");

    REQUIRE(found.has_value());
    CHECK(found->username     == "alice");
    CHECK(found->display_name == "alice_display");
}

TEST_CASE("ContactRepository findAll 无数据时返回空列表", "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    CHECK(repo.findAll("owner_nobody").empty());
}

TEST_CASE("ContactRepository findAll 按 display_name 排序", "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    repo.save(makeContact("o1", "u3", "charlie"));
    repo.save(makeContact("o1", "u1", "alice"));
    repo.save(makeContact("o1", "u2", "bob"));

    auto list = repo.findAll("o1");
    REQUIRE(list.size() == 3);
    CHECK(list[0].username == "alice");
    CHECK(list[1].username == "bob");
    CHECK(list[2].username == "charlie");
}

TEST_CASE("ContactRepository save 重复 (owner,user_id) 更新而非新增", "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    repo.save(makeContact("o1", "u1", "alice"));
    Contact updated = makeContact("o1", "u1", "alice_renamed");
    repo.save(updated);

    auto all = repo.findAll("o1");
    REQUIRE(all.size() == 1);
    CHECK(all[0].display_name == "alice_renamed_display");
}

TEST_CASE("ContactRepository remove 删除指定联系人", "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    repo.save(makeContact("o1", "u1", "alice"));
    repo.save(makeContact("o1", "u2", "bob"));
    repo.remove("o1", "u1");

    auto all = repo.findAll("o1");
    REQUIRE(all.size() == 1);
    CHECK(all[0].user_id == "u2");
}

TEST_CASE("ContactRepository replaceAll 全量替换", "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    repo.save(makeContact("o1", "u1", "alice"));
    repo.save(makeContact("o1", "u2", "bob"));

    std::vector<Contact> new_list = {makeContact("o1", "u3", "charlie"),
                                      makeContact("o1", "u4", "dave")};
    repo.replaceAll("o1", new_list);

    auto all = repo.findAll("o1");
    REQUIRE(all.size() == 2);
    CHECK(all[0].username == "charlie");
    CHECK(all[1].username == "dave");
}

TEST_CASE("ContactRepository owner 隔离：不同 owner 的联系人互不干扰",
          "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    repo.save(makeContact("owner_a", "u1", "alice"));
    repo.save(makeContact("owner_b", "u1", "alice_b"));

    auto a_list = repo.findAll("owner_a");
    auto b_list = repo.findAll("owner_b");

    REQUIRE(a_list.size() == 1);
    REQUIRE(b_list.size() == 1);
    CHECK(a_list[0].display_name == "alice_display");
    CHECK(b_list[0].display_name == "alice_b_display");
}
```

- [ ] **Step 4：更新 CMakeLists.txt 文件**

`client/CMakeLists.txt` 的 NovaChat_Core 源列表追加：
```cmake
src/storage/ContactRepository.cpp
```

`client/tests/CMakeLists.txt` 的 NovaChat_Tests 源列表追加：
```cmake
storage/test_contact_repository.cpp
```

- [ ] **Step 5：运行测试**

```
cmake --build --preset windows-debug
ctest --preset windows-debug -V
```

预期：7 个 ContactRepository 测试 PASS，累计 31 个测试全部 PASS。

- [ ] **Step 6：提交**

```
git add client/src/storage/ContactRepository.h client/src/storage/ContactRepository.cpp
git add client/tests/storage/test_contact_repository.cpp
git add client/CMakeLists.txt client/tests/CMakeLists.txt
git commit -m "feat(storage): 添加 ContactRepository，7 个单元测试通过"
```

---

## Task 20：客户端 ContactService

**Files:**
- Create: `client/src/service/ContactService.h`
- Create: `client/src/service/ContactService.cpp`

- [ ] **Step 1：写 client/src/service/ContactService.h**

```cpp
#pragma once
#include "core/Contact.h"
#include "core/User.h"
#include "network/TcpClient.h"
#include "storage/ContactRepository.h"
#include <QObject>
#include <QString>
#include <vector>

// 联系人业务服务
// · syncContacts()：从服务端拉取完整好友列表并落库
// · addFriend() / deleteFriend()：操作并更新本地缓存
// · searchUser()：在服务端搜索用户
class ContactService : public QObject {
    Q_OBJECT
public:
    ContactService(TcpClient*         client,
                   ContactRepository& repo,
                   const User&        current_user,
                   QObject*           parent = nullptr);

    void syncContacts();
    void addFriend   (const std::string& target_user_id);
    void deleteFriend(const std::string& target_user_id);
    void searchUser  (const std::string& keyword);

    const std::vector<Contact>& contacts() const { return contacts_; }

signals:
    void contactsSynced (std::vector<Contact> contacts);
    void contactAdded   (Contact contact);
    void contactDeleted (std::string user_id);
    void searchResult   (std::vector<Contact> users);
    void operationFailed(const QString& reason);

private:
    TcpClient*         client_;
    ContactRepository& repo_;
    const User&        current_user_;
    std::vector<Contact> contacts_;   // 内存缓存
};
```

- [ ] **Step 2：写 client/src/service/ContactService.cpp**

```cpp
#include "service/ContactService.h"
#include "logger/Logger.h"
#include "proto/MessageType.h"
#include "contact.pb.h"
#include "common.pb.h"

ContactService::ContactService(TcpClient*         client,
                                ContactRepository& repo,
                                const User&        current_user,
                                QObject*           parent)
    : QObject(parent)
    , client_(client)
    , repo_(repo)
    , current_user_(current_user)
{
    // 加载本地缓存（离线时也能显示联系人）
    contacts_ = repo_.findAll(current_user_.server_id);
}

void ContactService::syncContacts() {
    novachat::GetContactsRequest req;
    req.set_token(current_user_.token);

    Packet pkt;
    pkt.type = MessageType::GetContactsRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this](Packet resp) {
        if (resp.type == MessageType::GetContactsResponse) {
            novachat::GetContactsResponse r;
            if (!r.ParseFromArray(resp.payload.data(),
                                  static_cast<int>(resp.payload.size()))) return;

            std::vector<Contact> list;
            list.reserve(r.contacts_size());
            for (const auto& ci : r.contacts()) {
                Contact c;
                c.owner_id    = current_user_.server_id;
                c.user_id     = ci.user_id();
                c.username    = ci.username();
                c.display_name = ci.display_name();
                c.avatar_url  = ci.avatar_url();
                list.push_back(c);
            }

            repo_.replaceAll(current_user_.server_id, list);
            contacts_ = list;

            Logger::info("Contacts synced: {} contacts", list.size());
            emit contactsSynced(contacts_);

        } else if (resp.type == MessageType::ErrorResponse) {
            novachat::ErrorResponse er;
            er.ParseFromArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
            emit operationFailed(QString::fromStdString(er.message()));
        }
    });
}

void ContactService::addFriend(const std::string& target_user_id) {
    novachat::AddFriendRequest req;
    req.set_token(current_user_.token);
    req.set_target_user_id(target_user_id);

    Packet pkt;
    pkt.type = MessageType::AddFriendRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this](Packet resp) {
        if (resp.type == MessageType::AddFriendResponse) {
            novachat::AddFriendResponse r;
            if (!r.ParseFromArray(resp.payload.data(),
                                  static_cast<int>(resp.payload.size()))) return;

            const auto& ci = r.contact();
            Contact c;
            c.owner_id    = current_user_.server_id;
            c.user_id     = ci.user_id();
            c.username    = ci.username();
            c.display_name = ci.display_name();
            c.avatar_url  = ci.avatar_url();

            repo_.save(c);
            contacts_.push_back(c);

            Logger::info("Friend added: {}", c.username);
            emit contactAdded(c);

        } else if (resp.type == MessageType::ErrorResponse) {
            novachat::ErrorResponse er;
            er.ParseFromArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
            emit operationFailed(QString::fromStdString(er.message()));
        }
    });
}

void ContactService::deleteFriend(const std::string& target_user_id) {
    novachat::DeleteFriendRequest req;
    req.set_token(current_user_.token);
    req.set_target_user_id(target_user_id);

    Packet pkt;
    pkt.type = MessageType::DeleteFriendRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this, target_user_id](Packet resp) {
        if (resp.type == MessageType::DeleteFriendResponse) {
            repo_.remove(current_user_.server_id, target_user_id);
            contacts_.erase(
                std::remove_if(contacts_.begin(), contacts_.end(),
                    [&target_user_id](const Contact& c) {
                        return c.user_id == target_user_id;
                    }),
                contacts_.end());

            Logger::info("Friend deleted: {}", target_user_id);
            emit contactDeleted(target_user_id);

        } else if (resp.type == MessageType::ErrorResponse) {
            novachat::ErrorResponse er;
            er.ParseFromArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
            emit operationFailed(QString::fromStdString(er.message()));
        }
    });
}

void ContactService::searchUser(const std::string& keyword) {
    novachat::SearchUserRequest req;
    req.set_token(current_user_.token);
    req.set_keyword(keyword);

    Packet pkt;
    pkt.type = MessageType::SearchUserRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this](Packet resp) {
        if (resp.type == MessageType::SearchUserResponse) {
            novachat::SearchUserResponse r;
            if (!r.ParseFromArray(resp.payload.data(),
                                  static_cast<int>(resp.payload.size()))) return;

            std::vector<Contact> results;
            for (const auto& ci : r.users()) {
                Contact c;
                c.user_id     = ci.user_id();
                c.username    = ci.username();
                c.display_name = ci.display_name();
                results.push_back(c);
            }
            emit searchResult(results);
        }
    });
}
```

- [ ] **Step 3：更新 client/CMakeLists.txt，追加 ContactService.cpp**

```cmake
src/service/ContactService.cpp   # ← 追加到 NovaChat_Core 源列表
```

同时在 `target_link_libraries(NovaChat_Core ...)` 中确认已有 `<algorithm>` 支持（标准库，无需额外链接）。

- [ ] **Step 4：编译验证**

```
cmake --build --preset windows-debug
```

预期：编译无报错。

- [ ] **Step 5：提交**

```
git add client/src/service/ContactService.h client/src/service/ContactService.cpp
git add client/CMakeLists.txt
git commit -m "feat(service): 添加 ContactService，封装联系人同步/添加/删除/搜索"
```

---

## Task 21：客户端 ContactPanel + AddFriendDialog UI

**Files:**
- Create: `client/src/ui/AddFriendDialog.h`
- Create: `client/src/ui/AddFriendDialog.cpp`
- Create: `client/src/ui/ContactPanel.h`
- Create: `client/src/ui/ContactPanel.cpp`

- [ ] **Step 1：写 client/src/ui/AddFriendDialog.h**

```cpp
#pragma once
#include "core/Contact.h"
#include "service/ContactService.h"
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <vector>

// 搜索并添加好友的对话框
class AddFriendDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddFriendDialog(ContactService* service,
                              QWidget*        parent = nullptr);

private slots:
    void onSearchClicked();
    void onAddClicked();
    void onSearchResult(const std::vector<Contact>& users);
    void onOperationFailed(const QString& reason);

private:
    ContactService*  service_;
    std::vector<Contact> results_;   // 当前搜索结果

    QLineEdit*   search_edit_;
    QPushButton* search_btn_;
    QListWidget* result_list_;
    QPushButton* add_btn_;
    QLabel*      status_label_;
};
```

- [ ] **Step 2：写 client/src/ui/AddFriendDialog.cpp**

```cpp
#include "ui/AddFriendDialog.h"
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

AddFriendDialog::AddFriendDialog(ContactService* service, QWidget* parent)
    : QDialog(parent)
    , service_(service)
{
    setWindowTitle("添加好友");
    setMinimumWidth(360);

    search_edit_  = new QLineEdit(this);
    search_btn_   = new QPushButton("搜索", this);
    result_list_  = new QListWidget(this);
    add_btn_      = new QPushButton("添加为好友", this);
    status_label_ = new QLabel(this);

    search_edit_->setPlaceholderText("输入用户名搜索...");
    add_btn_->setEnabled(false);
    status_label_->setStyleSheet("color: #666;");

    auto* search_row = new QHBoxLayout;
    search_row->addWidget(search_edit_);
    search_row->addWidget(search_btn_);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(search_row);
    layout->addWidget(result_list_);
    layout->addWidget(status_label_);
    layout->addWidget(add_btn_);

    connect(search_btn_,  &QPushButton::clicked,
            this, &AddFriendDialog::onSearchClicked);
    connect(search_edit_, &QLineEdit::returnPressed,
            this, &AddFriendDialog::onSearchClicked);
    connect(add_btn_,     &QPushButton::clicked,
            this, &AddFriendDialog::onAddClicked);
    connect(result_list_, &QListWidget::currentRowChanged,
            this, [this](int row) {
                add_btn_->setEnabled(row >= 0);
            });

    connect(service_, &ContactService::searchResult,
            this, &AddFriendDialog::onSearchResult);
    connect(service_, &ContactService::operationFailed,
            this, &AddFriendDialog::onOperationFailed);
    connect(service_, &ContactService::contactAdded,
            this, [this](const Contact&) {
                status_label_->setText("已成功添加好友！");
                add_btn_->setEnabled(false);
            });
}

void AddFriendDialog::onSearchClicked() {
    const QString kw = search_edit_->text().trimmed();
    if (kw.isEmpty()) return;
    result_list_->clear();
    results_.clear();
    add_btn_->setEnabled(false);
    status_label_->setText("搜索中...");
    service_->searchUser(kw.toStdString());
}

void AddFriendDialog::onSearchResult(const std::vector<Contact>& users) {
    result_list_->clear();
    results_ = users;
    if (users.empty()) {
        status_label_->setText("未找到匹配用户");
        return;
    }
    status_label_->setText(
        QString("找到 %1 个用户").arg(static_cast<int>(users.size())));
    for (const auto& u : users) {
        result_list_->addItem(
            QString("%1  (%2)")
            .arg(QString::fromStdString(u.display_name))
            .arg(QString::fromStdString(u.username)));
    }
}

void AddFriendDialog::onAddClicked() {
    int row = result_list_->currentRow();
    if (row < 0 || row >= static_cast<int>(results_.size())) return;
    add_btn_->setEnabled(false);
    status_label_->setText("添加中...");
    service_->addFriend(results_[row].user_id);
}

void AddFriendDialog::onOperationFailed(const QString& reason) {
    status_label_->setText(reason);
    add_btn_->setEnabled(true);
}
```

- [ ] **Step 3：写 client/src/ui/ContactPanel.h**

```cpp
#pragma once
#include "core/Contact.h"
#include "service/ContactService.h"
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>
#include <vector>

// 联系人面板
// · 本地过滤搜索（QListWidget 隐藏/显示，无需 server 调用）
// · 添加好友通过 AddFriendDialog
// · 右键菜单删除好友
class ContactPanel : public QWidget {
    Q_OBJECT
public:
    explicit ContactPanel(ContactService* service,
                           QWidget*        parent = nullptr);

signals:
    // 用户点击联系人时发出（供 MainWindow 打开聊天窗口）
    void contactSelected(Contact contact);

private slots:
    void onSearchChanged(const QString& text);
    void onAddFriendClicked();
    void onContactsSynced(const std::vector<Contact>& contacts);
    void onContactAdded  (const Contact& contact);
    void onContactDeleted(const std::string& user_id);
    void onItemDoubleClicked(QListWidgetItem* item);
    void onContextMenuRequested(const QPoint& pos);

private:
    void populateList(const QString& filter = {});

    ContactService*      service_;
    std::vector<Contact> contacts_;   // 镜像 ContactService 缓存

    QLineEdit*   search_edit_;
    QPushButton* add_btn_;
    QListWidget* list_widget_;
    QLabel*      count_label_;
};
```

- [ ] **Step 4：写 client/src/ui/ContactPanel.cpp**

```cpp
#include "ui/ContactPanel.h"
#include "ui/AddFriendDialog.h"
#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidgetItem>

ContactPanel::ContactPanel(ContactService* service, QWidget* parent)
    : QWidget(parent)
    , service_(service)
{
    search_edit_ = new QLineEdit(this);
    add_btn_     = new QPushButton("+ 添加好友", this);
    list_widget_ = new QListWidget(this);
    count_label_ = new QLabel(this);

    search_edit_->setPlaceholderText("搜索联系人...");
    list_widget_->setContextMenuPolicy(Qt::CustomContextMenu);

    auto* top_row = new QHBoxLayout;
    top_row->addWidget(search_edit_);
    top_row->addWidget(add_btn_);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addLayout(top_row);
    layout->addWidget(list_widget_);
    layout->addWidget(count_label_);

    // 从 ContactService 获取初始数据（可能已有本地缓存）
    contacts_ = service_->contacts();
    populateList();

    connect(search_edit_, &QLineEdit::textChanged,
            this, &ContactPanel::onSearchChanged);
    connect(add_btn_, &QPushButton::clicked,
            this, &ContactPanel::onAddFriendClicked);
    connect(list_widget_, &QListWidget::itemDoubleClicked,
            this, &ContactPanel::onItemDoubleClicked);
    connect(list_widget_, &QListWidget::customContextMenuRequested,
            this, &ContactPanel::onContextMenuRequested);
    connect(service_, &ContactService::contactsSynced,
            this, &ContactPanel::onContactsSynced);
    connect(service_, &ContactService::contactAdded,
            this, &ContactPanel::onContactAdded);
    connect(service_, &ContactService::contactDeleted,
            this, &ContactPanel::onContactDeleted);
}

void ContactPanel::populateList(const QString& filter) {
    list_widget_->clear();
    int shown = 0;
    for (const auto& c : contacts_) {
        const QString display = QString::fromStdString(c.display_name);
        const QString name    = QString::fromStdString(c.username);
        if (!filter.isEmpty() &&
            !display.contains(filter, Qt::CaseInsensitive) &&
            !name.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }
        auto* item = new QListWidgetItem(
            QString("%1\n%2").arg(display, name), list_widget_);
        item->setData(Qt::UserRole,
                      QString::fromStdString(c.user_id));
        ++shown;
    }
    count_label_->setText(
        QString("共 %1 位联系人").arg(static_cast<int>(contacts_.size())));
}

void ContactPanel::onSearchChanged(const QString& text) {
    populateList(text.trimmed());
}

void ContactPanel::onAddFriendClicked() {
    auto* dlg = new AddFriendDialog(service_, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
}

void ContactPanel::onContactsSynced(const std::vector<Contact>& contacts) {
    contacts_ = contacts;
    populateList(search_edit_->text().trimmed());
}

void ContactPanel::onContactAdded(const Contact& contact) {
    contacts_.push_back(contact);
    populateList(search_edit_->text().trimmed());
}

void ContactPanel::onContactDeleted(const std::string& user_id) {
    contacts_.erase(
        std::remove_if(contacts_.begin(), contacts_.end(),
            [&user_id](const Contact& c) { return c.user_id == user_id; }),
        contacts_.end());
    populateList(search_edit_->text().trimmed());
}

void ContactPanel::onItemDoubleClicked(QListWidgetItem* item) {
    if (!item) return;
    const QString uid = item->data(Qt::UserRole).toString();
    for (const auto& c : contacts_) {
        if (c.user_id == uid.toStdString()) {
            emit contactSelected(c);
            return;
        }
    }
}

void ContactPanel::onContextMenuRequested(const QPoint& pos) {
    auto* item = list_widget_->itemAt(pos);
    if (!item) return;

    const QString uid = item->data(Qt::UserRole).toString();
    QMenu menu(this);
    auto* del_act = menu.addAction("删除好友");

    connect(del_act, &QAction::triggered, this, [this, uid, item] {
        const QString name = item->text().section('\n', 0, 0);
        auto btn = QMessageBox::question(
            this, "确认删除",
            QString("确定删除好友 %1？").arg(name));
        if (btn == QMessageBox::Yes) {
            service_->deleteFriend(uid.toStdString());
        }
    });

    menu.exec(list_widget_->mapToGlobal(pos));
}
```

- [ ] **Step 5：更新 client/CMakeLists.txt，把新 UI 文件加入 NovaChat_UI**

```cmake
add_library(NovaChat_UI STATIC
    src/ui/MainWindow.cpp
    src/ui/LoginWindow.cpp
    src/ui/RegisterDialog.cpp
    src/ui/ContactPanel.cpp       # ← 新增
    src/ui/AddFriendDialog.cpp    # ← 新增
)
```

- [ ] **Step 6：编译验证**

```
cmake --build --preset windows-debug
```

预期：编译无报错。

- [ ] **Step 7：提交**

```
git add client/src/ui/ContactPanel.h client/src/ui/ContactPanel.cpp
git add client/src/ui/AddFriendDialog.h client/src/ui/AddFriendDialog.cpp
git add client/CMakeLists.txt
git commit -m "feat(ui): 添加 ContactPanel（列表/搜索/删除）和 AddFriendDialog"
```

---

## Task 22：更新 MainWindow 集成 ContactPanel，登录后自动同步

**Files:**
- Modify: `client/src/ui/MainWindow.h`
- Modify: `client/src/ui/MainWindow.cpp`
- Modify: `client/src/main.cpp`

- [ ] **Step 1：更新 client/src/ui/MainWindow.h**

```cpp
#pragma once
#include "core/User.h"
#include "service/AuthService.h"
#include "service/ContactService.h"
#include "ui/ContactPanel.h"
#include <QMainWindow>
#include <QSplitter>

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const User&     user,
                         AuthService*    auth_service,
                         ContactService* contact_service,
                         QWidget*        parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onLogout();

private:
    User            current_user_;
    AuthService*    auth_service_;
    ContactService* contact_service_;
    ContactPanel*   contact_panel_;
};
```

- [ ] **Step 2：更新 client/src/ui/MainWindow.cpp**

```cpp
#include "ui/MainWindow.h"
#include "ui/LoginWindow.h"
#include <QApplication>
#include <QLabel>
#include <QMenuBar>
#include <QSplitter>
#include <QStatusBar>

MainWindow::MainWindow(const User&     user,
                        AuthService*    auth_service,
                        ContactService* contact_service,
                        QWidget*        parent)
    : QMainWindow(parent)
    , current_user_(user)
    , auth_service_(auth_service)
    , contact_service_(contact_service)
{
    setWindowTitle(QString("NovaChat — %1")
                   .arg(QString::fromStdString(user.display_name)));
    setMinimumSize(1000, 660);

    // ── 菜单栏 ─────────────────────────────────────────────────
    auto* file_menu  = menuBar()->addMenu("文件");
    auto* logout_act = file_menu->addAction("退出登录");
    auto* quit_act   = file_menu->addAction("退出程序");
    connect(logout_act, &QAction::triggered, this, &MainWindow::onLogout);
    connect(quit_act,   &QAction::triggered, qApp, &QApplication::quit);

    // ── 布局：左侧联系人面板 | 右侧聊天区域占位 ─────────────────
    contact_panel_ = new ContactPanel(contact_service_, this);

    auto* chat_placeholder = new QLabel("← 在左侧选择联系人开始聊天", this);
    chat_placeholder->setAlignment(Qt::AlignCenter);
    chat_placeholder->setStyleSheet("color: #999; font-size: 14px;");

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(contact_panel_);
    splitter->addWidget(chat_placeholder);
    splitter->setStretchFactor(0, 0);   // 联系人面板固定宽度
    splitter->setStretchFactor(1, 1);   // 聊天区域占满剩余空间
    splitter->setSizes({280, 720});
    setCentralWidget(splitter);

    statusBar()->showMessage(
        QString("已登录：%1").arg(QString::fromStdString(user.username)));
}

void MainWindow::onLogout() {
    auth_service_->logout();

    auto* login_win = new LoginWindow(auth_service_);
    // 重新登录后更新标题和联系人（由 main.cpp 处理窗口生命周期）
    login_win->show();
    close();
}
```

- [ ] **Step 3：更新 client/src/main.cpp（传入 ContactService，登录/自动登录后 syncContacts）**

```cpp
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include "logger/Logger.h"
#include "config/AppConfig.h"
#include "storage/DatabaseManager.h"
#include "storage/UserRepository.h"
#include "storage/ContactRepository.h"
#include "network/TcpClient.h"
#include "service/AuthService.h"
#include "service/ContactService.h"
#include "ui/LoginWindow.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("NovaChat");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("NovaChat");

    Logger::init("novachat.log");
    Logger::info("NovaChat v0.1.0 启动");

    AppConfig::instance().load("config.yaml");

    const QString data_dir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(data_dir);
    const std::string db_path = (data_dir + "/novachat.db").toStdString();

    auto db           = std::make_unique<DatabaseManager>(db_path);
    auto user_repo    = std::make_unique<UserRepository>(*db);
    auto contact_repo = std::make_unique<ContactRepository>(*db);

    auto* tcp_client = new TcpClient(&app);

    // ── 辅助 lambda：登录/自动登录成功后的共同逻辑 ──────────────
    auto launch_main = [&](const User& user) -> MainWindow* {
        auto* contact_svc = new ContactService(
            tcp_client, *contact_repo, user, &app);
        auto* auth_svc    = new AuthService(
            tcp_client, *user_repo, &app);

        auto* win = new MainWindow(user, auth_svc, contact_svc);
        win->show();

        // 连接成功后同步联系人（首次连接或重连后）
        QObject::connect(tcp_client, &TcpClient::connected,
                         contact_svc, &ContactService::syncContacts);
        return win;
    };

    auto active = user_repo->findActive();

    if (active) {
        Logger::info("Auto-login: {}", active->username);
        User user;
        user.server_id    = active->server_id;
        user.username     = active->username;
        user.display_name = active->display_name;
        user.avatar_url   = active->avatar_url;
        user.token        = active->token;
        launch_main(user);
    } else {
        auto* auth_svc   = new AuthService(tcp_client, *user_repo, &app);
        auto* login_win  = new LoginWindow(auth_svc);

        QObject::connect(login_win, &LoginWindow::loginSucceeded,
                         [&launch_main](User user) {
                             launch_main(user);
                         });
        login_win->show();
    }

    tcp_client->connectToServer(AppConfig::instance().server.host,
                                 AppConfig::instance().server.port);

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

- [ ] **Step 5：端到端联系人流程验证**

**启动服务端：**
```
./build/debug/server/NovaChatServer.exe
```

**启动两个客户端实例（用两个不同用户）注册并互加好友：**

客户端 A：
1. 注册用户名 `alice` / 昵称 `Alice`，登录
2. 主窗口左侧显示空联系人列表
3. 点击「+ 添加好友」→ 搜索 `bob` → 点「添加为好友」
4. ContactPanel 立即显示 Bob 的名字

客户端 B：
1. 注册用户名 `bob` / 昵称 `Bob`，登录
2. 主窗口左侧联系人列表显示 Alice（好友关系双向生效）

验证删除好友：
- 右键 Alice → 「删除好友」→ 确认 → Alice 从列表消失
- 服务端日志显示 `Friend removed`

- [ ] **Step 6：运行全部测试**

```
cmake --build --preset windows-debug
ctest --preset windows-debug -V
```

预期：31 个测试全部 PASS。

- [ ] **Step 7：提交**

```
git add client/src/ui/MainWindow.h client/src/ui/MainWindow.cpp
git add client/src/main.cpp
git commit -m "feat: 完成联系人模块——好友列表/添加/删除/搜索，登录后自动同步"
```

---

## 阶段四完成标准

- [ ] `ctest --preset windows-debug` 31 个测试全部 PASS
- [ ] 登录后 MainWindow 左侧显示 ContactPanel
- [ ] ContactPanel 支持本地搜索过滤（无需 server 调用）
- [ ] 添加好友：AddFriendDialog 搜索 → 点添加 → 好友出现在列表
- [ ] 删除好友：右键菜单确认后从列表消失
- [ ] 重启后联系人从本地 SQLite 恢复（无需重新同步）
- [ ] 服务端日志可见好友关系操作记录
- [ ] 所有代码已提交到 git
