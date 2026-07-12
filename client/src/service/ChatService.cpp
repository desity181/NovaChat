#include "service/ChatService.h"
#include "logger/Logger.h"
#include "proto/MessageType.h"
#include "chat.pb.h"
#include "common.pb.h"
#include <algorithm>
#include <chrono>

// Helper: emit authExpired if code matches, returns true if it was AUTH_EXPIRED.
static bool checkAuthExpired(ChatService* svc, const novachat::ErrorResponse& er) {
    if (er.code() == novachat::AUTH_EXPIRED) {
        emit svc->authExpired();
        return true;
    }
    return false;
}

using namespace std::chrono;

static int64_t nowMs() {
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

ChatService::ChatService(TcpClient*              client,
                          MessageRepository&      msg_repo,
                          ConversationRepository& conv_repo,
                          const User&             current_user,
                          QObject*                parent)
    : QObject(parent)
    , client_(client)
    , msg_repo_(msg_repo)
    , conv_repo_(conv_repo)
    , current_user_(current_user)
{
    conversations_ = conv_repo_.findAll(current_user_.server_id);

    // Handle server-initiated pushes (seq_id == 0) delivered via TcpClient::packetReceived
    connect(client_, &TcpClient::packetReceived,
            this,    &ChatService::onPacketReceived,
            Qt::QueuedConnection);
}

std::string ChatService::makeConvId(const std::string& a,
                                     const std::string& b) {
    return (a < b) ? (a + ":" + b) : (b + ":" + a);
}

void ChatService::sendMessage(const std::string& receiver_id,
                               const std::string& content) {
    const std::string conv_id =
        makeConvId(current_user_.server_id, receiver_id);

    const std::string local_id =
        "local_" + std::to_string(++local_id_counter_)
        + "_" + std::to_string(nowMs());

    // Optimistic local write (Sending status)
    Message local_msg;
    local_msg.message_id      = local_id;
    local_msg.conversation_id = conv_id;
    local_msg.sender_id       = current_user_.server_id;
    local_msg.content         = content;
    local_msg.type            = MessageContentType::Text;
    local_msg.timestamp_ms    = nowMs();
    local_msg.status          = MessageStatus::Sending;
    msg_repo_.save(local_msg);

    // Update conversation preview
    Conversation conv;
    conv.owner_id             = current_user_.server_id;
    conv.conversation_id      = conv_id;
    conv.target_id            = receiver_id;
    conv.last_message_preview = content.size() > 50
                                ? content.substr(0, 50) + "..." : content;
    conv.last_message_time    = local_msg.timestamp_ms;
    conv.unread_count         = 0;
    conv_repo_.saveOrUpdate(conv);

    emit messageSent(local_msg);

    novachat::SendMessageRequest req;
    req.set_token       (current_user_.token);
    req.set_local_msg_id(local_id);
    req.set_receiver_id (receiver_id);
    req.set_content     (content);

    Packet pkt;
    pkt.type = MessageType::SendMessageRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this, local_id](Packet resp) {
        if (resp.type == MessageType::SendMessageResponse) {
            novachat::SendMessageResponse r;
            if (!r.ParseFromArray(resp.payload.data(),
                                  static_cast<int>(resp.payload.size()))) return;
            msg_repo_.confirmSent(local_id, r.server_msg_id(), r.timestamp_ms());
            Logger::debug("Message confirmed: {} -> {}", local_id, r.server_msg_id());
        } else if (resp.type == MessageType::ErrorResponse) {
            novachat::ErrorResponse er;
            er.ParseFromArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
            msg_repo_.updateStatus(local_id, static_cast<int>(MessageStatus::Failed));
            emit messageFailed(local_id);
            checkAuthExpired(this, er);
        }
    });
}

void ChatService::loadHistory(const std::string& conv_id,
                               int64_t before_ts) {
    novachat::GetHistoryRequest req;
    req.set_token           (current_user_.token);
    req.set_conversation_id (conv_id);
    req.set_before_timestamp(before_ts);
    req.set_limit           (50);

    Packet pkt;
    pkt.type = MessageType::GetHistoryRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this, conv_id](Packet resp) {
        if (resp.type != MessageType::GetHistoryResponse) return;

        novachat::GetHistoryResponse r;
        if (!r.ParseFromArray(resp.payload.data(),
                              static_cast<int>(resp.payload.size()))) return;

        std::vector<Message> msgs;
        msgs.reserve(r.messages_size());
        for (const auto& m : r.messages()) {
            Message msg;
            msg.message_id      = m.message_id();
            msg.conversation_id = m.conversation_id();
            msg.sender_id       = m.sender_id();
            msg.content         = m.content();
            msg.type            = MessageContentType::Text;
            msg.timestamp_ms    = m.timestamp_ms();
            msg.status          = MessageStatus::Sent;
            msg_repo_.save(msg);
            msgs.push_back(msg);
        }

        emit historyLoaded(conv_id, msgs, r.has_more());
    });
}

void ChatService::syncConversations() {
    novachat::GetConversationsRequest req;
    req.set_token(current_user_.token);

    Packet pkt;
    pkt.type = MessageType::GetConversationsRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this](Packet resp) {
        if (resp.type != MessageType::GetConversationsResponse) return;

        novachat::GetConversationsResponse r;
        if (!r.ParseFromArray(resp.payload.data(),
                              static_cast<int>(resp.payload.size()))) return;

        for (const auto& c : r.conversations()) {
            Conversation conv;
            conv.owner_id             = current_user_.server_id;
            conv.conversation_id      = c.conversation_id();
            conv.target_id            = c.peer_user_id();
            conv.last_message_preview = c.last_message_preview();
            conv.last_message_time    = c.last_message_time();
            conv.unread_count         = 0;
            conv_repo_.saveOrUpdate(conv);
        }

        conversations_ = conv_repo_.findAll(current_user_.server_id);
        emit conversationsLoaded(conversations_);
        Logger::info("Conversations synced: {}", conversations_.size());
    });
}

void ChatService::clearUnread(const std::string& conv_id) {
    conv_repo_.clearUnread(current_user_.server_id, conv_id);
}

void ChatService::onPacketReceived(Packet pkt) {
    if (pkt.type == MessageType::MessagePush) {
        handleMessagePush(pkt);
    }
}

void ChatService::handleMessagePush(const Packet& pkt) {
    novachat::MessagePush push;
    if (!push.ParseFromArray(pkt.payload.data(),
                             static_cast<int>(pkt.payload.size()))) return;

    Message msg;
    msg.message_id      = push.message_id();
    msg.conversation_id = push.conversation_id();
    msg.sender_id       = push.sender_id();
    msg.content         = push.content();
    msg.type            = MessageContentType::Text;
    msg.timestamp_ms    = push.timestamp_ms();
    msg.status          = MessageStatus::Sent;

    msg_repo_.save(msg);
    conv_repo_.updateLastMessage(current_user_.server_id,
                                  msg.conversation_id,
                                  msg.content, msg.timestamp_ms);
    conv_repo_.incrementUnread(current_user_.server_id, msg.conversation_id);

    conversations_ = conv_repo_.findAll(current_user_.server_id);

    Logger::debug("Message push received from {}", msg.sender_id);
    emit messageReceived(msg);
}
