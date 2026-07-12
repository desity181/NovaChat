#include "handler/AuthHandler.h"
#include "proto/MessageType.h"
#include "proto/ProtocolCodec.h"
#include "auth.pb.h"
#include "common.pb.h"
#include <spdlog/spdlog.h>

AuthHandler::AuthHandler(ServerStorage& storage, const std::string& persist_file)
    : storage_(storage)
    , persist_file_(persist_file) {}

void AuthHandler::handle(std::shared_ptr<ClientSession> session,
                          const Packet& packet) {
    switch (packet.type) {
    case MessageType::RegisterRequest:
        onRegisterRequest(std::move(session), packet); break;
    case MessageType::LoginRequest:
        onLoginRequest(std::move(session), packet);    break;
    case MessageType::LogoutRequest:
        onLogoutRequest(std::move(session), packet);   break;
    default:
        break;
    }
}

void AuthHandler::sendError(std::shared_ptr<ClientSession> session,
                             uint32_t seq_id, int code,
                             const std::string& message) {
    novachat::ErrorResponse er;
    er.set_code(static_cast<novachat::ErrorCode>(code));
    er.set_message(message);

    Packet resp;
    resp.type   = MessageType::ErrorResponse;
    resp.seq_id = seq_id;
    resp.payload.resize(er.ByteSizeLong());
    er.SerializeToArray(resp.payload.data(),
                        static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));
}

void AuthHandler::onRegisterRequest(std::shared_ptr<ClientSession> session,
                                     const Packet& packet) {
    novachat::RegisterRequest req;
    if (!req.ParseFromArray(packet.payload.data(),
                            static_cast<int>(packet.payload.size()))) {
        sendError(session, packet.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }

    auto record = storage_.registerUser(req.username(), req.password(),
                                         req.display_name());
    if (!record) {
        sendError(session, packet.seq_id,
                  novachat::USER_ALREADY_EXISTS, "Username already exists");
        return;
    }

    novachat::RegisterResponse resp_pb;
    resp_pb.set_user_id(record->user_id);
    resp_pb.set_display_name(record->display_name);

    Packet resp;
    resp.type   = MessageType::RegisterResponse;
    resp.seq_id = packet.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(),
                             static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));

    spdlog::info("User registered: {} ({})", record->username, record->user_id);
    // Persist immediately so a server restart doesn't lose the registration
    storage_.saveToFile();
}

void AuthHandler::onLoginRequest(std::shared_ptr<ClientSession> session,
                                  const Packet& packet) {
    novachat::LoginRequest req;
    if (!req.ParseFromArray(packet.payload.data(),
                            static_cast<int>(packet.payload.size()))) {
        sendError(session, packet.seq_id, novachat::UNKNOWN_ERROR, "parse error");
        return;
    }

    auto record = storage_.authenticate(req.username(), req.password());
    if (!record) {
        sendError(session, packet.seq_id,
                  novachat::AUTH_FAILED, "Invalid username or password");
        return;
    }

    const std::string token = storage_.createSession(record->user_id);
    session->setUserId(record->user_id);
    storage_.registerSession(record->user_id, session);  // Register for message push

    novachat::LoginResponse resp_pb;
    resp_pb.set_token(token);
    resp_pb.set_user_id(record->user_id);
    resp_pb.set_display_name(record->display_name);

    Packet resp;
    resp.type   = MessageType::LoginResponse;
    resp.seq_id = packet.seq_id;
    resp.payload.resize(resp_pb.ByteSizeLong());
    resp_pb.SerializeToArray(resp.payload.data(),
                             static_cast<int>(resp.payload.size()));
    session->sendPacket(std::move(resp));

    spdlog::info("User logged in: {} ({})", record->username, record->user_id);
}

void AuthHandler::onLogoutRequest(std::shared_ptr<ClientSession> session,
                                   const Packet& packet) {
    novachat::LogoutRequest req;
    if (!req.ParseFromArray(packet.payload.data(),
                            static_cast<int>(packet.payload.size()))) {
        return;
    }
    storage_.removeSession(req.token());
    session->setUserId("");
    spdlog::info("User logged out, token removed");
}
