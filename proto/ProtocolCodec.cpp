#include "proto/ProtocolCodec.h"
#include <cstring>

// ── Byte-order helpers ────────────────────────────────────────────
static uint32_t to_be32(uint32_t v) {
    return ((v & 0xFFU) << 24) | (((v >> 8) & 0xFFU) << 16) |
           (((v >> 16) & 0xFFU) << 8) | ((v >> 24) & 0xFFU);
}

static uint16_t to_be16(uint16_t v) {
    return static_cast<uint16_t>(((v & 0xFFU) << 8) | ((v >> 8) & 0xFFU));
}

static uint32_t from_be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
            static_cast<uint32_t>(p[3]);
}

static uint16_t from_be16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) |
                                   static_cast<uint16_t>(p[1]));
}

// ── Encode ────────────────────────────────────────────────────────
std::vector<uint8_t> ProtocolCodec::encode(const Packet& packet) {
    const size_t total = HEADER_SIZE + packet.payload.size();
    std::vector<uint8_t> out(total);
    uint8_t* p = out.data();

    const uint16_t magic  = to_be16(MAGIC);
    const uint32_t length = to_be32(static_cast<uint32_t>(packet.payload.size()));
    const uint16_t msg_id = to_be16(static_cast<uint16_t>(packet.type));
    const uint32_t seq_id = to_be32(packet.seq_id);

    std::memcpy(p, &magic,  2); p += 2;
    std::memcpy(p, &length, 4); p += 4;
    std::memcpy(p, &msg_id, 2); p += 2;
    std::memcpy(p, &seq_id, 4); p += 4;

    if (!packet.payload.empty()) {
        std::memcpy(p, packet.payload.data(), packet.payload.size());
    }
    return out;
}

// ── Decode ────────────────────────────────────────────────────────
void ProtocolCodec::feed(const uint8_t* data, size_t len, const PacketCallback& callback) {
    buffer_.insert(buffer_.end(), data, data + len);

    while (buffer_.size() >= HEADER_SIZE) {
        const uint8_t* h = buffer_.data();

        if (from_be16(h) != MAGIC) {
            // Frame sync lost — discard buffer
            buffer_.clear();
            return;
        }

        const uint32_t payload_len = from_be32(h + 2);
        const size_t   total       = HEADER_SIZE + payload_len;

        if (buffer_.size() < total) {
            break;  // Wait for more data
        }

        Packet pkt;
        pkt.type   = static_cast<MessageType>(from_be16(h + 6));
        pkt.seq_id = from_be32(h + 8);
        if (payload_len > 0) {
            pkt.payload.assign(h + HEADER_SIZE, h + total);
        }

        buffer_.erase(buffer_.begin(),
                      buffer_.begin() + static_cast<ptrdiff_t>(total));
        callback(std::move(pkt));
    }
}
