#include "network/TcpServer.h"
#include <spdlog/spdlog.h>

TcpServer::TcpServer(asio::io_context&            io_ctx,
                     uint16_t                     port,
                     ClientSession::PacketHandler handler)
    : io_ctx_(io_ctx)
    , acceptor_(io_ctx,
                asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    , handler_(std::move(handler))
{}

void TcpServer::start() {
    spdlog::info("TcpServer listening on port {}",
                 acceptor_.local_endpoint().port());
    doAccept();
}

void TcpServer::doAccept() {
    acceptor_.async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                spdlog::info("New connection from {}",
                             socket.remote_endpoint().address().to_string());
                auto session = std::make_shared<ClientSession>(
                    std::move(socket), handler_);
                session->start();
            } else {
                spdlog::error("Accept error: {}", ec.message());
            }
            doAccept();  // Continue accepting connections
        });
}
