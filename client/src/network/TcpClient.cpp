#include "network/TcpClient.h"
#include "config/AppConfig.h"
#include "core/Contact.h"
#include "core/Conversation.h"
#include "core/Message.h"
#include "logger/Logger.h"
#include <QMetaObject>
#include <algorithm>
#include <chrono>
#include <vector>

TcpClient::TcpClient(QObject* parent)
    : QObject(parent)
    , work_guard_(asio::make_work_guard(io_ctx_))
    , socket_(io_ctx_)
    , reconnect_timer_(io_ctx_)
    , read_buf_(READ_BUF_SIZE)
{
    // Register all custom types used in cross-thread Qt signals so that
    // Qt::QueuedConnection can copy them across the event-loop boundary.
    qRegisterMetaType<Packet>("Packet");
    qRegisterMetaType<Message>("Message");
    qRegisterMetaType<Contact>("Contact");
    qRegisterMetaType<Conversation>("Conversation");
    qRegisterMetaType<std::vector<Message>>("std::vector<Message>");
    qRegisterMetaType<std::vector<Contact>>("std::vector<Contact>");
    qRegisterMetaType<std::vector<Conversation>>("std::vector<Conversation>");
    qRegisterMetaType<std::string>("std::string");
    io_thread_ = std::thread([this] { io_ctx_.run(); });
}

TcpClient::~TcpClient() {
    manual_disconnect_ = true;
    asio::post(io_ctx_, [this] {
        asio::error_code ec;
        socket_.close(ec);
        reconnect_timer_.cancel();
        if (heartbeat_timer_) heartbeat_timer_->cancel();
    });
    work_guard_.reset();
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
}

uint32_t TcpClient::nextSeqId() {
    return ++seq_counter_;
}

// ── Public API ────────────────────────────────────────────────────────────

void TcpClient::connectToServer(const std::string& host, uint16_t port) {
    cached_host_       = host;
    cached_port_       = port;
    manual_disconnect_ = false;
    reconnect_attempt_ = 0;
    doConnect();
}

void TcpClient::disconnectFromServer() {
    manual_disconnect_ = true;
    asio::post(io_ctx_, [this] {
        asio::error_code ec;
        socket_.close(ec);
        reconnect_timer_.cancel();
        if (heartbeat_timer_) heartbeat_timer_->cancel();
    });
}

// ── Connection management ─────────────────────────────────────────────────

void TcpClient::doConnect() {
    asio::post(io_ctx_, [this] {
        // Close any existing socket (triggers pending async_read with operation_aborted)
        asio::error_code ec;
        socket_.close(ec);

        asio::ip::tcp::resolver resolver(io_ctx_);
        auto endpoints = resolver.resolve(cached_host_,
                                          std::to_string(cached_port_), ec);
        if (ec) {
            scheduleReconnect();
            return;
        }

        asio::async_connect(socket_, endpoints,
            [this](const asio::error_code& err,
                   const asio::ip::tcp::endpoint&) {
                if (err) {
                    scheduleReconnect();
                    return;
                }
                reconnect_attempt_ = 0;
                {
                    std::lock_guard<std::mutex> lock(callbacks_mutex_);
                    callbacks_.clear();
                }
                codec_ = ProtocolCodec{};  // Reset decode state
                Logger::info("Connected to server");
                emit connected();
                scheduleHeartbeat();
                doRead();
            });
    });
}

void TcpClient::scheduleReconnect() {
    if (manual_disconnect_) return;
    emit disconnected();

    if (++reconnect_attempt_ > kMaxReconnectAttempts) {
        emit connectionFailed();
        return;
    }

    // Exponential back-off: 2, 4, 8, 16, 32, 60, 60, 60 seconds
    const int delay_s = std::min(1 << reconnect_attempt_, kMaxReconnectDelaySec);
    emit reconnecting(reconnect_attempt_, delay_s);

    reconnect_timer_.expires_after(std::chrono::seconds(delay_s));
    reconnect_timer_.async_wait([this](const asio::error_code& ec) {
        if (!ec && !manual_disconnect_) doConnect();
    });
}

void TcpClient::onConnectionLost() {
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        callbacks_.clear();
    }
    if (heartbeat_timer_) heartbeat_timer_->cancel();
    scheduleReconnect();
}

// ── Send ──────────────────────────────────────────────────────────────────

void TcpClient::sendPacket(Packet packet) {
    auto data = std::make_shared<std::vector<uint8_t>>(
        ProtocolCodec::encode(packet));
    asio::post(io_ctx_, [this, data] {
        asio::async_write(socket_, asio::buffer(*data),
            [data](const asio::error_code& ec, size_t /*bytes*/) {
                if (ec) {
                    Logger::error("TcpClient send error: {}", ec.message());
                }
            });
    });
}

void TcpClient::sendPacket(Packet packet, ResponseCallback callback) {
    const uint32_t seq = nextSeqId();
    packet.seq_id = seq;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        callbacks_[seq] = std::move(callback);
    }
    sendPacket(std::move(packet));
}

// ── Read loop ─────────────────────────────────────────────────────────────

void TcpClient::doRead() {
    socket_.async_read_some(asio::buffer(read_buf_),
        [this](const asio::error_code& ec, size_t bytes) {
            if (ec) {
                if (ec != asio::error::operation_aborted) {
                    Logger::warn("TcpClient read error: {}", ec.message());
                    onConnectionLost();
                }
                return;
            }
            codec_.feed(read_buf_.data(), bytes,
                [this](Packet pkt) { handlePacket(std::move(pkt)); });
            doRead();
        });
}

// ── Heartbeat ─────────────────────────────────────────────────────────────

void TcpClient::scheduleHeartbeat() {
    const int interval = AppConfig::instance().server.heartbeat_interval_sec;

    heartbeat_timer_ = std::make_unique<asio::steady_timer>(io_ctx_);
    heartbeat_timer_->expires_after(std::chrono::seconds(interval));
    heartbeat_timer_->async_wait([this](const asio::error_code& ec) {
        if (ec) return;
        Packet hb;
        hb.type   = MessageType::HeartbeatRequest;
        hb.seq_id = nextSeqId();
        sendPacket(hb);
        Logger::debug("Heartbeat sent");
        scheduleHeartbeat();
    });
}

// ── Packet dispatch ───────────────────────────────────────────────────────

void TcpClient::handlePacket(Packet packet) {
    ResponseCallback cb;
    {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        auto it = callbacks_.find(packet.seq_id);
        if (it != callbacks_.end()) {
            cb = std::move(it->second);
            callbacks_.erase(it);
        }
    }

    if (cb) {
        Packet pkt_copy = packet;
        QMetaObject::invokeMethod(this,
            [cb = std::move(cb), pkt = std::move(pkt_copy)]() mutable {
                cb(std::move(pkt));
            }, Qt::QueuedConnection);
    } else {
        emit packetReceived(packet);
    }
}
