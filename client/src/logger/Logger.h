#pragma once
#include <spdlog/spdlog.h>
#include <string>

// 全局日志门面，底层使用 spdlog 默认 logger
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
