#pragma once
#include "proto/ProtocolCodec.h"
#include <QObject>
#include <asio.hpp>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

// Asynchronous TCP client.
// · Asio io_context runs on a dedicated thread.
// · Incoming packets are delivered to the main thread via Qt QueuedConnection signals.
// · Public methods must be called from the main (Qt) thread only.
// · Automatically reconnects with exponential back-off after a connection loss.
class TcpClient : public QObject {
    Q_OBJECT

public:
    using ResponseCallback = std::function<void(Packet)>;

    explicit TcpClient(QObject* parent = nullptr);
    ~TcpClient() override;

    // Begin connecting; caches host/port for automatic reconnection.
    void connectToServer(const std::string& host, uint16_t port);

    // Disconnect and suppress automatic reconnection.
    void disconnectFromServer();

    // Legacy alias kept for compatibility (same as disconnectFromServer).
    void disconnect() { disconnectFromServer(); }

    // Fire-and-forget send.
    void sendPacket(Packet packet);

    // Send with a one-shot response callback (executed on the main thread).
    void sendPacket(Packet packet, ResponseCallback callback);

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString& message);
    // Server-initiated pushes with no registered callback are delivered here.
    void packetReceived(Packet packet);
    // Waiting to reconnect; attempt counts from 1, delay_seconds is the wait time.
    void reconnecting(int attempt, int delay_seconds);
    // Reconnection gave up after kMaxReconnectAttempts failed attempts.
    void connectionFailed();

private:
    void doConnect();
    void scheduleReconnect();
    void onConnectionLost();
    void doRead();
    void scheduleHeartbeat();
    void handlePacket(Packet packet);
    uint32_t nextSeqId();

    asio::io_context                                            io_ctx_;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard_;
    asio::ip::tcp::socket                                       socket_;
    asio::steady_timer                                          reconnect_timer_;
    std::unique_ptr<asio::steady_timer>                         heartbeat_timer_;
    std::thread                                                 io_thread_;

    ProtocolCodec           codec_;
    std::vector<uint8_t>    read_buf_;
    static constexpr size_t READ_BUF_SIZE = 65536;

    std::atomic<uint32_t>                          seq_counter_{0};
    std::mutex                                     callbacks_mutex_;
    std::unordered_map<uint32_t, ResponseCallback> callbacks_;

    std::string cached_host_;
    uint16_t    cached_port_{0};
    int         reconnect_attempt_{0};
    bool        manual_disconnect_{false};

    static constexpr int kMaxReconnectAttempts = 8;
    static constexpr int kMaxReconnectDelaySec = 60;
};
