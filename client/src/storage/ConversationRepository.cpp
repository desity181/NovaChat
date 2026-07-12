#include "storage/ConversationRepository.h"
#include <sqlite3.h>

ConversationRepository::ConversationRepository(DatabaseManager& db) : db_(db) {}

Conversation ConversationRepository::rowToConversation(sqlite3_stmt* stmt) {
    Conversation c;
    c.owner_id            = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    c.conversation_id     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    c.target_id           = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    const unsigned char* p = sqlite3_column_text(stmt, 3);
    c.last_message_preview = p ? reinterpret_cast<const char*>(p) : "";
    c.last_message_time   = sqlite3_column_int64(stmt, 4);
    c.unread_count        = sqlite3_column_int  (stmt, 5);
    return c;
}

void ConversationRepository::saveOrUpdate(const Conversation& c) {
    const char* sql = R"(
        INSERT INTO conversations
            (owner_id, conversation_id, target_id,
             last_message_preview, last_message_time, unread_count)
        VALUES (?,?,?,?,?,?)
        ON CONFLICT(owner_id, conversation_id) DO UPDATE SET
            target_id             = excluded.target_id,
            last_message_preview  = excluded.last_message_preview,
            last_message_time     = excluded.last_message_time,
            unread_count          = excluded.unread_count
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text (stmt, 1, c.owner_id.c_str(),             -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 2, c.conversation_id.c_str(),      -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 3, c.target_id.c_str(),            -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 4, c.last_message_preview.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, c.last_message_time);
    sqlite3_bind_int  (stmt, 6, c.unread_count);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void ConversationRepository::updateLastMessage(const std::string& owner_id,
                                                const std::string& conv_id,
                                                const std::string& preview,
                                                int64_t ts) {
    const char* sql = R"(
        UPDATE conversations
        SET last_message_preview = ?, last_message_time = ?
        WHERE owner_id = ? AND conversation_id = ?
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text (stmt, 1, preview.c_str(),  -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, ts);
    sqlite3_bind_text (stmt, 3, owner_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text (stmt, 4, conv_id.c_str(),  -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void ConversationRepository::incrementUnread(const std::string& owner_id,
                                              const std::string& conv_id) {
    const char* sql = R"(
        UPDATE conversations SET unread_count = unread_count + 1
        WHERE owner_id = ? AND conversation_id = ?
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, owner_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, conv_id.c_str(),  -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void ConversationRepository::clearUnread(const std::string& owner_id,
                                          const std::string& conv_id) {
    const char* sql = R"(
        UPDATE conversations SET unread_count = 0
        WHERE owner_id = ? AND conversation_id = ?
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, owner_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, conv_id.c_str(),  -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<Conversation>
ConversationRepository::findAll(const std::string& owner_id) {
    const char* sql = R"(
        SELECT owner_id, conversation_id, target_id,
               last_message_preview, last_message_time, unread_count
        FROM conversations WHERE owner_id = ?
        ORDER BY last_message_time DESC
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, owner_id.c_str(), -1, SQLITE_STATIC);

    std::vector<Conversation> result;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        result.push_back(rowToConversation(stmt));
    sqlite3_finalize(stmt);
    return result;
}
