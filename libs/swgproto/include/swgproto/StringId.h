#pragma once

#include <string>

namespace swgproto {

// Core3's StringId: a "file + string table key" pair used for
// server-catalog object names (e.g. "object/creature/player/human_male" +
// "human_male") - see BaseLineMessage::insertStringId, which inserts an
// unused int32(0) spacer between the two ASCII strings. Shared by both
// TangibleObject and IntangibleObject baselines (both use the same
// insertStringId helper for their objectName field), hence its own header
// rather than living inside either baseline's file.
struct StringId {
    std::string file;
    std::string stringId;
};

} // namespace swgproto
