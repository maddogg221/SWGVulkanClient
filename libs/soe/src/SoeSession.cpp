#include "soe/SoeSession.h"

#include <iostream>
#include <random>
#include <utility>

#include "soe/Compression.h"
#include "soe/Crc32.h"
#include "soe/PacketBuffer.h"
#include "soe/XorCipher.h"

namespace soe {

namespace {

// Splits `buf`'s remaining bytes into one or more message payloads: a
// BaseMultiPacket bundle (detected via the 0x1900 marker) becomes several,
// anything else is treated as a single message spanning the rest of the
// buffer. Shared between a normal (unfragmented) Data Channel payload and a
// freshly-reassembled Fragmented payload - Core3's own reference client
// re-runs this exact same check after reassembling a Fragmented packet (it
// hands the completed packet to the same handler an ordinary Data Channel
// packet goes through), so a large bundled message must be supported here
// too, not just for single-packet payloads.
void extractDataChannelMessages(PacketBuffer& buf, std::vector<std::vector<uint8_t>>& messages) {
    if (buf.remaining() >= 2 && buf.peekUint16(buf.readPosition()) == 0x1900) {
        buf.readUint16(); // consume the multi-packet marker
        while (buf.remaining() > 0) {
            uint8_t lenByte = buf.readByte();
            size_t subLen = (lenByte == 0xFF) ? buf.readUint16BE() : lenByte;
            if (subLen > buf.remaining()) {
                break; // malformed bundle - keep whatever parsed cleanly so far
            }
            messages.push_back(buf.readBytes(subLen));
        }
    } else {
        messages.push_back(buf.readBytes(buf.remaining()));
    }
}

} // namespace

SoeSession::SoeSession(asio::io_context& io, const std::string& host, uint16_t port)
    : io_(io),
      workGuard_(asio::make_work_guard(io_)),
      socket_(io_, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0)) {
    asio::ip::udp::resolver resolver(io_);
    auto results = resolver.resolve(asio::ip::udp::v4(), host, std::to_string(port));
    remoteEndpoint_ = *results.begin();
    socket_.connect(remoteEndpoint_);
}

SoeSession::~SoeSession() {
    if (connected_) {
        try {
            disconnect();
        } catch (const std::exception& e) {
            // Best-effort only - a destructor must never throw, and there's
            // nothing more we can do if the final Disconnect fails to send
            // (the socket is being torn down regardless). Still worth a
            // diagnostic: this is the safety net for a real, previously
            // confirmed stuck-session bug, so silently losing track of
            // whether it fired would make a recurrence indistinguishable
            // from this fix failing.
            std::cerr << "SoeSession: final Disconnect failed to send: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "SoeSession: final Disconnect failed to send (unknown error)\n";
        }
    }

    std::error_code ec;
    socket_.close(ec);
}

void SoeSession::sendRaw(const std::vector<uint8_t>& bytes) {
    socket_.send(asio::buffer(bytes));
}

std::vector<uint8_t> SoeSession::receiveRawPacket(std::chrono::milliseconds timeout) {
    std::vector<uint8_t> buffer(2048);
    size_t bytesReceived = 0;
    bool timedOut = false;
    std::error_code recvError;
    bool recvDone = false;

    asio::steady_timer timer(io_);
    timer.expires_after(timeout);

    socket_.async_receive(asio::buffer(buffer),
        [&](const std::error_code& ec, size_t bytes) {
            recvError = ec;
            bytesReceived = bytes;
            recvDone = true;
            timer.cancel();
        });

    timer.async_wait([&](const std::error_code& ec) {
        if (!ec) {
            timedOut = true;
            socket_.cancel();
        }
    });

    // io_ is kept alive (and permanently "has work" via workGuard_) for the
    // whole session, so run_one() can be driven directly here: it blocks
    // until one ready handler runs, which we call repeatedly until the
    // receive itself completes (with real data, or with operation_aborted
    // from the timer's cancel() above) - no restart()/run() churn needed.
    while (!recvDone) {
        io_.run_one();
    }

    if (timedOut && bytesReceived == 0) {
        throw TimeoutError("SoeSession: timed out waiting for a packet");
    }
    if (recvError && recvError != asio::error::operation_aborted) {
        throw std::runtime_error("SoeSession: receive failed: " + recvError.message());
    }

    buffer.resize(bytesReceived);
    return buffer;
}

