#include <catch2/catch_test_macros.hpp>
#include "storage/DatabaseManager.h"
#include "storage/UserRepository.h"

static LocalAccount makeAccount(const std::string& id,
                                 const std::string& name,
                                 const std::string& token,
                                 bool active = false) {
    LocalAccount acc;
    acc.server_id    = id;
    acc.username     = name;
    acc.display_name = name + "_display";
    acc.avatar_url   = "";
    acc.token        = token;
    acc.is_active    = active;
    return acc;
}

TEST_CASE("UserRepository save then findById returns the account", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "tok_a"));
    auto found = repo.findById("u001");

    REQUIRE(found.has_value());
    CHECK(found->username     == "alice");
    CHECK(found->display_name == "alice_display");
    CHECK(found->token        == "tok_a");
    CHECK(!found->is_active);
}

TEST_CASE("UserRepository findActive returns nullopt when no account is active", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "tok_a"));
    CHECK(!repo.findActive().has_value());
}

TEST_CASE("UserRepository setActive then findActive returns that account", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "tok_a"));
    repo.save(makeAccount("u002", "bob",   "tok_b"));

    repo.setActive("u001");
    auto active = repo.findActive();

    REQUIRE(active.has_value());
    CHECK(active->server_id == "u001");
    CHECK(active->is_active);
}

TEST_CASE("UserRepository setActive switches so only one account is active", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "tok_a"));
    repo.save(makeAccount("u002", "bob",   "tok_b"));

    repo.setActive("u001");
    repo.setActive("u002");

    auto active = repo.findActive();
    REQUIRE(active.has_value());
    CHECK(active->server_id == "u002");
}

TEST_CASE("UserRepository clearActive removes active flag", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "tok_a"));
    repo.setActive("u001");
    REQUIRE(repo.findActive().has_value());

    repo.clearActive();
    CHECK(!repo.findActive().has_value());
}

TEST_CASE("UserRepository updateToken updates token for existing account", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "old_token"));
    repo.updateToken("u001", "new_token");

    auto found = repo.findById("u001");
    REQUIRE(found.has_value());
    CHECK(found->token == "new_token");
}

TEST_CASE("UserRepository save with duplicate server_id updates rather than inserts", "[user_repo]") {
    DatabaseManager db(":memory:");
    UserRepository  repo(db);

    repo.save(makeAccount("u001", "alice", "tok_a"));
    repo.save(makeAccount("u001", "alice_updated", "tok_b"));

    auto found = repo.findById("u001");
    REQUIRE(found.has_value());
    CHECK(found->display_name == "alice_updated_display");
    CHECK(found->token        == "tok_b");
}
