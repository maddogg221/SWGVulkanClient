#include "swgproto/ClientCreateCharacter.h"

#include "soe/MessageHash.h"
#include "soe/PacketBuffer.h"

namespace swgproto {

std::vector<uint8_t> buildClientCreateCharacter(const ClientCreateCharacterParams& params) {
    soe::PacketBuffer buf;
    buf.writeUint16(0x0C); // opCount (12, per Core3's own source)
    buf.writeUint32(soe::MessageHash::compute("ClientCreateCharacter")); // 0xB97F3074

    buf.writeAscii(params.customAppearanceData);
    buf.writeUnicode(params.characterName);
    buf.writeAscii(params.templateName);
    buf.writeAscii("");
    buf.writeAscii(params.hairTemplateName);
    buf.writeAscii(params.hairAppearanceData);
    buf.writeAscii(params.profession);
    buf.writeByte(0x00);
    buf.writeFloat(params.scaleFactor);
    buf.writeUnicode(params.biography);
    buf.writeByte(params.useNewbieTutorial ? 0x01 : 0x00);

    return std::vector<uint8_t>(buf.data(), buf.data() + buf.size());
}

} // namespace swgproto
