#include "storage/UserRepository.h"
#include <sqlite3.h>

UserRepository::UserRepository(DatabaseManager& db) : db_(db) {}

void UserRepository::save(const LocalAccount& acc) {
    const char* sql = R"(
        INSERT INTO local_accounts
            (server_id, username, display_name, avatar_url, token, is_active)
        VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(server_id) DO UPDATE SET
            username     = excluded.username,
            display_name = excluded.display_name,
            avatar_url   = excluded.avatar_url,
            token        = excluded.token
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, acc.server_id.c_str(),    -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, acc.username.c_str(),     -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, acc.display_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, acc.avatar_url.c_str(),   -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, acc.token.c_str(),        -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 6, acc.is_active ? 1 : 0);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void UserRepository::updateToken(const std::string& server_id,
                                  const std::string& token) {
    const char* sql =
        "UPDATE local_accounts SET token = ? WHERE server_id = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, token.c_str(),     -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, server_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void UserRepository::setActive(const std::string& server_id) {
    db_.execute("UPDATE local_accounts SET is_active = 0");
    const char* sql =
        "UPDATE local_accounts SET is_active = 1 WHERE server_id = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, server_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void UserRepository::clearActive() {
    db_.execute("UPDATE local_accounts SET is_active = 0");
}

std::optional<LocalAccount>
UserRepository::rowToAccount(sqlite3_stmt* stmt) {
    LocalAccount acc;
    acc.id           = sqlite3_column_int64(stmt, 0);
    acc.server_id    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    acc.username     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    acc.display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    acc.avatar_url   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    acc.token        = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
    acc.is_active    = sqlite3_column_int(stmt, 6) != 0;
    return acc;
}

std::optional<LocalAccount> UserRepository::findActive() {
    const char* sql = R"(
        SELECT id, server_id, username, display_name, avatar_url, token, is_active
        FROM local_accounts WHERE is_active = 1 LIMIT 1
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    std::optional<LocalAccount> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = rowToAccount(stmt);
    sqlite3_finalize(stmt);
    return result;
}

std::optional<LocalAccount>
UserRepository::findById(const std::string& server_id) {
    const char* sql = R"(
        SELECT id, server_id, username, display_name, avatar_url, token, is_active
        FROM local_accounts WHERE server_id = ? LIMIT 1
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, server_id.c_str(), -1, SQLITE_STATIC);
    std::optional<LocalAccount> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = rowToAccount(stmt);
    sqlite3_finalize(stmt);
    return result;
}
