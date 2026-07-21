#pragma once

#include <array>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ByteDeltaContainer.h"
#include "swgproto/GroupObjectEntry.h"
#include "swgproto/HarvesterHopperEntry.h"
#include "swgproto/Int32DeltaContainer.h"
#include "swgproto/MissionObjectEntry.h"
#include "swgproto/PlayerQuestData.h"
#include "swgproto/SchematicEntry.h"
#include "swgproto/SkillModEntry.h"
#include "swgproto/StringId.h"
#include "swgproto/StringInt32Map.h"
#include "swgproto/Uint64DeltaContainer.h"
#include "swgproto/WaypointEntry.h"
#include "swgproto/WearableEntry.h"

namespace swgproto {

// The wire-shape "kinds" this project's schema engine knows how to read. Each
// one maps to exactly one C++ type (see FieldKindTraits<K>::MemberType below)
// - a schema field ties a kind to a real struct member via pointer-to-member,
// and the compiler checks the two agree (see ObjectSchema.h's `field<>()`).
//
// This set is deliberately not "universal" - it's exactly the shapes proven
// necessary across CreatureObject (BASE1/BASE3) and PlayerObject (BASE3/
// BASE6), Phase 2's first two real object types. Core3 has more container
// encodings than Int32Container/FixedInt32x4/SkillListLike (SkillModList,
// AbilityList, DeltaVectorMap, DeltaBitArray - see PLAN.md step 5's deferred
// list) - add a new FieldKind + FieldKindTraits specialization when a real
// case needs one, don't pre-build for shapes that haven't been read from
// source yet.
enum class FieldKind : uint8_t {
    Int32,
    Uint16,
    Uint64,
    Float,
    Byte,
    Bool,
    Ascii,
    Unicode,
    StringIdField,
    Int32Container,
    FixedInt32x4,
    FixedInt32x3NoTag,
    FixedInt32x2NoTag,
    FixedInt32x4NoTag,
    SkillListLike,
    AbilityListLike,
    ByteContainer,
    StringInt32MapField,
    PlayerQuestDataMapField,
    WaypointListField,
    SchematicListField,
    Uint64Container,
    FixedUint64x2NoTag,
    WearableListField,
    SkillModListField,
    MissionObjectListField,
    GroupMemberListField,
    GroupShipListField,
    HarvesterHopperListField,
};

// Specialized once per FieldKind below. `MemberType` is the C++ type a
// struct member tagged with this kind must have (checked at compile time,
// see ObjectSchema.h). `read()` consumes the field's bytes from `buf` into
// `out`; `format()` renders an already-decoded value for delta's print-only
// path (see SchemaEngine.h).
template <FieldKind K>
struct FieldKindTraits;

template <>
struct FieldKindTraits<FieldKind::Int32> {
    using MemberType = int32_t;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            out = static_cast<int32_t>(buf.readUint32());
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) { return std::to_string(v); }
};

template <>
struct FieldKindTraits<FieldKind::Uint16> {
    using MemberType = uint16_t;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            out = buf.readUint16();
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) { return std::to_string(v); }
};

template <>
struct FieldKindTraits<FieldKind::Uint64> {
    using MemberType = uint64_t;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            out = buf.readUint64();
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) { return std::to_string(v); }
};

template <>
struct FieldKindTraits<FieldKind::Float> {
    using MemberType = float;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            out = buf.readFloat();
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) { return std::to_string(v); }
};

template <>
struct FieldKindTraits<FieldKind::Byte> {
    using MemberType = uint8_t;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            out = buf.readByte();
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) { return std::to_string(static_cast<int>(v)); }
};

template <>
struct FieldKindTraits<FieldKind::Bool> {
    using MemberType = bool;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            out = buf.readByte() != 0;
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) { return v ? "true" : "false"; }
};

template <>
struct FieldKindTraits<FieldKind::Ascii> {
    using MemberType = std::string;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            out = buf.readAscii();
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) { return v; }
};

template <>
struct FieldKindTraits<FieldKind::Unicode> {
    using MemberType = std::u16string;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            out = buf.readUnicode();
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) {
        std::string out;
        out.reserve(v.size());
        for (char16_t ch : v) {
            out.push_back(static_cast<char>(ch));
        }
        return out;
    }
};

template <>
struct FieldKindTraits<FieldKind::StringIdField> {
    using MemberType = StringId;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            out.file = buf.readAscii();
            buf.readUint32(); // unused spacer, see StringId.h
            out.stringId = buf.readAscii();
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) { return v.file + ":" + v.stringId; }
};

