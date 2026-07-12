#include "service/AuthService.h"
#include "logger/Logger.h"
#include "proto/MessageType.h"
#include "auth.pb.h"
#include "common.pb.h"

AuthService::AuthService(TcpClient*      client,
                          UserRepository& user_repo,
                          QObject*        parent)
    : QObject(parent)
    , client_(client)
    , user_repo_(user_repo)
{}

void AuthService::login(const std::string& username,
                         const std::string& password) {
    novachat::LoginRequest req;
    req.set_username(username);
    req.set_password(password);

    Packet pkt;
    pkt.type = MessageType::LoginRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(),
                         static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this, username](Packet resp) {
        if (resp.type == MessageType::LoginResponse) {
            novachat::LoginResponse lr;
            if (!lr.ParseFromArray(resp.payload.data(),
                                   static_cast<int>(resp.payload.size()))) {
                emit loginFailed("Failed to parse server response");
                return;
            }
            LocalAccount acc;
            acc.server_id    = lr.user_id();
            acc.username     = username;
            acc.display_name = lr.display_name();
            acc.avatar_url   = lr.avatar_url();
            acc.token        = lr.token();
            acc.is_active    = true;

            user_repo_.save(acc);
            user_repo_.setActive(lr.user_id());

            current_user_.server_id    = lr.user_id();
            current_user_.username     = username;
            current_user_.display_name = lr.display_name();
            current_user_.avatar_url   = lr.avatar_url();
            current_user_.token        = lr.token();

            Logger::info("Login succeeded: {} ({})", username, lr.user_id());
            emit loginSucceeded(current_user_);

        } else if (resp.type == MessageType::ErrorResponse) {
            novachat::ErrorResponse er;
            er.ParseFromArray(resp.payload.data(),
                              static_cast<int>(resp.payload.size()));
            Logger::warn("Login failed: {}", er.message());
            emit loginFailed(QString::fromStdString(er.message()));
        }
    });
}

void AuthService::registerUser(const std::string& username,
                                 const std::string& password,
                                 const std::string& display_name) {
    novachat::RegisterRequest req;
    req.set_username(username);
    req.set_password(password);
    req.set_display_name(display_name.empty() ? username : display_name);

    Packet pkt;
    pkt.type = MessageType::RegisterRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(),
                         static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt), [this](Packet resp) {
        if (resp.type == MessageType::RegisterResponse) {
            Logger::info("Register succeeded");
            emit registerSucceeded();
        } else if (resp.type == MessageType::ErrorResponse) {
            novachat::ErrorResponse er;
            er.ParseFromArray(resp.payload.data(),
                              static_cast<int>(resp.payload.size()));
            emit registerFailed(QString::fromStdString(er.message()));
        }
    });
}

void AuthService::logout() {
    if (current_user_.token.empty()) return;

    novachat::LogoutRequest req;
    req.set_token(current_user_.token);

    Packet pkt;
    pkt.type = MessageType::LogoutRequest;
    pkt.payload.resize(req.ByteSizeLong());
    req.SerializeToArray(pkt.payload.data(),
                         static_cast<int>(pkt.payload.size()));

    client_->sendPacket(std::move(pkt));  // fire-and-forget
    user_repo_.clearActive();
    current_user_ = {};

    Logger::info("Logged out");
    emit loggedOut();
}
