#include "config/AppConfig.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

AppConfig& AppConfig::instance() {
    static AppConfig instance;
    return instance;
}

void AppConfig::load(const std::string& config_file) {
    try {
        YAML::Node cfg = YAML::LoadFile(config_file);

        if (auto s = cfg["server"]) {
            server.host                   = s["host"].as<std::string>(server.host);
            server.port                   = s["port"].as<uint16_t>(server.port);
            server.heartbeat_interval_sec = s["heartbeat_interval_sec"].as<int>(server.heartbeat_interval_sec);
            server.reconnect_interval_sec = s["reconnect_interval_sec"].as<int>(server.reconnect_interval_sec);
        }

        if (auto l = cfg["log"]) {
            log.file  = l["file"].as<std::string>(log.file);
            log.level = l["level"].as<std::string>(log.level);
        }
    } catch (const YAML::Exception&) {
        // Use defaults when file is missing or malformed — do not abort startup
    }
}

void AppConfig::save(const std::string& config_file) const {
    YAML::Node cfg;
    cfg["server"]["host"]                   = server.host;
    cfg["server"]["port"]                   = server.port;
    cfg["server"]["heartbeat_interval_sec"] = server.heartbeat_interval_sec;
    cfg["server"]["reconnect_interval_sec"] = server.reconnect_interval_sec;
    cfg["log"]["file"]                      = log.file;
    cfg["log"]["level"]                     = log.level;

    std::ofstream out(config_file);
    out << cfg;
}
