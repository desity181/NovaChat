#include <catch2/catch_test_macros.hpp>
#include "storage/DatabaseManager.h"
#include "storage/MessageRepository.h"

static Message makeMsg(const std::string& id,
                        const std::string& conv,
                        const std::string& sender,
                        int64_t ts,
                        MessageStatus status = MessageStatus::Sending) {
    Message m;
    m.message_id      = id;
    m.conversation_id = conv;
    m.sender_id       = sender;
    m.content         = "Hello from " + sender;
    m.type            = MessageContentType::Text;
    m.timestamp_ms    = ts;
    m.status          = status;
    return m;
}

TEST_CASE("MessageRepository save and findById", "[msg_repo]") {
    DatabaseManager   db(":memory:");
    MessageRepository repo(db);

    repo.save(makeMsg("m1", "conv_a", "alice", 1000));
    auto found = repo.findById("m1");

    REQUIRE(found.has_value());
    CHECK(found->content      == "Hello from alice");
    CHECK(found->timestamp_ms == 1000);
}

TEST_CASE("MessageRepository duplicate message_id is silently ignored", "[msg_repo]") {
    DatabaseManager   db(":memory:");
    MessageRepository repo(db);

    repo.save(makeMsg("m1", "conv_a", "alice", 1000));
    REQUIRE_NOTHROW(repo.save(makeMsg("m1", "conv_a", "alice", 2000)));

    auto found = repo.findById("m1");
    REQUIRE(found.has_value());
    CHECK(found->timestamp_ms == 1000);  // Original timestamp preserved
}

TEST_CASE("MessageRepository findByConversation returns ascending order", "[msg_repo]") {
    DatabaseManager   db(":memory:");
    MessageRepository repo(db);

    repo.save(makeMsg("m3", "conv_a", "alice", 3000));
    repo.save(makeMsg("m1", "conv_a", "bob",   1000));
    repo.save(makeMsg("m2", "conv_a", "alice", 2000));

    auto msgs = repo.findByConversation("conv_a");
    REQUIRE(msgs.size() == 3);
    CHECK(msgs[0].message_id == "m1");
    CHECK(msgs[1].message_id == "m2");
    CHECK(msgs[2].message_id == "m3");
}

TEST_CASE("MessageRepository findByConversation pagination with before_timestamp", "[msg_repo]") {
    DatabaseManager   db(":memory:");
    MessageRepository repo(db);

    for (int i = 1; i <= 5; ++i)
        repo.save(makeMsg("m" + std::to_string(i), "conv_a", "alice",
                          static_cast<int64_t>(i) * 1000));

    // Fetch messages with timestamp < 4000, limit 2
    auto msgs = repo.findByConversation("conv_a", 4000, 2);
    REQUIRE(msgs.size() == 2);
    CHECK(msgs[0].message_id == "m2");
    CHECK(msgs[1].message_id == "m3");
}

TEST_CASE("MessageRepository updateStatus", "[msg_repo]") {
    DatabaseManager   db(":memory:");
    MessageRepository repo(db);

    repo.save(makeMsg("m1", "conv_a", "alice", 1000, MessageStatus::Sending));
    repo.updateStatus("m1", static_cast<int>(MessageStatus::Sent));

    auto found = repo.findById("m1");
    REQUIRE(found.has_value());
    CHECK(found->status == MessageStatus::Sent);
}

TEST_CASE("MessageRepository confirmSent replaces local_id with server_id", "[msg_repo]") {
    DatabaseManager   db(":memory:");
    MessageRepository repo(db);

    repo.save(makeMsg("local_tmp_001", "conv_a", "alice", 1000, MessageStatus::Sending));
    repo.confirmSent("local_tmp_001", "server_abc", 9999);

    CHECK(!repo.findById("local_tmp_001").has_value());
    auto found = repo.findById("server_abc");
    REQUIRE(found.has_value());
    CHECK(found->timestamp_ms == 9999);
    CHECK(found->status       == MessageStatus::Sent);
}

TEST_CASE("MessageRepository different conversations are isolated", "[msg_repo]") {
    DatabaseManager   db(":memory:");
    MessageRepository repo(db);

    repo.save(makeMsg("m1", "conv_a", "alice", 1000));
    repo.save(makeMsg("m2", "conv_b", "bob",   1000));

    CHECK(repo.findByConversation("conv_a").size() == 1);
    CHECK(repo.findByConversation("conv_b").size() == 1);
    CHECK(repo.findByConversation("conv_c").empty());
}
