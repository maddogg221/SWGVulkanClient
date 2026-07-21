#include "soe/FragmentReassembler.h"

#include <algorithm>
#include <exception>

#include "soe/PacketBuffer.h"

namespace soe {

namespace {
// Matches Core3's own BaseFragmentedPacket::MAX_COMPLETE_FRAG_SIZE -
// defends against a corrupt/malicious total-size field triggering an
// unbounded allocation.
constexpr uint32_t kMaxFragmentedSize = 1500000;
} // namespace

FragmentResult FragmentReassembler::addFragment(uint16_t sequence, PacketBuffer& buf) {
    FragmentResult result;

    if (totalSize_ == 0) {
        // First fragment of a new reassembly - its sequence number becomes
        // the baseline every later fragment is checked against, and its
        // payload starts with the 4-byte total-size field.
        uint32_t totalSize = 0;
        try {
            totalSize = buf.readUint32BE();
        } catch (const std::exception&) {
            result.status = FragmentStatus::Invalid;
            result.error = "first fragment too short to hold its total-size field";
            return result;
        }

        if (totalSize == 0 || totalSize > kMaxFragmentedSize) {
            result.status = FragmentStatus::Invalid;
            result.error =
                "total size " + std::to_string(totalSize) + " out of range (max " +
                std::to_string(kMaxFragmentedSize) + ")";
            return result;
        }

        totalSize_ = totalSize;
        buffer_.reserve(totalSize_);
        nextExpectedSequence_ = static_cast<uint16_t>(sequence + 1);
    } else if (sequence != nextExpectedSequence_) {
        result.status = FragmentStatus::Invalid;
        result.error = "out-of-order fragment: expected sequence " +
            std::to_string(nextExpectedSequence_) + ", got " + std::to_string(sequence);
        buffer_.clear();
        totalSize_ = 0;
        return result;
    } else {
        nextExpectedSequence_ = static_cast<uint16_t>(sequence + 1);
    }

    size_t needed = totalSize_ - buffer_.size();
    size_t take = std::min(needed, buf.remaining());
    auto chunk = buf.readBytes(take);
    buffer_.insert(buffer_.end(), chunk.begin(), chunk.end());

    if (buffer_.size() < totalSize_) {
        result.status = FragmentStatus::Incomplete;
        return result;
    }

    result.status = FragmentStatus::Complete;
    result.payload = std::move(buffer_);
    buffer_.clear();
    totalSize_ = 0;
    return result;
}

} // namespace soe
