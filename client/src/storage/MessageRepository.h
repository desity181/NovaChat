#pragma once
#include "core/Message.h"
#include "storage/DatabaseManager.h"
#include <optional>
#include <string>
#include <vector>

// Operations on the messages table.
// All methods must be called from the owning (DB) thread.
class MessageRepository {
public:
    explicit MessageRepository(DatabaseManager& db);

    // Insert a message (ON CONFLICT IGNORE — duplicate message_id is silently skipped).
    void save(const Message& msg);

    // Replace a local temp ID with the server-assigned ID and mark status=Sent.
    void confirmSent(const std::string& local_id,
                     const std::string& server_id,
                     int64_t            server_timestamp_ms);

    // Update message status (0=Sending, 1=Sent, 2=Failed).
    void updateStatus(const std::string& message_id, int status);

    // Return messages for a conversation in ascending order.
    // before_timestamp_ms == 0: fetch the newest page.
    // before_timestamp_ms > 0: fetch messages older than this timestamp (pagination).
    std::vector<Message> findByConversation(const std::string& conversation_id,
                                             int64_t before_timestamp_ms = 0,
                                             int     limit               = 50);

    std::optional<Message> findById(const std::string& message_id);

private:
    DatabaseManager& db_;
    static Message rowToMessage(sqlite3_stmt* stmt);
};
