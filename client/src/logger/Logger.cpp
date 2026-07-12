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
