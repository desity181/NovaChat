#pragma once
#include "network/ClientSession.h"
#include "storage/ServerStorage.h"
#include <chrono>
#include <memory>
#include <optional>

class ChatHandler {
public:
    explicit ChatHandler(ServerStorage& storage);

    void handle(std::shared_ptr<ClientSession> session, const Packet& packet);

private:
    void onSendMessage      (std::shared_ptr<ClientSession> session, const Packet& pkt);
    void onGetHistory       (std::shared_ptr<ClientSession> session, const Packet& pkt);
    void onGetConversations (std::shared_ptr<ClientSession> session, const Packet& pkt);

    std::optional<ServerStorage::UserRecord>
    validateToken(std::shared_ptr<ClientSession> session,
                  const std::string& token, uint32_t seq_id);

    void sendError(std::shared_ptr<ClientSession> session,
                   uint32_t seq_id, int code, const std::string& msg);

    static std::string makeConvId(const std::string& a, const std::string& b);
    static int64_t nowMs();

    ServerStorage& storage_;
};
