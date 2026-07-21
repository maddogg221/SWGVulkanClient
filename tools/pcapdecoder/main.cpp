// Offline decoder for a previously captured pcap's server->client zone
// traffic, extracted to a plain hex-per-line text file beforehand, e.g.:
//   tshark -r capture.pcap -Y "ip.src==<server> and udp.srcport==<zoneport>
//     and udp.dstport==<clientport>" -T fields -e data.data > hex.txt
// Reuses the exact same decrypt/decompress/defragment/bundle-split pipeline
// SoeSession::receiveMessages() uses live, so a captured session can be
// analyzed after the fact without needing a second live connection - built
// specifically because Finalizer enforces one character online per account,
// which rules out a live nearby dummyclient observer whenever the user is
// already playing on their own real client. Proved its worth in Phase 4: a
// captured real combat session showed ObjControllerMessage's CombatAction/
// CombatSpam sub-types are never used at all on this server - real combat
// feedback runs through ShowFlyText hit-location strings plus CreatureObject
// baseline deltas (shockWounds/stateBitmask/posture) instead. See
// DISCOVERY.txt's "PHASE 4 STEP 3 COMPLETE" for the full finding.
//
// Requires the zone connection's crcSeed, which is only ever sent once (in
// cleartext) in the SessionResponse packet at the very start of that
// connection - the input hex file MUST include that packet as its first
// line, or --crc-seed must be supplied explicitly (needed whenever the
// capture starts mid-session, e.g. after the user was already logged in).
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "clientcommon/HexDump.h"
#include "clientcommon/ObjControllerHandlers.h"
#include "soe/Compression.h"
#include "soe/Crc32.h"
#include "soe/FragmentReassembler.h"
#include "soe/MessageDispatcher.h"
#include "soe/PacketBuffer.h"
#include "soe/XorCipher.h"
#include "swgproto/BaselineEnvelope.h"
#include "swgproto/ChatSystemMessage.h"
#include "swgproto/CmdStartScene.h"
#include "swgproto/CreatureObjectDelta.h"
#include "swgproto/ObjControllerDispatcher.h"
#include "swgproto/ObjControllerMessage.h"

namespace {

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    bytes.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        bytes.push_back(static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return bytes;
}

// ASCII-only preview, matching dummyclient's own toUtf8Preview() - good
// enough for logging typed chat/system text in a capture dump.
std::string toUtf8Preview(const std::u16string& s) {
    std::string out;
    out.reserve(s.size());
    for (char16_t ch : s) {
        out.push_back(static_cast<char>(ch));
    }
    return out;
}

// Mirrors SoeSession.cpp's private extractDataChannelMessages() - see that
// file's comment for why a bundle needs the same split logic whether it
// arrived as one packet or was just reassembled from fragments.
void extractDataChannelMessages(soe::PacketBuffer& buf,
                                 std::vector<std::vector<uint8_t>>& messages) {
    if (buf.remaining() >= 2 && buf.peekUint16(buf.readPosition()) == 0x1900) {
        buf.readUint16();
        while (buf.remaining() > 0) {
            uint8_t lenByte = buf.readByte();
            size_t subLen = (lenByte == 0xFF) ? buf.readUint16BE() : lenByte;
            if (subLen > buf.remaining()) {
                break;
            }
            messages.push_back(buf.readBytes(subLen));
        }
    } else {
        messages.push_back(buf.readBytes(buf.remaining()));
    }
}

// Top-level SOE MultiPacket (opcode 0x0300) unwrap - a genuinely different
// mechanism from the 0x1900 Data Channel bundle marker above (that one bundles
// several *application* messages inside one Data Channel payload; this one
// bundles several independent *SOE* packets - Acks, a Data Channel packet,
// a Fragmented chunk, etc. - into one UDP datagram). Never seen from the
// server in this project's own captures so far, but the real official client
// uses it heavily for its own outbound traffic (confirmed via a real Phase 17
// capture: ~48% of captured client->server packets were 0x0300). Ported from
// Core3's BasePacketHandler::handleMultiPacket() - CRC/decrypt/decompress
// already happened generically on the whole packet before this runs, exactly
// like a plain Data Channel packet, so `buf` here is plaintext positioned
// right after the 0x0300 opcode itself (no top-level sequence number, unlike
// Data Channel/Fragmented).
void extractMultiPacketMessages(soe::PacketBuffer& buf, soe::FragmentReassembler& reassembler,
                                 std::vector<std::vector<uint8_t>>& messages) {
    while (buf.remaining() > 0) {
        uint8_t blockSize = buf.readByte();
        if (blockSize == 0 || blockSize > buf.remaining()) {
            break; // malformed bundle - keep whatever parsed cleanly so far
        }
        auto blockBytes = buf.readBytes(blockSize);
        soe::PacketBuffer block(blockBytes.data(), blockBytes.size());
        if (block.remaining() < 2) {
            continue;
        }
        uint16_t subOpcode = block.readUint16();
        if (subOpcode == 0x0900) {
            if (block.remaining() < 2) {
                continue;
            }
            block.readUint16BE(); // sub-packet sequence, unused here
            extractDataChannelMessages(block, messages);
        } else if (subOpcode == 0x0D00) {
            if (block.remaining() < 2) {
                continue;
            }
            uint16_t subSeq = block.readUint16BE();
            auto fragment = reassembler.addFragment(subSeq, block);
            if (fragment.status == soe::FragmentStatus::Complete) {
                soe::PacketBuffer full(fragment.payload.data(), fragment.payload.size());
                extractDataChannelMessages(full, messages);
            }
        }
        // Anything else (Ack 0x1500, OutOfOrder 0x1100, OK 0x0001, ...) is a
        // control sub-packet - already fully consumed via readBytes(blockSize)
        // above, nothing more to do with it.
    }
}

struct CaptureStats {
    std::map<uint32_t, size_t> countsByType;
    static constexpr size_t kMaxSamplesPerType = 8;
    std::map<uint32_t, std::vector<std::vector<uint8_t>>> unknownSamples;
};

struct CliOptions {
    std::string inputPath;
    uint32_t crcSeed = 0;
    bool haveCrcSeed = false;

