#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace soe {

class PacketBuffer;

enum class FragmentStatus {
    Incomplete, // more fragments needed - no payload yet
    Complete,   // reassembly finished - `payload` is the reassembled message
    Invalid,    // this fragment was rejected (bad total size, or out of
                // sequence) - the reassembly was reset; see `error`
};

struct FragmentResult {
    FragmentStatus status = FragmentStatus::Incomplete;
    std::vector<uint8_t> payload; // meaningful only when status == Complete
    std::string error;            // meaningful only when status == Invalid
};

// Reassembles a message split across multiple SOE Fragmented (opcode
// 0x0D00) packets. Mirrors Core3's BaseFragmentedPacket: the first fragment
// fed to a given reassembly carries a 4-byte big-endian total-size prefix
// (the reassembled size - NOT the size of the original unfragmented
// message, which would also include a 4-byte opcode+sequence header every
// fragment omits), every later fragment is pure continuation bytes.
//
// One instance tracks at most one reassembly in progress at a time -
// matches the real protocol (a session never interleaves two fragmented
// messages) and Core3's own BaseClient::fragmentedPacket, which is a single
// member, not a per-message map.
//
// Deliberately reports errors via the returned status (not exceptions):
// a malformed or out-of-order fragment is an EXPECTED failure mode a
// caller must handle every time (not an exceptional program state), and a
// caller driving this from inside a broad try/catch (as
// SoeSession::receiveMessages() does, to isolate one bad packet from the
// rest of the session) could otherwise silently swallow it with no log
// trail - see DISCOVERY.txt's "PHASE 2 STEP 2" follow-up for the incident
// that prompted this.
class FragmentReassembler {
public:
    // Feeds one fragment's payload - `buf` positioned right after that
    // fragment's own 4-byte opcode+sequence header - into the in-progress
    // reassembly. `sequence` is that same fragment's SOE sequence number
    // (the one SoeSession also acks) - used only to detect fragments
    // arriving out of order (see below), not part of the reassembled
    // payload itself.
    //
    // Out-of-order protection: this class does NOT buffer/reorder/request
    // resends the way a full SOE reliability layer would (that's a bigger,
    // separate piece of work - SoeSession doesn't do general packet
    // reordering for any opcode yet, fragmented or not). What it DOES do is
    // require every fragment after the first to carry the immediately-next
    // sequence number, and reset (Invalid) if that's violated - turning
    // "silently reassemble corrupted/misordered bytes and hand them to a
    // parser" into "detect and drop," which is the concrete risk this
    // guards against.
    FragmentResult addFragment(uint16_t sequence, PacketBuffer& buf);

private:
    std::vector<uint8_t> buffer_;
    uint32_t totalSize_ = 0;
    uint16_t nextExpectedSequence_ = 0;
};

} // namespace soe
