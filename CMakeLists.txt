cmake_minimum_required(VERSION 3.25)
project(NovaChat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Windows: Asio requires at minimum Windows 10 API
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00)
endif()

find_package(Qt6          REQUIRED COMPONENTS Core Widgets Network)
find_package(spdlog       CONFIG REQUIRED)
find_package(Catch2 3     CONFIG REQUIRED)
find_package(protobuf     CONFIG REQUIRED)
find_package(SQLite3      REQUIRED)
find_package(yaml-cpp     CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(asio         CONFIG REQUIRED)

enable_testing()

add_subdirectory(proto)    # Must precede client/server — produces NovaChat_Proto
add_subdirectory(client)
add_subdirectory(server)
