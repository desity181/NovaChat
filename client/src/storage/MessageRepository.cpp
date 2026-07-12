#include "storage/MessageRepository.h"
#include <algorithm>
#include <sqlite3.h>

MessageRepository::MessageRepository(DatabaseManager& db) : db_(db) {}

Message MessageRepository::rowToMessage(sqlite3_stmt* stmt) {
    Message m;
    m.message_id      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    m.conversation_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    m.sender_id       = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    m.content         = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    m.type            = static_cast<MessageContentType>(sqlite3_column_int(stmt, 4));
    m.timestamp_ms    = sqlite3_column_int64(stmt, 5);
    m.status          = static_cast<MessageStatus>(sqlite3_column_int(stmt, 6));
    return m;
}

void MessageRepository::save(const Message& m) {
    const char* sql = R"(
        INSERT OR IGNORE INTO messages
            (message_id, conversation_id, sender_id, content,
             msg_type, timestamp, status)
        VALUES (?,?,?,?,?,?,?)
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text (stmt, 1, m.message_id.c_str(),      -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 2, m.conversation_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 3, m.sender_id.c_str(),       -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 4, m.content.c_str(),         -1, SQLITE_STATIC);
    sqlite3_bind_int  (stmt, 5, static_cast<int>(m.type));
    sqlite3_bind_int64(stmt, 6, m.timestamp_ms);
    sqlite3_bind_int  (stmt, 7, static_cast<int>(m.status));
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void MessageRepository::confirmSent(const std::string& local_id,
                                     const std::string& server_id,
                                     int64_t            server_ts) {
    const char* sql = R"(
        UPDATE messages
        SET message_id = ?, timestamp = ?, status = 1
        WHERE message_id = ?
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text (stmt, 1, server_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, server_ts);
    sqlite3_bind_text (stmt, 3, local_id.c_str(),  -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void MessageRepository::updateStatus(const std::string& message_id, int status) {
    const char* sql = "UPDATE messages SET status = ? WHERE message_id = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_int (stmt, 1, status);
    sqlite3_bind_text(stmt, 2, message_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<Message>
MessageRepository::findByConversation(const std::string& conv_id,
                                       int64_t before_ts,
                                       int limit) {
    // Query descending then reverse to present ascending order
    const char* sql_latest = R"(
        SELECT message_id, conversation_id, sender_id, content,
               msg_type, timestamp, status
        FROM messages WHERE conversation_id = ?
        ORDER BY timestamp DESC LIMIT ?
    )";
    const char* sql_before = R"(
        SELECT message_id, conversation_id, sender_id, content,
               msg_type, timestamp, status
        FROM messages WHERE conversation_id = ? AND timestamp < ?
        ORDER BY timestamp DESC LIMIT ?
    )";

    sqlite3_stmt* stmt;
    if (before_ts == 0) {
        sqlite3_prepare_v2(db_.handle(), sql_latest, -1, &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, conv_id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int (stmt, 2, limit);
    } else {
        sqlite3_prepare_v2(db_.handle(), sql_before, -1, &stmt, nullptr);
        sqlite3_bind_text (stmt, 1, conv_id.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 2, before_ts);
        sqlite3_bind_int  (stmt, 3, limit);
    }

    std::vector<Message> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(rowToMessage(stmt));
    }
    sqlite3_finalize(stmt);
    std::reverse(result.begin(), result.end());  // DESC → ASC
    return result;
}

std::optional<Message>
MessageRepository::findById(const std::string& message_id) {
    const char* sql = R"(
        SELECT message_id, conversation_id, sender_id, content,
               msg_type, timestamp, status
        FROM messages WHERE message_id = ? LIMIT 1
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, message_id.c_str(), -1, SQLITE_STATIC);
    std::optional<Message> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = rowToMessage(stmt);
    sqlite3_finalize(stmt);
    return result;
}
