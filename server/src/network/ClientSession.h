#pragma once
#include "proto/ProtocolCodec.h"
#include <asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    using PacketHandler =
        std::function<void(std::shared_ptr<ClientSession>, Packet)>;

    ClientSession(asio::ip::tcp::socket socket, PacketHandler handler);

    void start();
    void sendPacket(Packet packet);

    const std::string& userId() const { return user_id_; }
    void setUserId(const std::string& id) { user_id_ = id; }

private:
    void doRead();

    asio::ip::tcp::socket socket_;
    PacketHandler         handler_;
    ProtocolCodec         codec_;
    std::vector<uint8_t>  read_buf_;
    std::string           user_id_;

    static constexpr size_t READ_BUF_SIZE = 65536;
};
