#pragma once
#include "network/ClientSession.h"
#include <asio.hpp>
#include <cstdint>

class TcpServer {
public:
    TcpServer(asio::io_context&            io_ctx,
              uint16_t                     port,
              ClientSession::PacketHandler handler);

    void start();

private:
    void doAccept();

    asio::io_context&            io_ctx_;
    asio::ip::tcp::acceptor      acceptor_;
    ClientSession::PacketHandler handler_;
};
