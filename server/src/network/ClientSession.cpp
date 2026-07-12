#include "network/ClientSession.h"
#include <spdlog/spdlog.h>

ClientSession::ClientSession(asio::ip::tcp::socket socket,
                             PacketHandler         handler)
    : socket_(std::move(socket))
    , handler_(std::move(handler))
    , read_buf_(READ_BUF_SIZE)
{}

void ClientSession::start() {
    doRead();
}

void ClientSession::sendPacket(Packet packet) {
    auto data = std::make_shared<std::vector<uint8_t>>(
        ProtocolCodec::encode(packet));

    auto self = shared_from_this();
    asio::async_write(socket_, asio::buffer(*data),
        [self, data](const asio::error_code& ec, size_t /*bytes*/) {
            if (ec) {
                spdlog::warn("ClientSession send error: {}", ec.message());
            }
        });
}

void ClientSession::doRead() {
    auto self = shared_from_this();
    socket_.async_read_some(asio::buffer(read_buf_),
        [self](const asio::error_code& ec, size_t bytes) {
            if (ec) {
                spdlog::info("Client disconnected: {}", ec.message());
                return;  // Session ends naturally as shared_ptr ref-count reaches zero
            }
            self->codec_.feed(self->read_buf_.data(), bytes,
                [self](Packet pkt) {
                    self->handler_(self, std::move(pkt));
                });
            self->doRead();
        });
}
