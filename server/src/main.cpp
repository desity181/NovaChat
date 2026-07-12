#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "network/TcpServer.h"
#include "handler/AuthHandler.h"
#include "handler/ContactHandler.h"
#include "handler/ChatHandler.h"
#include "storage/ServerStorage.h"
#include "proto/MessageType.h"
#include <asio.hpp>
#include <atomic>
#include <csignal>

static constexpr const char* PERSIST_FILE = "novachat_users.json";

namespace {
    std::atomic<bool>  g_running{true};
    asio::io_context*  g_io_ctx_ptr{nullptr};
    ServerStorage*     g_storage_ptr{nullptr};
}

static void signal_handler(int) {
    g_running = false;
    if (g_io_ctx_ptr)  g_io_ctx_ptr->stop();
}

int main() {
    auto logger = spdlog::stdout_color_mt("server");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    asio::io_context io_ctx;
    g_io_ctx_ptr = &io_ctx;

    ServerStorage  storage;
    g_storage_ptr = &storage;

    // Load persisted user registrations from previous runs
    storage.loadFromFile(PERSIST_FILE);
    spdlog::info("User store loaded from '{}'", PERSIST_FILE);

    AuthHandler    auth_handler(storage, PERSIST_FILE);
    ContactHandler contact_handler(storage);
    ChatHandler    chat_handler(storage);

    auto handler = [&auth_handler, &contact_handler, &chat_handler](
                       std::shared_ptr<ClientSession> session, Packet pkt) {
        switch (pkt.type) {
        case MessageType::HeartbeatRequest: {
            Packet resp;
            resp.type   = MessageType::HeartbeatResponse;
            resp.seq_id = pkt.seq_id;
            session->sendPacket(std::move(resp));
            break;
        }
        case MessageType::RegisterRequest:
        case MessageType::LoginRequest:
        case MessageType::LogoutRequest:
            auth_handler.handle(std::move(session), pkt);
            break;
        case MessageType::GetContactsRequest:
        case MessageType::AddFriendRequest:
        case MessageType::DeleteFriendRequest:
        case MessageType::SearchUserRequest:
            contact_handler.handle(std::move(session), pkt);
            break;
        case MessageType::SendMessageRequest:
        case MessageType::GetHistoryRequest:
        case MessageType::GetConversationsRequest:
            chat_handler.handle(std::move(session), pkt);
            break;
        default:
            spdlog::warn("Unknown packet 0x{:04X}",
                         static_cast<uint16_t>(pkt.type));
        }
    };

    TcpServer server(io_ctx, 9527, handler);
    server.start();
    spdlog::info("NovaChatServer v0.1.0 ready (port 9527)");

    io_ctx.run();

    // Persist user data on clean shutdown (Ctrl+C)
    storage.saveToFile();
    spdlog::info("User store saved to '{}'", PERSIST_FILE);
    spdlog::info("Server stopped");
    spdlog::shutdown();
    return 0;
}
