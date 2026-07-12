#pragma once
#include "network/ClientSession.h"
#include "storage/ServerStorage.h"
#include <memory>

class ContactHandler {
public:
    explicit ContactHandler(ServerStorage& storage);

    void handle(std::shared_ptr<ClientSession> session, const Packet& packet);

private:
    void onGetContacts  (std::shared_ptr<ClientSession> session, const Packet& pkt);
    void onAddFriend    (std::shared_ptr<ClientSession> session, const Packet& pkt);
    void onDeleteFriend (std::shared_ptr<ClientSession> session, const Packet& pkt);
    void onSearchUser   (std::shared_ptr<ClientSession> session, const Packet& pkt);

    // Validate the token; on failure sends an ErrorResponse and returns nullopt.
    std::optional<ServerStorage::UserRecord>
    validateToken(std::shared_ptr<ClientSession> session,
                  const std::string& token, uint32_t seq_id);

    void sendError(std::shared_ptr<ClientSession> session,
                   uint32_t seq_id, int code, const std::string& msg);

    ServerStorage& storage_;
};
