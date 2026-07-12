#include <catch2/catch_test_macros.hpp>
#include "config/AppConfig.h"
#include <fstream>
#include <cstdio>

TEST_CASE("AppConfig default values are correct", "[config]") {
    AppConfig::instance().server = ServerConfig{};
    AppConfig::instance().log    = LogConfig{};

    CHECK(AppConfig::instance().server.host == "127.0.0.1");
    CHECK(AppConfig::instance().server.port == 9527);
    CHECK(AppConfig::instance().server.heartbeat_interval_sec == 30);
}

TEST_CASE("AppConfig loads YAML and overrides defaults", "[config]") {
    const char* tmp = "test_cfg_load.yaml";
    {
        std::ofstream f(tmp);
        f << "server:\n"
          << "  host: 192.168.1.100\n"
          << "  port: 8080\n";
    }

    AppConfig::instance().load(tmp);
    CHECK(AppConfig::instance().server.host == "192.168.1.100");
    CHECK(AppConfig::instance().server.port == 8080);

    std::remove(tmp);

    AppConfig::instance().server.host = "127.0.0.1";
    AppConfig::instance().server.port = 9527;
}

TEST_CASE("AppConfig missing file uses defaults without throwing", "[config]") {
    REQUIRE_NOTHROW(AppConfig::instance().load("nonexistent_config.yaml"));
    CHECK(AppConfig::instance().server.host == "127.0.0.1");
}

TEST_CASE("AppConfig save/load round-trip is consistent", "[config]") {
    const char* tmp = "test_cfg_roundtrip.yaml";
    AppConfig::instance().server.port = 12345;
    AppConfig::instance().save(tmp);

    AppConfig::instance().server.port = 9527;
    AppConfig::instance().load(tmp);
    CHECK(AppConfig::instance().server.port == 12345);

    std::remove(tmp);
    AppConfig::instance().server.port = 9527;
}
