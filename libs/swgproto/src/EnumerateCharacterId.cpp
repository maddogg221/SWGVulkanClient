#include "swgproto/EnumerateCharacterId.h"

#include <stdexcept>

namespace swgproto {

namespace {
// Unicode name (4-byte count, 0 chars minimum) + race(4) + objectId(8) +
// galaxyId(4) + characterType(4) - the smallest a real entry could ever be.
constexpr size_t kMinCharacterEntrySize = 24;
} // namespace

EnumerateCharacterId EnumerateCharacterId::parse(soe::PacketBuffer& buf) {
    EnumerateCharacterId result;

    uint32_t charCount = buf.readUint32();
    if (charCount > buf.remaining() / kMinCharacterEntrySize) {
        throw std::runtime_error("EnumerateCharacterId: characterCount exceeds available data");
    }
    result.characters.reserve(charCount);

    for (uint32_t i = 0; i < charCount; ++i) {
        CharacterEntry entry;
        entry.name = buf.readUnicode();
        entry.race = buf.readUint32();
        entry.objectId = buf.readUint64();
        entry.galaxyId = buf.readUint32();
        entry.characterType = buf.readUint32();
        result.characters.push_back(std::move(entry));
    }

    return result;
}

} // namespace swgproto
