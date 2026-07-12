#pragma once
#include <string>
#include <cstdint>

struct ServerConfig {
    std::string host{"127.0.0.1"};
    uint16_t    port{9527};
    int         heartbeat_interval_sec{30};
    int         reconnect_interval_sec{5};
};

struct LogConfig {
    std::string file{"novachat.log"};
    std::string level{"debug"};
};

// Singleton config object loaded from config.yaml; call load() once at startup
class AppConfig {
public:
    static AppConfig& instance();

    void load(const std::string& config_file);
    void save(const std::string& config_file) const;

    ServerConfig server;
    LogConfig    log;

private:
    AppConfig() = default;
};
