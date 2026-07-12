#pragma once
#include <string>
#include <cstdint>
#include <QMetaType>

struct Conversation {
    std::string conversation_id;
    std::string owner_id;
    std::string target_id;
    std::string last_message_preview;
    int64_t     last_message_time{0};
    int         unread_count{0};
};

Q_DECLARE_METATYPE(Conversation)
