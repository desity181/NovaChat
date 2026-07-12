#pragma once
#include "network/ClientSession.h"
#include "storage/ServerStorage.h"
#include <memory>

class AuthHandler {
public:
    explicit AuthHandler(ServerStorage& storage, const std::string& persist_file = {});

    // Dispatch an auth-related packet from the given session.
    void handle(std::shared_ptr<ClientSession> session, const Packet& packet);

private:
    void onRegisterRequest(std::shared_ptr<ClientSession> session,
                           const Packet& packet);
    void onLoginRequest   (std::shared_ptr<ClientSession> session,
                           const Packet& packet);
    void onLogoutRequest  (std::shared_ptr<ClientSession> session,
                           const Packet& packet);

    void sendError(std::shared_ptr<ClientSession> session,
                   uint32_t seq_id, int code, const std::string& message);

    ServerStorage& storage_;
    std::string    persist_file_;
};
