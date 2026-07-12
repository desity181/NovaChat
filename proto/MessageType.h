#pragma once
#include <cstdint>

// Message type field (MsgId, 2 bytes) in the binary frame header
enum class MessageType : uint16_t {
    // 0x00xx  Auth
    RegisterRequest     = 0x0001,
    RegisterResponse    = 0x0002,
    LoginRequest        = 0x0003,
    LoginResponse       = 0x0004,
    LogoutRequest       = 0x0005,

    // 0x01xx  Contact
    GetContactsRequest   = 0x0101,
    GetContactsResponse  = 0x0102,
    AddFriendRequest     = 0x0103,
    AddFriendResponse    = 0x0104,
    DeleteFriendRequest  = 0x0105,
    DeleteFriendResponse = 0x0106,
    SearchUserRequest    = 0x0107,
    SearchUserResponse   = 0x0108,

    // 0x02xx  Chat
    SendMessageRequest       = 0x0201,
    SendMessageResponse      = 0x0202,
    MessagePush              = 0x0203,  // Server-initiated push
    GetHistoryRequest        = 0x0204,
    GetHistoryResponse       = 0x0205,
    GetConversationsRequest  = 0x0206,
    GetConversationsResponse = 0x0207,

    // 0x0Fxx  System
    HeartbeatRequest  = 0x0F01,
    HeartbeatResponse = 0x0F02,
    ErrorResponse     = 0x0FFF,
};
