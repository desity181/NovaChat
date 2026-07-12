#include <catch2/catch_test_macros.hpp>
#include "storage/DatabaseManager.h"
#include <sqlite3.h>
#include <string>

TEST_CASE("DatabaseManager creates schema on in-memory db without throwing", "[db]") {
    REQUIRE_NOTHROW(DatabaseManager(":memory:"));
}

TEST_CASE("DatabaseManager throws DatabaseException on invalid SQL", "[db]") {
    DatabaseManager db(":memory:");
    REQUIRE_THROWS_AS(db.execute("THIS IS NOT SQL"), DatabaseException);
}

TEST_CASE("DatabaseManager creates all expected tables", "[db]") {
    DatabaseManager db(":memory:");

    auto table_exists = [&](const std::string& name) -> bool {
        sqlite3_stmt* stmt;
        const std::string sql =
            "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='" + name + "'";
        sqlite3_prepare_v2(db.handle(), sql.c_str(), -1, &stmt, nullptr);
        sqlite3_step(stmt);
        int count = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return count > 0;
    };

    CHECK(table_exists("local_accounts"));
    CHECK(table_exists("contacts"));
    CHECK(table_exists("conversations"));
    CHECK(table_exists("messages"));
    CHECK(table_exists("app_config"));
}

TEST_CASE("DatabaseManager messages table has idx_messages_conv index", "[db]") {
    DatabaseManager db(":memory:");

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db.handle(),
        "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name='idx_messages_conv'",
        -1, &stmt, nullptr);
    sqlite3_step(stmt);
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    CHECK(count == 1);
}

TEST_CASE("DatabaseManager schema creation is idempotent (IF NOT EXISTS)", "[db]") {
    REQUIRE_NOTHROW(DatabaseManager(":memory:"));
    REQUIRE_NOTHROW(DatabaseManager(":memory:"));
}