template <>
struct FieldKindTraits<FieldKind::Int32Container> {
    using MemberType = std::vector<int32_t>;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parseInt32DeltaContainer(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " items skipped>";
    }
};

template <>
struct FieldKindTraits<FieldKind::FixedInt32x4> {
    using MemberType = std::array<int32_t, 4>;
    // Core3 writes a literal count (always 4) then exactly 4 raw int32s,
    // with NO updateCounter field in between - a genuinely different shape
    // from Int32Container, see PlayerObjectBaseline3.h's field comment.
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            uint32_t tag = buf.readUint32();
            if (tag != 4) {
                error = "expected a literal 4 tag, got " + std::to_string(tag);
                return false;
            }
            for (int i = 0; i < 4; ++i) {
                out[i] = static_cast<int32_t>(buf.readUint32());
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) {
        std::string out = "[";
        for (size_t i = 0; i < v.size(); ++i) {
            out += (i > 0 ? ", " : "") + std::to_string(v[i]);
        }
        return out + "]";
    }
};

template <>
struct FieldKindTraits<FieldKind::FixedInt32x3NoTag> {
    using MemberType = std::array<int32_t, 3>;
    // Unlike FixedInt32x4, Core3 writes NO tag/count/updateCounter at all -
    // just 3 sequential raw int32 inserts (see PlayerObjectBaseline3.h's
    // unknownTail field comment: literal constants 0x6C2/0xDC62/0x23, no
    // delta setter anywhere).
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            for (int i = 0; i < 3; ++i) {
                out[i] = static_cast<int32_t>(buf.readUint32());
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) {
        std::string out = "[";
        for (size_t i = 0; i < v.size(); ++i) {
            out += (i > 0 ? ", " : "") + std::to_string(v[i]);
        }
        return out + "]";
    }
};

template <>
struct FieldKindTraits<FieldKind::FixedInt32x2NoTag> {
    using MemberType = std::array<int32_t, 2>;
    // Same "no tag/count/updateCounter, just N sequential raw int32 inserts"
    // shape as FixedInt32x3NoTag, one element shorter - used for
    // PlayerObjectBaseline8's trailing, non-addressable 2-int tail.
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            for (int i = 0; i < 2; ++i) {
                out[i] = static_cast<int32_t>(buf.readUint32());
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) {
        std::string out = "[";
        for (size_t i = 0; i < v.size(); ++i) {
            out += (i > 0 ? ", " : "") + std::to_string(v[i]);
        }
        return out + "]";
    }
};

template <>
struct FieldKindTraits<FieldKind::FixedInt32x4NoTag> {
    using MemberType = std::array<int32_t, 4>;
    // Same "no tag/count/updateCounter, just N sequential raw int32 inserts"
    // shape as FixedInt32x3NoTag/FixedInt32x2NoTag, used for
    // PlayerObjectBaseline9's opaque, ambiguous-grouping gap (see
    // PlayerObjectBaseline9.h's field comment) - the TOTAL byte count (16)
    // is source-confirmed even though the sub-field boundaries aren't, so
    // this deliberately doesn't guess at where the "real" fields would be.
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            for (int i = 0; i < 4; ++i) {
                out[i] = static_cast<int32_t>(buf.readUint32());
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) {
        std::string out = "[";
        for (size_t i = 0; i < v.size(); ++i) {
            out += (i > 0 ? ", " : "") + std::to_string(v[i]);
        }
        return out + "]";
    }
};

template <>
struct FieldKindTraits<FieldKind::SkillListLike> {
    using MemberType = std::vector<std::string>;
    // Core3's SkillList shape: count + updateCounter header, then ASCII per
    // NON-NULL entry only - `count` covers skipped null entries too, so it
    // can't be trusted as "read exactly this many strings" (see
    // CreatureObjectBaseline1.h's field comment - this was a real bug once).
    // Reads ASCII strings until the buffer is exhausted instead, which is
    // only correct if this field is the LAST one in its schema - see
    // ObjectSchema.h's comment on this invariant.
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            buf.readUint32(); // count - not reliable, discarded
            buf.readUint32(); // updateCounter - discarded
            while (buf.remaining() > 0) {
                out.push_back(buf.readAscii());
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " items>";
    }
};

