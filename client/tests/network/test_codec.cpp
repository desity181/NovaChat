#include <catch2/catch_test_macros.hpp>
#include "proto/ProtocolCodec.h"

static Packet make_packet(MessageType type, uint32_t seq,
                           std::vector<uint8_t> payload = {}) {
    Packet p;
    p.type    = type;
    p.seq_id  = seq;
    p.payload = std::move(payload);
    return p;
}

TEST_CASE("ProtocolCodec encode/decode round-trip", "[codec]") {
    auto original = make_packet(MessageType::HeartbeatRequest, 42, {0x01, 0x02, 0x03});
    auto encoded  = ProtocolCodec::encode(original);

    ProtocolCodec codec;
    std::vector<Packet> decoded;
    codec.feed(encoded.data(), encoded.size(),
               [&decoded](Packet p) { decoded.push_back(std::move(p)); });

    REQUIRE(decoded.size() == 1);
    CHECK(decoded[0].type    == original.type);
    CHECK(decoded[0].seq_id  == original.seq_id);
    CHECK(decoded[0].payload == original.payload);
}

TEST_CASE("ProtocolCodec encodes empty payload correctly", "[codec]") {
    auto original = make_packet(MessageType::HeartbeatResponse, 0);
    auto encoded  = ProtocolCodec::encode(original);

    CHECK(encoded.size() == ProtocolCodec::HEADER_SIZE);

    ProtocolCodec codec;
    std::vector<Packet> decoded;
    codec.feed(encoded.data(), encoded.size(),
               [&decoded](Packet p) { decoded.push_back(std::move(p)); });

    REQUIRE(decoded.size() == 1);
    CHECK(decoded[0].payload.empty());
}

TEST_CASE("ProtocolCodec handles byte-by-byte feed (extreme fragmentation)", "[codec]") {
    auto original = make_packet(MessageType::LoginRequest, 7, {0xAB, 0xCD});
    auto encoded  = ProtocolCodec::encode(original);

    ProtocolCodec codec;
    std::vector<Packet> decoded;
    auto cb = [&decoded](Packet p) { decoded.push_back(std::move(p)); };

    for (size_t i = 0; i < encoded.size(); ++i) {
        codec.feed(&encoded[i], 1, cb);
        if (i < encoded.size() - 1) {
            CHECK(decoded.empty());
        }
    }
    REQUIRE(decoded.size() == 1);
    CHECK(decoded[0].seq_id == 7);
}

TEST_CASE("ProtocolCodec handles concatenated packets (TCP coalescing)", "[codec]") {
    auto p1 = make_packet(MessageType::HeartbeatRequest,  1);
    auto p2 = make_packet(MessageType::HeartbeatResponse, 2);

    auto enc1 = ProtocolCodec::encode(p1);
    auto enc2 = ProtocolCodec::encode(p2);

    std::vector<uint8_t> combined;
    combined.insert(combined.end(), enc1.begin(), enc1.end());
    combined.insert(combined.end(), enc2.begin(), enc2.end());

    ProtocolCodec codec;
    std::vector<Packet> decoded;
    codec.feed(combined.data(), combined.size(),
               [&decoded](Packet p) { decoded.push_back(std::move(p)); });

    REQUIRE(decoded.size() == 2);
    CHECK(decoded[0].seq_id == 1);
    CHECK(decoded[1].seq_id == 2);
}

TEST_CASE("ProtocolCodec invalid magic clears buffer and decodes nothing", "[codec]") {
    std::vector<uint8_t> bad = {
        0xFF, 0xFF,              // wrong magic
        0x00, 0x00, 0x00, 0x00, // payload length = 0
        0x0F, 0x01,              // HeartbeatRequest
        0x00, 0x00, 0x00, 0x01  // seq_id = 1
    };

    ProtocolCodec codec;
    std::vector<Packet> decoded;
    codec.feed(bad.data(), bad.size(),
               [&decoded](Packet p) { decoded.push_back(std::move(p)); });

    CHECK(decoded.empty());
}

TEST_CASE("ProtocolCodec large payload round-trip", "[codec]") {
    std::vector<uint8_t> large_payload(4096, 0xAA);
    auto original = make_packet(MessageType::SendMessageRequest, 99, large_payload);
    auto encoded  = ProtocolCodec::encode(original);

    ProtocolCodec codec;
    std::vector<Packet> decoded;
    codec.feed(encoded.data(), encoded.size(),
               [&decoded](Packet p) { decoded.push_back(std::move(p)); });

    REQUIRE(decoded.size() == 1);
    CHECK(decoded[0].payload == large_payload);
}