    // DeltasMessage decode is opt-in (most captures don't need it, and
    // BaselineEnvelope::parse() + decodeCreatureObjectDelta() add real
    // per-packet cost) - neither flag set means DeltasMessage traffic is
    // left to the "other top-level hashes" tally, same as any other
    // undecoded message type.
    uint64_t trackObjectId = 0; // decode CreatureObject BASE1/3 deltas for this one objectId
    bool trackAllCreo = false;  // decode CreatureObject BASE1/3 deltas for EVERY objectId -
                                 // e.g. finding which NPC died, not just the player's own state
};

CliOptions parseCommandLine(int argc, char** argv) {
    CliOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++i];
        };
        if (arg == "--input") {
            opts.inputPath = next("--input");
        } else if (arg == "--crc-seed") {
            opts.crcSeed = static_cast<uint32_t>(std::stoul(next("--crc-seed"), nullptr, 16));
            opts.haveCrcSeed = true;
        } else if (arg == "--track-object") {
            opts.trackObjectId = std::stoull(next("--track-object"));
        } else if (arg == "--track-all-creo") {
            opts.trackAllCreo = true;
        }
    }
    if (opts.inputPath.empty()) {
        throw std::runtime_error(
            "usage: pcapdecoder --input <hexfile> [--crc-seed <hex>] "
            "[--track-object <id> | --track-all-creo]");
    }
    return opts;
}

} // namespace

