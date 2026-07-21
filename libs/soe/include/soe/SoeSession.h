#pragma once

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <asio.hpp>

#include "soe/FragmentReassembler.h"

namespace soe {

// Thrown when a receive times out with no packet arriving.
class TimeoutError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Thrown when the peer sends an SOE Disconnect (opcode 0x0500).
class DisconnectedError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A single SOE (Session-Oriented-Encapsulation) UDP connection: handshake,
// Data Channel wrap/unwrap, sequencing, and Ack handling. This is a
// synchronous/blocking session, sufficient for a one-shot CLI flow like
// dummyclient's login sequence - not a general-purpose async networking
// core (that's a later concern once rendering/input need to share an event
// loop with networking).
//
// Does NOT own its io_context - the caller passes one in (and keeps it
// alive for at least this session's lifetime), so multiple SoeSession
// instances used sequentially by the same process (e.g. dummyclient's
// login session, then its zone session) can share one, rather than each
// spinning up its own.
//
// See DISCOVERY.txt for the wire format this implements: real clients send
// a 14-byte SessionRequest (not Core3's own toy 10-byte one), sequence
// numbers are big-endian, and login responses commonly arrive bundled via
// the BaseMultiPacket 0x1900 marker rather than as separate packets.
class SoeSession {
public:
    SoeSession(asio::io_context& io, const std::string& host, uint16_t port);
    ~SoeSession();

    SoeSession(const SoeSession&) = delete;
    SoeSession& operator=(const SoeSession&) = delete;

    // Performs the SOE handshake (SessionRequest/SessionResponse). Throws
    // TimeoutError if the server never responds. After this returns,
    // crcSeed() is valid and sendMessage()/receiveMessages() can be used.
    void connect(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // Wraps `payload` (opCount+hash+fields, already serialized by a
    // swgproto builder) in a Data Channel packet and sends it: assigns the
    // next outgoing sequence number, applies the close()/encrypt/appendCRC
    // pipeline, and writes it to the socket.
    void sendMessage(const std::vector<uint8_t>& payload);

    // Blocks until a packet arrives or `timeout` elapses (throws
    // TimeoutError). Handles the receive pipeline (testCRC -> decrypt ->
    // decompress if flagged), automatically ACKs sequenced Data Channel and
    // Fragmented packets, and returns the application message payload(s)
    // it carried - more than one if the packet (or a completed Fragmented
    // reassembly) was a BaseMultiPacket bundle (the 0x1900 marker), exactly
    // one otherwise. A Fragmented packet (SOE opcode 0x0D00) that doesn't
    // complete a reassembly on its own produces an empty result just like a
    // control packet does - call again to keep feeding it more fragments.
    // Control packets (Ack, NetStatus) are also consumed internally and
    // produce an empty result - call again for the next application
    // message. Throws DisconnectedError if the peer sends an SOE
    // Disconnect.
    std::vector<std::vector<uint8_t>> receiveMessages(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));

    // Sends an SOE Disconnect (opcode 0x0500) to the peer, matching Core3's
    // own DisconnectMessage - NOT a Data Channel (0x0900) message, and not
    // sequenced (Core3 explicitly disables sequencing for it, since this
    // packet's own layout already uses the sequence field's usual byte
    // offset for the connectionID). Idempotent - a second call (or the
    // destructor's automatic call, see below) is a no-op. Leaving a session
    // without a graceful disconnect was observed to make the server
    // increasingly likely to reject or hang a subsequent connection
    // attempt for the same character shortly afterward.
    void disconnect();

    uint32_t crcSeed() const { return crcSeed_; }

private:
    std::vector<uint8_t> receiveRawPacket(std::chrono::milliseconds timeout);
    void sendAck(uint16_t sequence);
    void sendRaw(const std::vector<uint8_t>& bytes);

    // Externally owned - see the class comment above.
    asio::io_context& io_;
    // Keeps io_ "primed" for as long as THIS session is alive (never out of
    // work on this session's account), so receiveRawPacket() can drive it
    // with io_.run_one() on every call without the restart() dance an
    // io_context needs once it's actually stopped/out of work - restart()
    // is a call for OCCASIONAL reuse after exhaustion, not a per-receive
    // idiom. See receiveRawPacket() for the run_one() loop. Harmless for
    // multiple SoeSession instances sharing one io_context to each hold
    // their own guard - the io_context only goes idle once every guard on
    // it has been released.
    asio::executor_work_guard<asio::io_context::executor_type> workGuard_;
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remoteEndpoint_;

    uint32_t connectionId_ = 0;
    uint32_t crcSeed_ = 0;
    uint16_t sendSequence_ = 0;
    // Set once connect() succeeds, cleared once we send our own Disconnect
    // or learn the peer already sent one. The destructor uses this to send
    // a best-effort graceful Disconnect on ANY exit path (normal return,
    // exception, early return) without every call site needing to remember
    // to call disconnect() itself.
    bool connected_ = false;

    // Fragmented-message (SOE opcode 0x0D00) reassembly state - persists
    // across receiveRawPacket() calls since one logical message can span
    // more UDP datagrams than fit in a single receiveMessages() call. See
    // FragmentReassembler for the wire format it implements.
    FragmentReassembler fragmentReassembler_;
};

} // namespace soe
