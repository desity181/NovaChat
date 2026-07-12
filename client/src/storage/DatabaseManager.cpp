#include "storage/DatabaseManager.h"

DatabaseManager::DatabaseManager(const std::string& db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string err = db_ ? sqlite3_errmsg(db_) : "sqlite3_open failed";
        sqlite3_close(db_);
        db_ = nullptr;
        throw DatabaseException(err);
    }
    // WAL mode improves concurrent read performance for file databases;
    // :memory: silently falls back to its own in-memory journal mode.
    execute("PRAGMA journal_mode=WAL");
    execute("PRAGMA foreign_keys=ON");
    createSchema();
}

DatabaseManager::~DatabaseManager() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void DatabaseManager::execute(const std::string& sql) {
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string err = err_msg ? err_msg : "unknown error";
        sqlite3_free(err_msg);
        throw DatabaseException(err);
    }
}

void DatabaseManager::createSchema() {
    execute(R"(
        CREATE TABLE IF NOT EXISTS local_accounts (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            server_id    TEXT    NOT NULL UNIQUE,
            username     TEXT    NOT NULL,
            display_name TEXT    NOT NULL,
            avatar_url   TEXT    NOT NULL DEFAULT '',
            token        TEXT    NOT NULL,
            is_active    INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS contacts (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            owner_id     TEXT    NOT NULL,
            user_id      TEXT    NOT NULL,
            username     TEXT    NOT NULL,
            display_name TEXT    NOT NULL,
            avatar_url   TEXT    NOT NULL DEFAULT '',
            UNIQUE(owner_id, user_id)
        );

        CREATE TABLE IF NOT EXISTS conversations (
            id                   INTEGER PRIMARY KEY AUTOINCREMENT,
            owner_id             TEXT    NOT NULL,
            conversation_id      TEXT    NOT NULL,
            target_id            TEXT    NOT NULL,
            last_message_preview TEXT    NOT NULL DEFAULT '',
            last_message_time    INTEGER NOT NULL DEFAULT 0,
            unread_count         INTEGER NOT NULL DEFAULT 0,
            UNIQUE(owner_id, conversation_id)
        );

        CREATE TABLE IF NOT EXISTS messages (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            message_id      TEXT    NOT NULL UNIQUE,
            conversation_id TEXT    NOT NULL,
            sender_id       TEXT    NOT NULL,
            content         TEXT    NOT NULL,
            msg_type        INTEGER NOT NULL DEFAULT 1,
            timestamp       INTEGER NOT NULL,
            status          INTEGER NOT NULL DEFAULT 0
        );

        CREATE INDEX IF NOT EXISTS idx_messages_conv
            ON messages(conversation_id, timestamp);

        CREATE TABLE IF NOT EXISTS app_config (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )");
}
