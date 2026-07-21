#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// Parses the "count + updateCounter + N raw bytes" shape used by
// DeltaBitArray (a thin subclass of DeltaVector<byte> that adds no
// insertToMessage override of its own, per Core3's DeltaBitArray.h - it
// uses the generic DeltaVector<E>::insertToMessage with E=byte). Byte-for-
// byte identical structure to Int32DeltaContainer.h's
// parseInt32DeltaContainer, just byte-sized items instead of int32 items -
// kept as a separate function/type rather than a template parameterized on
// item width, matching this project's "one small file per proven shape"
// convention (see Int32DeltaContainer.h's own header comment on why this
// isn't meant to be a universal container parser).
ParseResult<std::vector<uint8_t>> parseByteDeltaContainer(soe::PacketBuffer& buf);

} // namespace swgproto
