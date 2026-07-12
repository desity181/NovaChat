#pragma once
#include <string>
#include <cstdint>
#include <QMetaType>

enum class MessageStatus : int {
    Sending = 0,
    Sent    = 1,
    Failed  = 2,
};

// Content type for a chat message (distinct from the protocol-level MessageType enum)
enum class MessageContentType : int {
    Text  = 1,
    Image = 2,
    File  = 3,
};

struct Message {
    std::string        message_id;
    std::string        conversation_id;
    std::string        sender_id;
    std::string        content;
    MessageContentType type{MessageContentType::Text};
    int64_t            timestamp_ms{0};
    MessageStatus      status{MessageStatus::Sending};
};

Q_DECLARE_METATYPE(Message)