void SoeSession::connect(std::chrono::milliseconds timeout) {
    std::random_device rd;
    connectionId_ = rd();

    PacketBuffer request;
    request.writeUint16(0x0100); // SessionRequest opcode
    request.writeUint16(0x0000);
    request.writeUint16BE(2);    // crcLength
    request.writeUint32(connectionId_);
    request.writeUint32BE(496);  // maxRawSize (engine3 Packet::RAW_MAX_SIZE)

    sendRaw(std::vector<uint8_t>(request.data(), request.data() + request.size()));

    auto raw = receiveRawPacket(timeout);
    if (raw.size() != 17) {
        throw std::runtime_error(
            "SoeSession: unexpected SessionResponse size " + std::to_string(raw.size()));
    }

    PacketBuffer response(raw.data(), raw.size());
    uint16_t opcode = response.readUint16();
    if (opcode != 0x0200) {
        throw std::runtime_error(
            "SoeSession: expected SessionResponse (0x0200), got 0x" + std::to_string(opcode));
    }

    response.readUint32(); // echoed connectionID, unused
    crcSeed_ = response.readUint32BE();
    // remaining 7 trailer bytes are a fixed constant, ignored.

    connected_ = true;
}

void SoeSession::sendMessage(const std::vector<uint8_t>& payload) {
    PacketBuffer buf;
    buf.writeUint16(0x0900);
    buf.writeUint16(0); // sequence placeholder, patched below
    buf.writeBytes(payload);
    buf.writeByte(0x00); // compression flag - never compress; our messages are small
    buf.writeUint16(0);  // CRC placeholder, overwritten by PacketCrc::append

    buf.writeUint16BEAt(2, sendSequence_);
    ++sendSequence_;

    XorCipher::encrypt(buf, crcSeed_);
    PacketCrc::append(buf, crcSeed_);

    sendRaw(std::vector<uint8_t>(buf.data(), buf.data() + buf.size()));
}

void SoeSession::disconnect() {
    if (!connected_) {
        return; // already disconnected (or never connected) - idempotent no-op
    }

    PacketBuffer buf;
    buf.writeUint16(0x0500); // Disconnect opcode
    buf.writeUint32(connectionId_);
    buf.writeUint16(0x0600); // fixed trailer constant, per DisconnectMessage
    buf.writeByte(0x00);     // compression flag
    buf.writeUint16(0);      // CRC placeholder

    // No sequence number: Core3's DisconnectMessage explicitly disables
    // sequencing for this packet (its own layout already uses the sequence
    // field's usual byte offset for connectionID).
    XorCipher::encrypt(buf, crcSeed_);
    PacketCrc::append(buf, crcSeed_);

    // Only mark ourselves disconnected once the send actually succeeds - if
    // sendRaw() throws, connected_ stays true so a later retry (including
    // the destructor's own best-effort attempt) still tries to notify the
    // server, instead of the failure silently disarming that safety net.
    sendRaw(std::vector<uint8_t>(buf.data(), buf.data() + buf.size()));
    connected_ = false;
}

void SoeSession::sendAck(uint16_t sequence) {
    PacketBuffer buf;
    buf.writeUint16(0x1500); // Acknowledge opcode
    buf.writeUint16BE(sequence);
    buf.writeByte(0x00); // compression flag
    buf.writeUint16(0);  // CRC placeholder

    XorCipher::encrypt(buf, crcSeed_);
    PacketCrc::append(buf, crcSeed_);

    sendRaw(std::vector<uint8_t>(buf.data(), buf.data() + buf.size()));
}

