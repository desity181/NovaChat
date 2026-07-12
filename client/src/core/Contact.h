#pragma once
#include <string>
#include <QMetaType>

struct Contact {
    std::string user_id;
    std::string owner_id;
    std::string username;
    std::string display_name;
    std::string avatar_url;
};

Q_DECLARE_METATYPE(Contact)