template <>
struct FieldKindTraits<FieldKind::AbilityListLike> {
    using MemberType = std::vector<std::string>;
    // AbilityList's shape: count + updateCounter header, then ASCII per
    // entry - but UNLIKE SkillList, AbilityList::insertToMessage does NOT
    // null-check entries before writing, so `count` truly matches the
    // number of strings that follow. Reads EXACTLY `count` strings, not
    // "until the buffer is exhausted" - this matters here specifically
    // because abilityList is field index 0, not the last field in
    // PlayerObjectBaseline9's schema, so SkillListLike's read-until-empty
    // trick would be actively wrong (it would consume the rest of the
    // message), not just unnecessary.
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        uint32_t count = 0;
        try {
            count = buf.readUint32();
            buf.readUint32(); // updateCounter - discarded
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
        if (count > buf.remaining() / 2) { // cheap lower bound: 2 bytes min per ASCII string
            error = "count " + std::to_string(count) + " exceeds remaining buffer";
            return false;
        }
        try {
            for (uint32_t i = 0; i < count; ++i) {
                out.push_back(buf.readAscii());
            }
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " items>";
    }
};

template <>
struct FieldKindTraits<FieldKind::ByteContainer> {
    using MemberType = std::vector<uint8_t>;
    // DeltaBitArray's actual shape (see ByteDeltaContainer.h) - the same
    // count+updateCounter+N-items shape as Int32Container, byte-sized items.
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parseByteDeltaContainer(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " bytes>";
    }
};

template <>
struct FieldKindTraits<FieldKind::StringInt32MapField> {
    using MemberType = std::vector<StringInt32Entry>;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parseStringInt32Map(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " entries>";
    }
};

template <>
struct FieldKindTraits<FieldKind::PlayerQuestDataMapField> {
    using MemberType = std::vector<PlayerQuestEntry>;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parsePlayerQuestDataMap(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " entries>";
    }
};

template <>
struct FieldKindTraits<FieldKind::WaypointListField> {
    using MemberType = std::vector<WaypointEntry>;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parseWaypointList(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " waypoints>";
    }
};

template <>
struct FieldKindTraits<FieldKind::SchematicListField> {
    using MemberType = std::vector<SchematicEntry>;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parseSchematicList(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " schematics>";
    }
};

template <>
struct FieldKindTraits<FieldKind::Uint64Container> {
    using MemberType = std::vector<uint64_t>;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parseUint64DeltaContainer(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " items>";
    }
};

template <>
struct FieldKindTraits<FieldKind::FixedUint64x2NoTag> {
    using MemberType = std::array<uint64_t, 2>;
    // Same "no tag/count/updateCounter, just N sequential raw inserts" shape
    // as FixedInt32x2NoTag, but for CreatureObjectBaseline6's
    // groupInviterId/groupInviteCounter pair - confirmed from
    // CreatureObjectDeltaMessage6.h to share a SINGLE delta index (0x07:
    // startUpdate then insertLong twice), so both values are modeled as one
    // struct member updated together, not two separate fields.
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        try {
            out[0] = buf.readUint64();
            out[1] = buf.readUint64();
            return true;
        } catch (const std::exception& e) {
            error = e.what();
            return false;
        }
    }
    static std::string format(const MemberType& v) {
        return "[" + std::to_string(v[0]) + ", " + std::to_string(v[1]) + "]";
    }
};

template <>
struct FieldKindTraits<FieldKind::WearableListField> {
    using MemberType = std::vector<WearableEntry>;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parseWearableList(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " wearables>";
    }
};

template <>
struct FieldKindTraits<FieldKind::SkillModListField> {
    using MemberType = std::vector<SkillModEntry>;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parseSkillModList(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " skill mods>";
    }
};

template <>
struct FieldKindTraits<FieldKind::MissionObjectListField> {
    using MemberType = std::vector<MissionObjectEntry>;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parseMissionObjectList(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " mission objects>";
    }
};

template <>
struct FieldKindTraits<FieldKind::GroupMemberListField> {
    using MemberType = std::vector<GroupMemberEntry>;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parseGroupMemberList(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " members>";
    }
};

template <>
struct FieldKindTraits<FieldKind::GroupShipListField> {
    using MemberType = std::vector<GroupShipEntry>;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parseGroupShipList(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " ships>";
    }
};

template <>
struct FieldKindTraits<FieldKind::HarvesterHopperListField> {
    using MemberType = std::vector<HarvesterHopperEntry>;
    static bool read(MemberType& out, soe::PacketBuffer& buf, std::string& error) {
        auto result = parseHarvesterHopperList(buf);
        if (!result.ok()) {
            error = result.error();
            return false;
        }
        out = result.value();
        return true;
    }
    static std::string format(const MemberType& v) {
        return "<" + std::to_string(v.size()) + " hopper entries>";
    }
};

} // namespace swgproto
