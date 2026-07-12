#pragma once
#include <string>
#include <cstdint>

struct LocalAccount {
    int64_t     id{0};
    std::string server_id;
    std::string username;
    std::string display_name;
    std::string avatar_url;
    std::string token;
    bool        is_active{false};
};