std::vector<std::vector<uint8_t>> SoeSession::receiveMessages(std::chrono::milliseconds timeout) {
    auto raw = receiveRawPacket(timeout);
    PacketBuffer buf(raw.data(), raw.size());

    if (buf.size() < 2) {
        return {}; // too small to even carry an opcode; drop it
    }

    uint16_t opcode = buf.peekUint16(0);

    // CRC must be validated before trusting ANY opcode's semantics - including
    // Disconnect. Core3's own BasePacketHandler routes every opcode through
    // processRecieve() (testCRC -> decrypt) first; checking Disconnect before
    // CRC would let a corrupted-in-transit or spoofed packet force an
    // unverified session teardown.
    if (!PacketCrc::test(buf, crcSeed_)) {
        return {}; // corrupt/forged packet, drop it
    }

    if (opcode == 0x0500) {
        // The peer already knows the connection is ending - no need for us
        // to also send our own Disconnect when this session is torn down.
        connected_ = false;
        throw DisconnectedError("SoeSession: peer sent Disconnect");
    }
    if (opcode != 0x0900 && opcode != 0x0D00) {
        // Control packet (Ack, NetStatus, etc.) - nothing to hand back.
        return {};
    }

    try {
        XorCipher::decrypt(buf, crcSeed_);

        // A valid Data Channel message needs at least 2 (opcode) + 2
        // (sequence) + 1 (compression flag) + 2 (CRC) = 7 bytes, even with an
        // empty payload. Without this check, buf.size()-3 below can underflow
        // for a malformed/undersized packet and read out of bounds.
        if (buf.size() < 7) {
            return {};
        }

        uint8_t compFlag = buf.peekByte(buf.size() - 3);
        if (compFlag == 0x01) {
            uint8_t firstByte = buf.peekByte(0);
            size_t headerLen = (firstByte == 0x00) ? 2 : 1;
            size_t compressedLen = buf.size() - headerLen - 3;

            auto inflated = Compression::decompress(buf.data() + headerLen, compressedLen, 8192);

            PacketBuffer rebuilt(headerLen + inflated.size() + 3);
            rebuilt.writeBytes(buf.data(), headerLen);
            rebuilt.writeBytes(inflated.data(), inflated.size());
            rebuilt.writeBytes(buf.data() + buf.size() - 3, 3);
            buf = std::move(rebuilt);
        }

        buf.removeLastBytes(3);

        buf.readUint16(); // opcode, already known (0x0900 or 0x0D00)
        uint16_t seq = buf.readUint16BE();

        // Every Data Channel or Fragmented packet that reaches here passed
        // CRC and decrypted cleanly - ack it now regardless of what happens
        // next. An invalid/incomplete fragment or a malformed bundle inside
        // it is an application-level issue CRC can't catch, not evidence
        // the packet itself was lost in transit.
        sendAck(seq);

        std::vector<std::vector<uint8_t>> messages;

        if (opcode == 0x0D00) {
            auto fragment = fragmentReassembler_.addFragment(seq, buf);

            if (fragment.status == FragmentStatus::Invalid) {
                // Reported via the return value rather than an exception
                // specifically so this can't be silently absorbed by the
                // catch below with no log line - see FragmentReassembler's
                // class comment.
                std::cerr << "SoeSession: dropping fragmented packet (seq=" << seq
                           << "): " << fragment.error << "\n";
                return {};
            }
            if (fragment.status == FragmentStatus::Incomplete) {
                return {}; // more fragments still needed
            }

            PacketBuffer full(fragment.payload.data(), fragment.payload.size());
            extractDataChannelMessages(full, messages);
        } else {
            extractDataChannelMessages(buf, messages);
        }

        return messages;
    } catch (const std::exception& e) {
        // A CRC-valid packet can still have internally-inconsistent
        // application-level length fields (CRC only proves the bytes weren't
        // corrupted in transit) - drop this one malformed packet rather than
        // taking down the whole session, but log it: silently swallowing
        // every exception here previously made a real dropped-packet
        // scenario indistinguishable from nothing happening at all.
        std::cerr << "SoeSession: dropping malformed packet: " << e.what() << "\n";
        return {};
    }
}

} // namespace soe
