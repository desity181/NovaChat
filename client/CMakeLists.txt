# ── Core static library (non-UI, unit-testable) ────────────────
add_library(NovaChat_Core STATIC
    src/logger/Logger.cpp
    src/config/AppConfig.cpp
    src/storage/DatabaseManager.cpp
    src/storage/UserRepository.cpp
    src/storage/ContactRepository.cpp
    src/storage/MessageRepository.cpp
    src/storage/ConversationRepository.cpp
    src/service/AuthService.cpp
    src/service/ContactService.cpp
    src/service/ChatService.cpp
    src/network/TcpClient.cpp
)
target_include_directories(NovaChat_Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(NovaChat_Core PUBLIC
    Qt6::Core
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    SQLite::SQLite3
    asio::asio
    NovaChat_Proto
)

# ── UI static library (Qt Widgets) ─────────────────────────────
add_library(NovaChat_UI STATIC
    src/ui/MainWindow.cpp
    src/ui/LoginWindow.cpp
    src/ui/RegisterDialog.cpp
    src/ui/ContactPanel.cpp
    src/ui/AddFriendDialog.cpp
    src/ui/ConversationList.cpp
    src/ui/MessageBubbleDelegate.cpp
    src/ui/ChatView.cpp
    src/ui/SettingsDialog.cpp
)
target_link_libraries(NovaChat_UI PUBLIC
    Qt6::Widgets
    NovaChat_Core
)

# ── Executable ──────────────────────────────────────────────────
add_executable(NovaChat WIN32
    src/main.cpp
    resources/resources.qrc
)
target_link_libraries(NovaChat PRIVATE NovaChat_Core NovaChat_UI Qt6::Network)

# ── Unit tests ──────────────────────────────────────────────────
add_subdirectory(tests)
