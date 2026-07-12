#pragma once
#include <sqlite3.h>
#include <stdexcept>
#include <string>

class DatabaseException : public std::runtime_error {
public:
    explicit DatabaseException(const std::string& msg)
        : std::runtime_error("DatabaseException: " + msg) {}
};

// RAII SQLite connection; creates schema on construction.
// All methods must be called from the owning thread.
class DatabaseManager {
public:
    explicit DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager&)            = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    sqlite3* handle() const noexcept { return db_; }

    // Execute SQL that returns no rows (DDL / UPDATE / DELETE / PRAGMA)
    void execute(const std::string& sql);

private:
    sqlite3* db_{nullptr};
    void createSchema();
};
