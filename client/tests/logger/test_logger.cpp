#include <catch2/catch_test_macros.hpp>
#include "logger/Logger.h"

TEST_CASE("Logger::init does not throw", "[logger]") {
    REQUIRE_NOTHROW(Logger::init("test_logger_1.log", spdlog::level::debug));
    Logger::shutdown();
}

TEST_CASE("Logger all levels do not crash", "[logger]") {
    Logger::init("test_logger_2.log", spdlog::level::trace);

    REQUIRE_NOTHROW(Logger::trace("trace: {}", 1));
    REQUIRE_NOTHROW(Logger::debug("debug: {}", 2));
    REQUIRE_NOTHROW(Logger::info("info: {}", 3));
    REQUIRE_NOTHROW(Logger::warn("warn: {}", 4));
    REQUIRE_NOTHROW(Logger::error("error: {}", 5));

    Logger::shutdown();
}
