#include "handler/ChatHandler.h"
#include "proto/MessageType.h"
#include "chat.pb.h"
#include "common.pb.h"
#include <algorithm>
#include <spdlog/spdlog.h>

ChatHandler::ChatHandler(ServerStorage& storage) : storage_(storage) {}

int64_t ChatHandler::nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

std::string ChatHandler::makeConvId(const std::string& a,
                                     const std::string& b) {
    return (a < b) ? (a + ":" + b) : (b + ":" + a);
}

void ChatHandler::handle(std::shared_ptr<ClientSession> session,
                          const Packet& packet) {
    switch (packet.type) {
    case MessageType::SendMessageRequest:
        onSendMessage(session, packet);     break;
    case MessageType::GetHistoryRequest:
        onGetHistory(session, packet);      break;
    case MessageType::GetConversationsRequest:
        onGetConversations(session, packet); break;
    default: break;
    }
}

std::optional<ServerStorage::UserRecord>
ChatHandler::validateToken(std::shared_ptr<ClientSession> session,
                            const std::string& token, uint32_t seq_id) {
    auto user = storage_.findByToken(token);
    if (!user) sendError(session, seq_id, novachat::AUTH_EXPIRED, "Token invalid");
    return user;
}

void ChatHandler::sendError(std::shared_ptr<ClientSession> session,
                             uint32_t seq_id, int code,
                             const std::string& msg) {
    novachat::ErrorResponse er;
    er.set_code(static_cast<novachat::ErrorCode>(code));
    er.set_message(msg);
    Packet resp;
    resp.type   = MessageType::ErrorResponse;
    resp.seq_id = seq_id;
    resp.payload.resize(er.ByteSizeLong());
    er.SerializeToArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));
}

void ChatHandler::onSendMessage(std::shared_ptr<ClientSession> session,
                                 const Packet& pkt) {
    novachat::SendMessageRequest req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()))) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }
    auto sender = validateToken(session, req.token(), pkt.seq_id);
    if (!sender) return;

    const std::string conv_id = makeConvId(sender->user_id, req.receiver_id());
    const int64_t     ts      = nowMs();

    ServerStorage::MsgRecord record;
    record.conversation_id = conv_id;
    record.sender_id       = sender->user_id;
    record.content         = req.content();
    record.timestamp_ms    = ts;
    const std::string server_id = storage_.saveMessage(record);

    // ── Reply to sender ───────────────────────────────────────────
    novachat::SendMessageResponse resp_pb;
    resp_pb.set_local_msg_id   (req.local_msg_id());
    resp_pb.set_server_msg_id  (server_id);
    resp_pb.set_conversation_id(conv_id);
    resp_pb.set_timestamp_ms   (ts);

    Packet resp;
    resp.type   = MessageType::SendMessageResponse;
    resp.seq_id = pkt.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));

    // ── Push to receiver if online ────────────────────────────────
    auto recv_session = storage_.findSession(req.receiver_id());
    if (recv_session) {
        novachat::MessagePush push;
        push.set_message_id     (server_id);
        push.set_conversation_id(conv_id);
        push.set_sender_id      (sender->user_id);
        push.set_content        (req.content());
        push.set_timestamp_ms   (ts);

        Packet push_pkt;
        push_pkt.type   = MessageType::MessagePush;
        push_pkt.seq_id = 0;  // Server-initiated push
        push_pkt.payload.resize(push.ByteSizeLong());
        push.SerializeToArray(push_pkt.payload.data(),
                              static_cast<int>(push_pkt.payload.size()));
        recv_session->sendPacket(std::move(push_pkt));
    }

    spdlog::info("Message {} -> {} (conv={})",
                 sender->username, req.receiver_id(), conv_id);
}

void ChatHandler::onGetHistory(std::shared_ptr<ClientSession> session,
                                const Packet& pkt) {
    novachat::GetHistoryRequest req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()))) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }
    auto user = validateToken(session, req.token(), pkt.seq_id);
    if (!user) return;

    const int limit = std::clamp(req.limit(), 1, 50);
    auto msgs = storage_.getHistory(req.conversation_id(),
                                    req.before_timestamp(), limit);

    novachat::GetHistoryResponse resp_pb;
    for (const auto& m : msgs) {
        auto* r = resp_pb.add_messages();
        r->set_message_id     (m.message_id);
        r->set_conversation_id(m.conversation_id);
        r->set_sender_id      (m.sender_id);
        r->set_content        (m.content);
        r->set_timestamp_ms   (m.timestamp_ms);
    }
    resp_pb.set_has_more(static_cast<int>(msgs.size()) == limit);

    Packet resp;
    resp.type   = MessageType::GetHistoryResponse;
    resp.seq_id = pkt.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));
}

void ChatHandler::onGetConversations(std::shared_ptr<ClientSession> session,
                                      const Packet& pkt) {
    novachat::GetConversationsRequest req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()))) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }
    auto user = validateToken(session, req.token(), pkt.seq_id);
    if (!user) return;

    auto latest_msgs = storage_.getLatestMessages(user->user_id);

    novachat::GetConversationsResponse resp_pb;
    for (const auto& m : latest_msgs) {
        auto* r = resp_pb.add_conversations();
        r->set_conversation_id(m.conversation_id);
        const auto colon = m.conversation_id.find(':');
        const std::string id_a = m.conversation_id.substr(0, colon);
        const std::string id_b = m.conversation_id.substr(colon + 1);
        r->set_peer_user_id(id_a == user->user_id ? id_b : id_a);
        r->set_last_message_preview(m.content.size() > 50
            ? m.content.substr(0, 50) + "..." : m.content);
        r->set_last_message_time(m.timestamp_ms);
    }

    Packet resp;
    resp.type   = MessageType::GetConversationsResponse;
    resp.seq_id = pkt.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));
}
