#pragma once
#include "network/ClientSession.h"
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// User store for the test server.
// User registrations are persisted to a JSON file so data survives server restarts.
// Messages and sessions are in-memory only (reset on restart).
// All public methods are thread-safe.
class ServerStorage {
public:
    struct UserRecord {
        std::string user_id;
        std::string username;
        std::string display_name;
        std::string password;   // Plain-text only for this test server
    };

    struct MsgRecord {
        std::string message_id;
        std::string conversation_id;
        std::string sender_id;
        std::string content;
        int64_t     timestamp_ms{};
    };

    // ── Persistence ──────────────────────────────────────────────────
    // Load user registrations from a JSON file (call once on startup).
    // Silently succeeds if the file does not exist yet.
    void loadFromFile(const std::string& path);

    // Persist user registrations to the same file used by loadFromFile.
    void saveToFile() const;

    // ── Auth ─────────────────────────────────────────────────────────
    std::optional<UserRecord> registerUser(const std::string& username,
                                           const std::string& password,
                                           const std::string& display_name);
    std::optional<UserRecord> authenticate(const std::string& username,
                                           const std::string& password);
    std::string createSession(const std::string& user_id);
    std::optional<UserRecord> findByToken(const std::string& token);
    void removeSession(const std::string& token);

    // ── Friend relationships (V1: immediately bidirectional) ─────────
    bool addFriend(const std::string& user_id, const std::string& friend_id);
    bool removeFriend(const std::string& user_id, const std::string& friend_id);
    std::vector<UserRecord> getFriends(const std::string& user_id);
    std::vector<UserRecord> searchUsers(const std::string& keyword);
    std::optional<UserRecord> findById(const std::string& user_id);

    // ── Online session registry (for message push) ───────────────────
    void registerSession  (const std::string& user_id,
                           std::shared_ptr<ClientSession> session);
    void unregisterSession(const std::string& user_id);
    std::shared_ptr<ClientSession> findSession(const std::string& user_id);

    // ── Message storage ──────────────────────────────────────────────
    std::string saveMessage(MsgRecord msg);
    std::vector<MsgRecord> getHistory(const std::string& conv_id,
                                       int64_t before_ts, int limit);
    std::vector<MsgRecord> getLatestMessages(const std::string& user_id);

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, UserRecord>  users_by_id_;
    std::unordered_map<std::string, std::string> username_to_id_;
    std::unordered_map<std::string, std::string> token_to_user_id_;
    std::unordered_map<std::string, std::unordered_set<std::string>> friendships_;
    std::unordered_map<std::string, std::weak_ptr<ClientSession>>    active_sessions_;
    std::unordered_map<std::string, std::vector<MsgRecord>>          messages_;

    std::string persist_path_;

    static std::string generateId();
    static std::string generateToken();
    static std::string generateHex(size_t bytes);
};
