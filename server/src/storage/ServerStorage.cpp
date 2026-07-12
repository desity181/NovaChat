#include "storage/ServerStorage.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <random>
#include <sstream>

// ── Random hex generation ─────────────────────────────────────────────────
std::string ServerStorage::generateHex(size_t bytes) {
    static std::mutex        rng_mutex;
    static std::random_device rd;
    static std::mt19937_64   gen(rd());
    // MSVC disallows uniform_int_distribution<uint8_t>; use uint32_t and mask
    std::uniform_int_distribution<uint32_t> dis(0, 255);

    std::lock_guard<std::mutex> lock(rng_mutex);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytes; ++i) {
        oss << std::setw(2) << (dis(gen) & 0xFFU);
    }
    return oss.str();
}

std::string ServerStorage::generateId()    { return "u_"  + generateHex(8);  }
std::string ServerStorage::generateToken() { return "tk_" + generateHex(16); }

// ── Auth ──────────────────────────────────────────────────────────────────
std::optional<ServerStorage::UserRecord>
ServerStorage::registerUser(const std::string& username,
                             const std::string& password,
                             const std::string& display_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (username_to_id_.count(username)) return std::nullopt;
    UserRecord rec;
    rec.user_id      = generateId();
    rec.username     = username;
    rec.display_name = display_name;
    rec.password     = password;
    users_by_id_[rec.user_id] = rec;
    username_to_id_[username] = rec.user_id;
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

// ── Friend relationships ──────────────────────────────────────────────────
bool ServerStorage::addFriend(const std::string& user_id,
                               const std::string& friend_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!users_by_id_.count(user_id) || !users_by_id_.count(friend_id)) return false;
    auto [it, inserted] = friendships_[user_id].insert(friend_id);
    if (!inserted) return false;
    friendships_[friend_id].insert(user_id);
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

// ── Online session registry ───────────────────────────────────────────────
void ServerStorage::registerSession(const std::string& user_id,
                                     std::shared_ptr<ClientSession> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_sessions_[user_id] = session;
}

void ServerStorage::unregisterSession(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_sessions_.erase(user_id);
}

std::shared_ptr<ClientSession>
ServerStorage::findSession(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = active_sessions_.find(user_id);
    if (it == active_sessions_.end()) return nullptr;
    return it->second.lock();
}

// ── Message storage ───────────────────────────────────────────────────────
std::string ServerStorage::saveMessage(MsgRecord msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    msg.message_id = generateHex(8);
    messages_[msg.conversation_id].push_back(msg);
    return msg.message_id;
}

std::vector<ServerStorage::MsgRecord>
ServerStorage::getHistory(const std::string& conv_id,
                           int64_t before_ts, int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = messages_.find(conv_id);
    if (it == messages_.end()) return {};

    const auto& msgs = it->second;
    std::vector<MsgRecord> result;
    for (int i = static_cast<int>(msgs.size()) - 1; i >= 0; --i) {
        if (before_ts > 0 && msgs[i].timestamp_ms >= before_ts) continue;
        result.push_back(msgs[i]);
        if (static_cast<int>(result.size()) >= limit) break;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

std::vector<ServerStorage::MsgRecord>
ServerStorage::getLatestMessages(const std::string& user_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<MsgRecord> latest;
    for (const auto& [conv_id, msgs] : messages_) {
        if (conv_id.find(user_id) == std::string::npos) continue;
        if (!msgs.empty()) latest.push_back(msgs.back());
    }
    std::sort(latest.begin(), latest.end(),
              [](const MsgRecord& a, const MsgRecord& b) {
                  return a.timestamp_ms > b.timestamp_ms;
              });
    return latest;
}

// ── Persistence ───────────────────────────────────────────────────────────

void ServerStorage::loadFromFile(const std::string& path) {
    persist_path_ = path;
    std::ifstream f(path);
    if (!f.is_open()) return;  // First run — no file yet

    try {
        nlohmann::json j;
        f >> j;
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& entry : j) {
            UserRecord rec;
            rec.user_id      = entry.at("user_id").get<std::string>();
            rec.username     = entry.at("username").get<std::string>();
            rec.display_name = entry.at("display_name").get<std::string>();
            rec.password     = entry.at("password").get<std::string>();
            users_by_id_[rec.user_id]  = rec;
            username_to_id_[rec.username] = rec.user_id;
        }
    } catch (const std::exception&) {
        // Malformed file — start fresh
    }
}

void ServerStorage::saveToFile() const {
    if (persist_path_.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j = nlohmann::json::array();
    for (const auto& [id, rec] : users_by_id_) {
        j.push_back({
            {"user_id",      rec.user_id},
            {"username",     rec.username},
            {"display_name", rec.display_name},
            {"password",     rec.password},
        });
    }
    std::ofstream f(persist_path_);
    f << j.dump(2);
}
