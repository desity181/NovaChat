#include <catch2/catch_test_macros.hpp>
#include "storage/DatabaseManager.h"
#include "storage/ContactRepository.h"

static Contact makeContact(const std::string& owner,
                            const std::string& uid,
                            const std::string& name) {
    Contact c;
    c.owner_id     = owner;
    c.user_id      = uid;
    c.username     = name;
    c.display_name = name + "_display";
    c.avatar_url   = "";
    return c;
}

TEST_CASE("ContactRepository save then findById returns the contact", "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    repo.save(makeContact("owner1", "u001", "alice"));
    auto found = repo.findById("owner1", "u001");

    REQUIRE(found.has_value());
    CHECK(found->username     == "alice");
    CHECK(found->display_name == "alice_display");
}

TEST_CASE("ContactRepository findAll returns empty for unknown owner", "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    CHECK(repo.findAll("owner_nobody").empty());
}

TEST_CASE("ContactRepository findAll orders by display_name", "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    repo.save(makeContact("o1", "u3", "charlie"));
    repo.save(makeContact("o1", "u1", "alice"));
    repo.save(makeContact("o1", "u2", "bob"));

    auto list = repo.findAll("o1");
    REQUIRE(list.size() == 3);
    CHECK(list[0].username == "alice");
    CHECK(list[1].username == "bob");
    CHECK(list[2].username == "charlie");
}

TEST_CASE("ContactRepository save with duplicate (owner,user_id) updates not inserts", "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    repo.save(makeContact("o1", "u1", "alice"));
    repo.save(makeContact("o1", "u1", "alice_renamed"));

    auto all = repo.findAll("o1");
    REQUIRE(all.size() == 1);
    CHECK(all[0].display_name == "alice_renamed_display");
}

TEST_CASE("ContactRepository remove deletes the specified contact", "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    repo.save(makeContact("o1", "u1", "alice"));
    repo.save(makeContact("o1", "u2", "bob"));
    repo.remove("o1", "u1");

    auto all = repo.findAll("o1");
    REQUIRE(all.size() == 1);
    CHECK(all[0].user_id == "u2");
}

TEST_CASE("ContactRepository replaceAll does a full replacement", "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    repo.save(makeContact("o1", "u1", "alice"));
    repo.save(makeContact("o1", "u2", "bob"));

    std::vector<Contact> new_list = {makeContact("o1", "u3", "charlie"),
                                      makeContact("o1", "u4", "dave")};
    repo.replaceAll("o1", new_list);

    auto all = repo.findAll("o1");
    REQUIRE(all.size() == 2);
    CHECK(all[0].username == "charlie");
    CHECK(all[1].username == "dave");
}

TEST_CASE("ContactRepository different owners are isolated", "[contact_repo]") {
    DatabaseManager   db(":memory:");
    ContactRepository repo(db);

    repo.save(makeContact("owner_a", "u1", "alice"));
    repo.save(makeContact("owner_b", "u1", "alice_b"));

    auto a_list = repo.findAll("owner_a");
    auto b_list = repo.findAll("owner_b");

    REQUIRE(a_list.size() == 1);
    REQUIRE(b_list.size() == 1);
    CHECK(a_list[0].display_name == "alice_display");
    CHECK(b_list[0].display_name == "alice_b_display");
}