int main(int argc, char** argv) {
    try {
        CliOptions opts = parseCommandLine(argc, argv);

        std::ifstream file(opts.inputPath);
        if (!file) {
            std::cerr << "Could not open " << opts.inputPath << "\n";
            return 1;
        }

        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        std::cout << "Loaded " << lines.size() << " packets from " << opts.inputPath << "\n";

        uint32_t crcSeed = opts.crcSeed;
        size_t startIndex = 0;

        if (!opts.haveCrcSeed) {
            // Auto-detect: the first line should be the 17-byte
            // SessionResponse (opcode 0x0200 -> wire bytes 00 02).
            if (lines.empty()) {
                std::cerr << "Input file is empty.\n";
                return 1;
            }
            auto bytes = hexToBytes(lines[0]);
            if (bytes.size() != 17 || bytes[0] != 0x00 || bytes[1] != 0x02) {
                std::cerr << "First packet isn't a 17-byte SessionResponse (0x0200) - pass "
                             "--crc-seed explicitly, or make sure the exported hex file "
                             "starts with the handshake.\n";
                return 1;
            }
            soe::PacketBuffer resp(bytes.data(), bytes.size());
            resp.readUint16();
            resp.readUint32(); // connectionID, unused
            crcSeed = resp.readUint32BE();
            startIndex = 1;
            std::cout << "Auto-detected crcSeed=0x" << std::hex << crcSeed << std::dec << "\n";
        } else {
            std::cout << "Using supplied crcSeed=0x" << std::hex << crcSeed << std::dec << "\n";
        }

        // ---- Dispatcher setup: one handler per message type this tool
        // decodes, plus a tally for everything else. ----
        CaptureStats stats;
        soe::MessageDispatcher dispatcher;

        // Known sub-types share the exact same decode+print registration
        // dummyclient uses (libs/clientcommon, built in Phase 5 step 2) -
        // no prefix, matching this tool's existing plain "Type: ..." output.
        // The onDecoded hook adds this tool's own per-type counting on top,
        // without needing a separate copy of the decode/print logic.
        swgproto::ObjControllerDispatcher objControllerDispatcher;
        clientcommon::registerObjControllerHandlers(
            objControllerDispatcher, std::cout, "",
            [&stats](const swgproto::ObjControllerMessage& envelope) {
                ++stats.countsByType[envelope.header2];
            });
        objControllerDispatcher.onUnknown(
            [&stats](const swgproto::ObjControllerMessage& envelope, soe::PacketBuffer& buf) {
                ++stats.countsByType[envelope.header2];
                auto bytes = buf.readBytes(buf.remaining());
                std::cout << "UNKNOWN header1=0x" << std::hex << envelope.header1
                           << " header2=0x" << envelope.header2 << std::dec
                           << " objectId=" << envelope.objectId << " (" << bytes.size()
                           << " bytes): ";
                clientcommon::printHexBytes(bytes);
                std::cout << "\n";

                auto& samples = stats.unknownSamples[envelope.header2];
                if (samples.size() < CaptureStats::kMaxSamplesPerType) {
                    samples.push_back(bytes);
                }
            });

        dispatcher.on(swgproto::kObjControllerMessageHash,
                       [&objControllerDispatcher](soe::PacketBuffer& buf) {
                           objControllerDispatcher.dispatch(buf);
                       });

        // Always decoded, unlike DeltasMessage below (opt-in, real per-packet
        // cost) - ChatSystemMessage is cheap, low-frequency, and often the
        // most direct evidence of what a captured action actually DID (e.g.
        // a structure privacy toggle's server confirmation text), not just
        // that a request/response pair happened.
        dispatcher.on(swgproto::kChatSystemMessageHash, [](soe::PacketBuffer& buf) {
            auto msg = swgproto::ChatSystemMessage::parse(buf);
            std::cout << "ChatSystemMessage (displayType=" << static_cast<int>(msg.displayType)
                       << "): \"" << toUtf8Preview(msg.message) << "\"\n";
        });

        // The real (worldX, worldZ) -> real ground Y sample this decoder was
        // extended for - terrain::TerrainGenerator::queryHeight()'s
        // real-position cross-check (Phase 4 of the terrain plan).
        dispatcher.on(swgproto::kCmdStartSceneHash, [](soe::PacketBuffer& buf) {
            auto msg = swgproto::CmdStartScene::parse(buf);
            std::cout << "CmdStartScene: terrainName=\"" << msg.terrainName << "\" x=" << msg.x
                       << " y=" << msg.y << " z=" << msg.z
                       << " selfObjectId=" << msg.selfObjectId << "\n";
        });

        if (opts.trackObjectId != 0 || opts.trackAllCreo) {
            dispatcher.on(swgproto::kDeltasMessageHash, [&opts](soe::PacketBuffer& buf) {
                auto env = swgproto::BaselineEnvelope::parse(buf);
                if (!env.ok()) {
                    return;
                }
                const auto& e = env.value();
                if (!opts.trackAllCreo && e.objectId != opts.trackObjectId) {
                    return;
                }
                if (swgproto::objectTypeTag(e.objectType) != "CREO" ||
                    (e.baselineNumber != 1 && e.baselineNumber != 3)) {
                    return;
                }
                auto decoded =
                    swgproto::decodeCreatureObjectDelta(e.baselineNumber, e.count, buf);
                std::cout << "CreatureObject BASE" << static_cast<int>(e.baselineNumber)
                           << " delta (objectId=" << e.objectId << "): ";
                for (const auto& u : decoded.updates) {
                    std::cout << u.fieldName << "=" << u.valueText << " ";
                }
                if (decoded.stoppedEarly) {
                    std::cout << "[stopped: " << decoded.stopReason << "]";
                }
                std::cout << "\n";
            });
        }

        // Every other hash falls through here; tally counts instead of
        // printing each occurrence (still noisy per-line at real traffic
        // volume) - the summary is useful for spotting which message types
        // this tool doesn't decode yet, same purpose as Phase 2 step 1's
        // original hash catalog.
        std::map<uint32_t, size_t> otherHashCounts;
        dispatcher.onUnknown([&otherHashCounts](uint32_t hash, soe::PacketBuffer& buf) {
            ++otherHashCounts[hash];
            auto bytes = buf.readBytes(buf.remaining());
            std::cout << "UNKNOWN top-level hash=0x" << std::hex << hash << std::dec << " ("
                       << bytes.size() << " bytes): ";
            clientcommon::printHexBytes(bytes);
            std::cout << "\n";
        });

        soe::FragmentReassembler fragmentReassembler;
        size_t droppedCrc = 0, droppedMalformed = 0, disconnects = 0, controlPackets = 0;

        for (size_t i = startIndex; i < lines.size(); ++i) {
            auto raw = hexToBytes(lines[i]);
            if (raw.size() < 2) {
                continue;
            }
            soe::PacketBuffer buf(raw.data(), raw.size());

            uint16_t opcode = buf.peekUint16(0);

            if (!soe::PacketCrc::test(buf, crcSeed)) {
                ++droppedCrc;
                continue;
            }

            if (opcode == 0x0500) {
                ++disconnects;
                continue;
            }
            if (opcode != 0x0900 && opcode != 0x0D00 && opcode != 0x0300) {
                ++controlPackets;
                continue;
            }

            try {
                soe::XorCipher::decrypt(buf, crcSeed);

                if (buf.size() < 7) {
                    ++droppedMalformed;
                    continue;
                }

                uint8_t compFlag = buf.peekByte(buf.size() - 3);
                if (compFlag == 0x01) {
                    uint8_t firstByte = buf.peekByte(0);
                    size_t headerLen = (firstByte == 0x00) ? 2 : 1;
                    size_t compressedLen = buf.size() - headerLen - 3;

                    auto inflated =
                        soe::Compression::decompress(buf.data() + headerLen, compressedLen, 8192);

                    soe::PacketBuffer rebuilt(headerLen + inflated.size() + 3);
                    rebuilt.writeBytes(buf.data(), headerLen);
                    rebuilt.writeBytes(inflated.data(), inflated.size());
                    rebuilt.writeBytes(buf.data() + buf.size() - 3, 3);
                    buf = std::move(rebuilt);
                }

                buf.removeLastBytes(3);

                buf.readUint16(); // opcode

                std::vector<std::vector<uint8_t>> messages;

                if (opcode == 0x0300) {
                    extractMultiPacketMessages(buf, fragmentReassembler, messages);
                } else {
                    uint16_t seq = buf.readUint16BE();
                    if (opcode == 0x0D00) {
                        auto fragment = fragmentReassembler.addFragment(seq, buf);
                        if (fragment.status == soe::FragmentStatus::Invalid) {
                            std::cerr << "Dropping fragmented packet (seq=" << seq
                                       << "): " << fragment.error << "\n";
                            continue;
                        }
                        if (fragment.status == soe::FragmentStatus::Incomplete) {
                            continue;
                        }
                        soe::PacketBuffer full(fragment.payload.data(), fragment.payload.size());
                        extractDataChannelMessages(full, messages);
                    } else {
                        extractDataChannelMessages(buf, messages);
                    }
                }

                for (auto& msg : messages) {
                    dispatcher.dispatch(msg);
                }
            } catch (const std::exception& e) {
                ++droppedMalformed;
                std::cerr << "Dropping malformed packet: " << e.what() << "\n";
            }
        }

        std::cout << "\n=== Decode summary ===\n";
        std::cout << "Dropped (bad CRC): " << droppedCrc << "\n";
        std::cout << "Dropped (malformed): " << droppedMalformed << "\n";
        std::cout << "Control packets (Ack/Ping/etc.): " << controlPackets << "\n";
        std::cout << "Disconnect packets: " << disconnects << "\n";

        std::cout << "\n=== Other top-level message hashes (not ObjControllerMessage) ===\n";
        for (const auto& [hash, count] : otherHashCounts) {
            std::cout << "  hash=0x" << std::hex << hash << std::dec << " count=" << count
                       << "\n";
        }

        std::cout << "\n=== ObjControllerMessage summary ===\n";
        size_t total = 0;
        for (const auto& [header2, count] : stats.countsByType) {
            total += count;
            std::cout << "  header2=0x" << std::hex << header2 << std::dec << " count=" << count
                       << "\n";
        }
        std::cout << "Total: " << total << " ObjControllerMessage instances.\n";

        if (!stats.unknownSamples.empty()) {
            std::cout << "\nRaw samples for undecoded sub-types:\n";
            for (const auto& [header2, samples] : stats.unknownSamples) {
                std::cout << "  header2=0x" << std::hex << header2 << std::dec << ":\n";
                for (const auto& bytes : samples) {
                    std::cout << "    (" << bytes.size() << " bytes): ";
                    clientcommon::printHexBytes(bytes);
                    std::cout << "\n";
                }
            }
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
