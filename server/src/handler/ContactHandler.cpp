#include "handler/ContactHandler.h"
#include "proto/MessageType.h"
#include "contact.pb.h"
#include "common.pb.h"
#include <spdlog/spdlog.h>

ContactHandler::ContactHandler(ServerStorage& storage)
    : storage_(storage) {}

void ContactHandler::handle(std::shared_ptr<ClientSession> session,
                             const Packet& packet) {
    switch (packet.type) {
    case MessageType::GetContactsRequest:  onGetContacts (session, packet); break;
    case MessageType::AddFriendRequest:    onAddFriend   (session, packet); break;
    case MessageType::DeleteFriendRequest: onDeleteFriend(session, packet); break;
    case MessageType::SearchUserRequest:   onSearchUser  (session, packet); break;
    default: break;
    }
}

std::optional<ServerStorage::UserRecord>
ContactHandler::validateToken(std::shared_ptr<ClientSession> session,
                               const std::string& token, uint32_t seq_id) {
    auto user = storage_.findByToken(token);
    if (!user) {
        sendError(session, seq_id, novachat::AUTH_EXPIRED, "Token invalid or expired");
    }
    return user;
}

void ContactHandler::sendError(std::shared_ptr<ClientSession> session,
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

static novachat::ContactInfo toContactInfo(const ServerStorage::UserRecord& rec) {
    novachat::ContactInfo ci;
    ci.set_user_id(rec.user_id);
    ci.set_username(rec.username);
    ci.set_display_name(rec.display_name);
    return ci;
}

void ContactHandler::onGetContacts(std::shared_ptr<ClientSession> session,
                                    const Packet& pkt) {
    novachat::GetContactsRequest req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()))) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }
    auto user = validateToken(session, req.token(), pkt.seq_id);
    if (!user) return;

    auto friends = storage_.getFriends(user->user_id);

    novachat::GetContactsResponse resp_pb;
    for (const auto& f : friends) {
        *resp_pb.add_contacts() = toContactInfo(f);
    }

    Packet resp;
    resp.type   = MessageType::GetContactsResponse;
    resp.seq_id = pkt.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));

    spdlog::debug("GetContacts: {} returned {} contacts",
                  user->username, friends.size());
}

void ContactHandler::onAddFriend(std::shared_ptr<ClientSession> session,
                                  const Packet& pkt) {
    novachat::AddFriendRequest req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()))) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }
    auto user = validateToken(session, req.token(), pkt.seq_id);
    if (!user) return;

    if (user->user_id == req.target_user_id()) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "Cannot add yourself");
        return;
    }

    bool ok = storage_.addFriend(user->user_id, req.target_user_id());
    if (!ok) {
        sendError(session, pkt.seq_id,
                  novachat::FRIEND_ALREADY_EXISTS, "Already friends or user not found");
        return;
    }

    auto target = storage_.findById(req.target_user_id());

    novachat::AddFriendResponse resp_pb;
    if (target) *resp_pb.mutable_contact() = toContactInfo(*target);

    Packet resp;
    resp.type   = MessageType::AddFriendResponse;
    resp.seq_id = pkt.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));

    spdlog::info("Friend added: {} <-> {}", user->username,
                 target ? target->username : req.target_user_id());
}

void ContactHandler::onDeleteFriend(std::shared_ptr<ClientSession> session,
                                     const Packet& pkt) {
    novachat::DeleteFriendRequest req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()))) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }
    auto user = validateToken(session, req.token(), pkt.seq_id);
    if (!user) return;

    bool ok = storage_.removeFriend(user->user_id, req.target_user_id());
    if (!ok) {
        sendError(session, pkt.seq_id, novachat::FRIEND_NOT_FOUND, "Friend not found");
        return;
    }

    Packet resp;
    resp.type   = MessageType::DeleteFriendResponse;
    resp.seq_id = pkt.seq_id;
    // DeleteFriendResponse has no payload
    session->sendPacket(std::move(resp));

    spdlog::info("Friend removed: {} -x- {}", user->username, req.target_user_id());
}

void ContactHandler::onSearchUser(std::shared_ptr<ClientSession> session,
                                   const Packet& pkt) {
    novachat::SearchUserRequest req;
    if (!req.ParseFromArray(pkt.payload.data(), static_cast<int>(pkt.payload.size()))) {
        sendError(session, pkt.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }
    auto user = validateToken(session, req.token(), pkt.seq_id);
    if (!user) return;

    auto results = storage_.searchUsers(req.keyword());

    novachat::SearchUserResponse resp_pb;
    for (const auto& r : results) {
        if (r.user_id != user->user_id) {  // Exclude the requesting user
            *resp_pb.add_users() = toContactInfo(r);
        }
    }

    Packet resp;
    resp.type   = MessageType::SearchUserResponse;
    resp.seq_id = pkt.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(), static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));
}
