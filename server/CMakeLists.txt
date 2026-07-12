find_package(spdlog        CONFIG REQUIRED)
find_package(asio          CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(NovaChatServer
    src/main.cpp
    src/network/TcpServer.cpp
    src/network/ClientSession.cpp
    src/storage/ServerStorage.cpp
    src/handler/AuthHandler.cpp
    src/handler/ContactHandler.cpp
    src/handler/ChatHandler.cpp
)

target_include_directories(NovaChatServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(NovaChatServer PRIVATE
    spdlog::spdlog
    asio::asio
    nlohmann_json::nlohmann_json
    NovaChat_Proto
)
