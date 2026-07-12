#pragma once
#include "core/Conversation.h"
#include "storage/DatabaseManager.h"
#include <string>
#include <vector>

// Operations on the conversations table.
// All methods must be called from the owning (DB) thread.
class ConversationRepository {
public:
    explicit ConversationRepository(DatabaseManager& db);

    // Insert or update (conflict on owner_id + conversation_id updates all fields).
    void saveOrUpdate(const Conversation& conv);

    // Update the last-message preview and timestamp.
    void updateLastMessage(const std::string& owner_id,
                           const std::string& conversation_id,
                           const std::string& preview,
                           int64_t            timestamp_ms);

    // Increment the unread count by 1.
    void incrementUnread(const std::string& owner_id,
                         const std::string& conversation_id);

    // Reset the unread count to 0.
    void clearUnread(const std::string& owner_id,
                     const std::string& conversation_id);

    // Return all conversations for owner_id, ordered by last_message_time descending.
    std::vector<Conversation> findAll(const std::string& owner_id);

private:
    DatabaseManager& db_;
    static Conversation rowToConversation(sqlite3_stmt* stmt);
};
