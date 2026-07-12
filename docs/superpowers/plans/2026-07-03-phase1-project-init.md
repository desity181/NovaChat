# NovaChat V1 — 阶段一实现计划：工程初始化

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 搭建可编译的 CMake 工程骨架，Logger 模块通过单元测试，客户端打开空白 Qt 窗口，服务端启动后打印日志并等待退出信号。

**Architecture:** CMake 多子项目结构（client / server），通过 vcpkg 管理第三方依赖（Qt6 除外）。客户端分为 `NovaChat_Core` 静态库（纯 C++，可单元测试）和 `NovaChat_UI` 静态库（Qt Widgets），主可执行文件链接两者。测试用 Catch2 v3 + CTest。

**Tech Stack:** C++20 · Qt6 Widgets（系统安装）· CMake 3.25+ · vcpkg · spdlog 1.x · Catch2 3.x

---

## 环境前置条件

- **vcpkg**：已安装并设置环境变量 `VCPKG_ROOT`（如 `C:/vcpkg`）
- **Qt6**：通过 Qt 官方安装器安装，设置环境变量 `QT6_DIR`（如 `C:/Qt/6.8.0/msvc2022_64`）
- **Ninja**：已安装（`winget install Ninja-build.Ninja`）
- **MSVC**：Visual Studio 2022 Build Tools（带 C++ 工具链）

---

## 文件结构（本阶段新增）

```
NovaChat/
├── CMakeLists.txt          ← 根构建文件
├── vcpkg.json              ← 依赖声明
├── CMakePresets.json       ← 构建预设
├── .gitignore
├── client/
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp
│   │   ├── logger/
│   │   │   ├── Logger.h
│   │   │   └── Logger.cpp
│   │   └── ui/
│   │       ├── MainWindow.h
│   │       └── MainWindow.cpp
│   └── tests/
│       ├── CMakeLists.txt
│       └── logger/
│           └── test_logger.cpp
└── server/
    ├── CMakeLists.txt
    └── src/
        └── main.cpp
```

---

## Task 1：创建 CMake 构建系统

**Files:**
- Create: `CMakeLists.txt`
- Create: `vcpkg.json`
- Create: `CMakePresets.json`
- Create: `.gitignore`

- [ ] **Step 1：写 vcpkg.json**

```json
{
    "name": "novachat",
    "version-string": "0.1.0",
    "dependencies": [
        "spdlog",
        "catch2",
        "asio",
        "protobuf",
        "sqlite3",
        "yaml-cpp",
        "nlohmann-json"
    ]
}
```

