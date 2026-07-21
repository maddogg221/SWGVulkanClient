#include "swgproto/InstallationObjectBaseline3.h"

#include "swgproto/SchemaEngine.h"

namespace swgproto {

ParseResult<InstallationObjectBaseline3> InstallationObjectBaseline3::parse(soe::PacketBuffer& buf) {
    return decodeBaseline<InstallationObjectBaseline3>(kInstallationObjectBaseline3Schema, buf);
}

} // namespace swgproto
