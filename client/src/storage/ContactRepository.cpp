#include "storage/ContactRepository.h"
#include <sqlite3.h>

ContactRepository::ContactRepository(DatabaseManager& db) : db_(db) {}

Contact ContactRepository::rowToContact(sqlite3_stmt* stmt) {
    Contact c;
    c.owner_id    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    c.user_id     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    c.username    = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    c.display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    c.avatar_url  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
    return c;
}

void ContactRepository::save(const Contact& c) {
    const char* sql = R"(
        INSERT INTO contacts (owner_id, user_id, username, display_name, avatar_url)
        VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(owner_id, user_id) DO UPDATE SET
            username     = excluded.username,
            display_name = excluded.display_name,
            avatar_url   = excluded.avatar_url
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, c.owner_id.c_str(),     -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, c.user_id.c_str(),      -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, c.username.c_str(),     -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, c.display_name.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, c.avatar_url.c_str(),   -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void ContactRepository::replaceAll(const std::string&         owner_id,
                                    const std::vector<Contact>& contacts) {
    db_.execute("BEGIN TRANSACTION");
    try {
        const std::string del =
            "DELETE FROM contacts WHERE owner_id = '" + owner_id + "'";
        db_.execute(del);
        for (const auto& c : contacts) save(c);
        db_.execute("COMMIT");
    } catch (...) {
        db_.execute("ROLLBACK");
        throw;
    }
}

void ContactRepository::remove(const std::string& owner_id,
                                const std::string& user_id) {
    const char* sql =
        "DELETE FROM contacts WHERE owner_id = ? AND user_id = ?";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, owner_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_id.c_str(),  -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

std::vector<Contact>
ContactRepository::findAll(const std::string& owner_id) {
    const char* sql = R"(
        SELECT owner_id, user_id, username, display_name, avatar_url
        FROM contacts WHERE owner_id = ?
        ORDER BY display_name COLLATE NOCASE
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, owner_id.c_str(), -1, SQLITE_STATIC);

    std::vector<Contact> result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        result.push_back(rowToContact(stmt));
    }
    sqlite3_finalize(stmt);
    return result;
}

std::optional<Contact>
ContactRepository::findById(const std::string& owner_id,
                             const std::string& user_id) {
    const char* sql = R"(
        SELECT owner_id, user_id, username, display_name, avatar_url
        FROM contacts WHERE owner_id = ? AND user_id = ? LIMIT 1
    )";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db_.handle(), sql, -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, owner_id.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user_id.c_str(),  -1, SQLITE_STATIC);

    std::optional<Contact> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) result = rowToContact(stmt);
    sqlite3_finalize(stmt);
    return result;
}
