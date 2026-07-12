#pragma once
#include "proto/MessageType.h"
#include <cstdint>
#include <functional>
#include <vector>

// Binary frame layout (12-byte fixed header + variable-length payload)
// ┌─────────┬────────────┬──────────┬──────────┬─────────────────┐
// │Magic(2B)│Payload(4B) │MsgId(2B) │SeqId(4B) │Payload(nB)      │
// │ 0x4E43  │  length    │  type    │ seq num  │ Protobuf bytes  │
// └─────────┴────────────┴──────────┴──────────┴─────────────────┘

struct Packet {
    MessageType          type{MessageType::HeartbeatRequest};
    uint32_t             seq_id{0};
    std::vector<uint8_t> payload;
};

class ProtocolCodec {
public:
    static constexpr uint16_t MAGIC       = 0x4E43;  // "NC"
    static constexpr size_t   HEADER_SIZE = 12;

    using PacketCallback = std::function<void(Packet)>;

    // Serialize a Packet into a byte stream ready to send (big-endian)
    static std::vector<uint8_t> encode(const Packet& packet);

    // Feed received bytes into the internal buffer; invoke callback for each
    // complete frame decoded. A single ProtocolCodec instance is not thread-safe.
    void feed(const uint8_t* data, size_t len, const PacketCallback& callback);

private:
    std::vector<uint8_t> buffer_;
};

// Register as Qt meta-type for cross-thread signal delivery
#include <QMetaType>
Q_DECLARE_METATYPE(Packet)
