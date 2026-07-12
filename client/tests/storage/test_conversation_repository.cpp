#include <catch2/catch_test_macros.hpp>
#include "storage/DatabaseManager.h"
#include "storage/ConversationRepository.h"

static Conversation makeConv(const std::string& owner,
                              const std::string& conv_id,
                              const std::string& target,
                              int64_t ts, int unread = 0) {
    Conversation c;
    c.owner_id             = owner;
    c.conversation_id      = conv_id;
    c.target_id            = target;
    c.last_message_preview = "last msg";
    c.last_message_time    = ts;
    c.unread_count         = unread;
    return c;
}

TEST_CASE("ConversationRepository saveOrUpdate and findAll", "[conv_repo]") {
    DatabaseManager        db(":memory:");
    ConversationRepository repo(db);

    repo.saveOrUpdate(makeConv("o1", "c1", "alice", 1000));
    auto all = repo.findAll("o1");

    REQUIRE(all.size() == 1);
    CHECK(all[0].target_id == "alice");
}

TEST_CASE("ConversationRepository saveOrUpdate updates existing conversation", "[conv_repo]") {
    DatabaseManager        db(":memory:");
    ConversationRepository repo(db);

    repo.saveOrUpdate(makeConv("o1", "c1", "alice", 1000));
    repo.saveOrUpdate(makeConv("o1", "c1", "alice", 2000, 3));

    auto all = repo.findAll("o1");
    REQUIRE(all.size() == 1);
    CHECK(all[0].last_message_time == 2000);
    CHECK(all[0].unread_count      == 3);
}

TEST_CASE("ConversationRepository findAll orders by last_message_time descending", "[conv_repo]") {
    DatabaseManager        db(":memory:");
    ConversationRepository repo(db);

    repo.saveOrUpdate(makeConv("o1", "c1", "alice", 1000));
    repo.saveOrUpdate(makeConv("o1", "c2", "bob",   3000));
    repo.saveOrUpdate(makeConv("o1", "c3", "carol", 2000));

    auto all = repo.findAll("o1");
    REQUIRE(all.size() == 3);
    CHECK(all[0].conversation_id == "c2");
    CHECK(all[1].conversation_id == "c3");
    CHECK(all[2].conversation_id == "c1");
}

TEST_CASE("ConversationRepository updateLastMessage", "[conv_repo]") {
    DatabaseManager        db(":memory:");
    ConversationRepository repo(db);

    repo.saveOrUpdate(makeConv("o1", "c1", "alice", 1000));
    repo.updateLastMessage("o1", "c1", "new preview", 9999);

    auto all = repo.findAll("o1");
    REQUIRE(all.size() == 1);
    CHECK(all[0].last_message_preview == "new preview");
    CHECK(all[0].last_message_time    == 9999);
}

TEST_CASE("ConversationRepository incrementUnread and clearUnread", "[conv_repo]") {
    DatabaseManager        db(":memory:");
    ConversationRepository repo(db);

    repo.saveOrUpdate(makeConv("o1", "c1", "alice", 1000, 0));
    repo.incrementUnread("o1", "c1");
    repo.incrementUnread("o1", "c1");

    {
        auto all = repo.findAll("o1");
        CHECK(all[0].unread_count == 2);
    }

    repo.clearUnread("o1", "c1");
    {
        auto all = repo.findAll("o1");
        CHECK(all[0].unread_count == 0);
    }
}