- [ ] **Step 2：写根 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.25)
project(NovaChat VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 自动化工具（MOC / UIC / RCC）
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(spdlog CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

enable_testing()

add_subdirectory(client)
add_subdirectory(server)
```

- [ ] **Step 3：写 CMakePresets.json**

```json
{
    "version": 6,
    "configurePresets": [
        {
            "name": "windows-debug",
            "displayName": "Windows Debug (MSVC + Ninja)",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/debug",
            "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "CMAKE_PREFIX_PATH": "$env{QT6_DIR}"
            }
        },
        {
            "name": "windows-release",
            "displayName": "Windows Release (MSVC + Ninja)",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/release",
            "toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Release",
                "CMAKE_PREFIX_PATH": "$env{QT6_DIR}"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "windows-debug",
            "configurePreset": "windows-debug"
        },
        {
            "name": "windows-release",
            "configurePreset": "windows-release"
        }
    ],
    "testPresets": [
        {
            "name": "windows-debug",
            "configurePreset": "windows-debug",
            "output": { "outputOnFailure": true }
        }
    ]
}
```

- [ ] **Step 4：写 .gitignore**

```
build/
*.log
*.db
.vs/
.vscode/
CMakeUserPresets.json
```

- [ ] **Step 5：写 client/CMakeLists.txt**

```cmake
# ── Core 静态库（非 UI，可单元测试）────────────────────────────
add_library(NovaChat_Core STATIC
    src/logger/Logger.cpp
)
target_include_directories(NovaChat_Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(NovaChat_Core PUBLIC
    Qt6::Core
    spdlog::spdlog
)

# ── UI 静态库（Qt Widgets）──────────────────────────────────────
add_library(NovaChat_UI STATIC
    src/ui/MainWindow.cpp
)
target_link_libraries(NovaChat_UI PUBLIC
    Qt6::Widgets
    NovaChat_Core
)

# ── 可执行文件 ──────────────────────────────────────────────────
add_executable(NovaChat WIN32 src/main.cpp)
target_link_libraries(NovaChat PRIVATE NovaChat_Core NovaChat_UI)

# ── 单元测试 ────────────────────────────────────────────────────
add_subdirectory(tests)
```

- [ ] **Step 6：写 client/tests/CMakeLists.txt**

```cmake
include(CTest)
include(Catch)

add_executable(NovaChat_Tests
    logger/test_logger.cpp
)
target_include_directories(NovaChat_Tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(NovaChat_Tests PRIVATE
    NovaChat_Core
    Catch2::Catch2WithMain
)
catch_discover_tests(NovaChat_Tests)
```

- [ ] **Step 7：写 server/CMakeLists.txt**

```cmake
find_package(spdlog CONFIG REQUIRED)

add_executable(NovaChatServer
    src/main.cpp
)
target_link_libraries(NovaChatServer PRIVATE
    spdlog::spdlog
)
```

- [ ] **Step 8：验证 CMake 配置通过**

```
cmake --preset windows-debug
```

预期输出（无报错）：
```
-- The CXX compiler identification is MSVC ...
-- Found Qt6 ...
-- Found spdlog ...
-- Found Catch2 ...
-- Configuring done
-- Build files have been written to: .../build/debug
```

- [ ] **Step 9：提交**

```
git init
git add .
git commit -m "build: 初始化 CMake 工程骨架"
```

---

## Task 2：创建 Logger 模块并通过单元测试

**Files:**
- Create: `client/src/logger/Logger.h`
- Create: `client/src/logger/Logger.cpp`
- Create: `client/tests/logger/test_logger.cpp`

- [ ] **Step 1：写 client/src/logger/Logger.h**

```cpp
#pragma once
#include <spdlog/spdlog.h>
#include <string>

// 全局日志门面（静态方法，底层使用 spdlog 默认 logger）
// 同时输出到控制台（彩色）和滚动文件（最大 5MB × 3 个）
class Logger {
public:
    static void init(const std::string& log_file,
                     spdlog::level::level_enum level = spdlog::level::debug);
    static void shutdown();

    template<typename... Args>
    static void trace(fmt::format_string<Args...> fmt, Args&&... args) {
        spdlog::trace(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void debug(fmt::format_string<Args...> fmt, Args&&... args) {
        spdlog::debug(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void info(fmt::format_string<Args...> fmt, Args&&... args) {
        spdlog::info(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void warn(fmt::format_string<Args...> fmt, Args&&... args) {
        spdlog::warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void error(fmt::format_string<Args...> fmt, Args&&... args) {
        spdlog::error(fmt, std::forward<Args>(args)...);
    }

    Logger() = delete;
};
```

- [ ] **Step 2：写 client/src/logger/Logger.cpp**

```cpp
#include "logger/Logger.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

void Logger::init(const std::string& log_file, spdlog::level::level_enum level) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink    = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        log_file, 5 * 1024 * 1024 /*5MB*/, 3 /*keep 3 files*/);

    auto logger = std::make_shared<spdlog::logger>(
        "novachat",
        spdlog::sinks_init_list{console_sink, file_sink});
    logger->set_level(level);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    spdlog::set_default_logger(logger);
}

void Logger::shutdown() {
    spdlog::shutdown();
}
```

- [ ] **Step 3：写 client/tests/logger/test_logger.cpp**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "logger/Logger.h"

TEST_CASE("Logger::init 初始化不抛异常", "[logger]") {
    REQUIRE_NOTHROW(Logger::init("test_logger_1.log", spdlog::level::debug));
    Logger::shutdown();
}

TEST_CASE("Logger 各级别输出均不崩溃", "[logger]") {
    Logger::init("test_logger_2.log", spdlog::level::trace);

    REQUIRE_NOTHROW(Logger::trace("trace: {}", 1));
    REQUIRE_NOTHROW(Logger::debug("debug: {}", 2));
    REQUIRE_NOTHROW(Logger::info("info: {}", 3));
    REQUIRE_NOTHROW(Logger::warn("warn: {}", 4));
    REQUIRE_NOTHROW(Logger::error("error: {}", 5));

    Logger::shutdown();
}
```

- [ ] **Step 4：运行测试，确认 PASS**

```
cmake --build --preset windows-debug
ctest --preset windows-debug -V
```

预期输出：
```
[==========] Running 2 tests from 1 test suite.
[ RUN      ] Logger::init 初始化不抛异常
[       OK ] Logger::init 初始化不抛异常
[ RUN      ] Logger 各级别输出均不崩溃
[       OK ] Logger 各级别输出均不崩溃
[==========] 2 tests from 1 test suite ran.
[  PASSED  ] 2 tests.
```

- [ ] **Step 5：提交**

```
git add client/src/logger/ client/tests/
git commit -m "feat(logger): 添加 Logger 模块，通过 Catch2 单元测试"
```

---

## Task 3：创建空白客户端主窗口

**Files:**

- Create: `client/src/ui/MainWindow.h`
- Create: `client/src/ui/MainWindow.cpp`
- Create: `client/src/main.cpp`

- [ ] **Step 1：写 client/src/ui/MainWindow.h**

```cpp
#pragma once
#include <QMainWindow>

// 应用主窗口 —— 阶段一仅为占位，后续各阶段替换为真实布局
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;
};
```

- [ ] **Step 2：写 client/src/ui/MainWindow.cpp**

```cpp
#include "ui/MainWindow.h"
#include <QLabel>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("NovaChat");
    setMinimumSize(900, 640);

    // 阶段一占位内容，后续替换为真实布局
    auto* placeholder = new QLabel("NovaChat — 初始化中...", this);
    placeholder->setAlignment(Qt::AlignCenter);
    setCentralWidget(placeholder);
}
```

- [ ] **Step 3：写 client/src/main.cpp**

```cpp
#include <QApplication>
#include "logger/Logger.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    // 在创建 QApplication 之前初始化日志（不依赖 Qt 事件循环）
    Logger::init("novachat.log");
    Logger::info("NovaChat v0.1.0 启动");

    QApplication app(argc, argv);
    app.setApplicationName("NovaChat");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("NovaChat");

    MainWindow window;
    window.show();

    Logger::info("主窗口已显示");
    const int result = app.exec();

    Logger::info("NovaChat 退出，返回值 {}", result);
    Logger::shutdown();
    return result;
}
```

- [ ] **Step 4：编译并运行客户端**

```
cmake --build --preset windows-debug
./build/debug/client/NovaChat.exe
```

预期：弹出 900×640 的窗口，标题栏显示 "NovaChat"，中间显示 "NovaChat — 初始化中..."。
控制台输出：
```
[...] [info] NovaChat v0.1.0 启动
[...] [info] 主窗口已显示
```

- [ ] **Step 5：提交**

```
git add client/src/ui/ client/src/main.cpp
git commit -m "feat(ui): 添加空白主窗口，客户端可正常启动"
```

---

## Task 4：创建空白服务端

**Files:**
- Create: `server/src/main.cpp`

- [ ] **Step 1：写 server/src/main.cpp**

```cpp
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

namespace {
    std::atomic<bool> g_running{true};
}

static void signal_handler(int /*sig*/) {
    g_running = false;
}

int main() {
    // 初始化服务端日志
    auto logger = spdlog::stdout_color_mt("server");
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    spdlog::info("NovaChatServer v0.1.0 启动，监听端口 9527（阶段一占位）");
    spdlog::info("按 Ctrl+C 停止");

    // 阶段一：占位主循环，后续替换为真实 Asio accept 循环
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    spdlog::info("服务端停止");
    spdlog::shutdown();
    return 0;
}
```

- [ ] **Step 2：编译并运行服务端**

```
cmake --build --preset windows-debug
./build/debug/server/NovaChatServer.exe
```

预期输出：
```
[...] [info] NovaChatServer v0.1.0 启动，监听端口 9527（阶段一占位）
[...] [info] 按 Ctrl+C 停止
```
按 Ctrl+C 后输出：
```
[...] [info] 服务端停止
```

- [ ] **Step 3：最终全量构建 + 测试验证**

```
cmake --build --preset windows-debug
ctest --preset windows-debug -V
```

预期：编译 0 错误，2 个测试全部 PASS。

- [ ] **Step 4：提交**

```
git add server/src/main.cpp
git commit -m "feat(server): 添加服务端占位主循环，可正常启动和退出"
```

---

## 阶段一完成标准

- [ ] `cmake --preset windows-debug` 配置无报错
- [ ] `cmake --build --preset windows-debug` 编译无错误无警告
- [ ] `ctest --preset windows-debug` 2 个测试全部 PASS
- [ ] 运行 `NovaChat.exe` → 空白窗口正常显示
- [ ] 运行 `NovaChatServer.exe` → 启动日志正常，Ctrl+C 后正常退出
- [ ] 所有代码已提交到 git
