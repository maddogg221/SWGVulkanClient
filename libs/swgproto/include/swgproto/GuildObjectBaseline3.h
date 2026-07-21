#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"
#include "swgproto/StringId.h"

namespace swgproto {

// GuildObject's BASE3 baseline (tag "GILD", 0x47494C44). Standalone, no
// ancestor - extends BaseLineMessage directly, same as CellObject.
//
// NOT a per-guild object, despite the name: GuildObjectImplementation::
// sendBaselinesTo() is a dead no-op ({}), confirmed directly from source.
// All real GILD traffic comes from GuildManagerImplementation, a zone-wide
// singleton service - its own sendBaselinesTo(CreatureObject* player) is
// called for every player at login/scene-reset
// (CreatureObjectImplementation.cpp), unconditionally, regardless of guild
// membership, using the MANAGER's own object ID (not any individual
// guild's OID). What this decodes is a single, server-wide directory of
// every guild that exists, not "your guild's info" - confirmed live: the
// objectId seen (4017234474) is far smaller than any normal per-instance
// SceneObject ID, consistent with a manager/service object rather than a
// world object.
//
// Field order confirmed directly from GuildObjectMessage3.h's insert*()
// sequence. `objectName`/`name`/`unknownField` are always literal/empty
// constants in every real capture (complexity=1.0, objectName={file=
// "String_id_table", key=""}, name=""/unicode, unknownField=0) - matches
// source's own hardcoded inserts exactly. `guildList` is Core3's
// DeltaSet<String, ManagedReference<GuildObject*>>::insertToMessage()
// shape: int32 count + int32 updateCounter + N ascii strings, NO per-item
// tag - each string is "<guildID>:<guildAbbrev>" (e.g. "1001866848:Feds").
// This EXACTLY matches the already-existing AbilityListLike FieldKind's
// shape (count+updateCounter header, then exactly `count` ascii strings,
// no null-skip quirk, no tag) - reused unchanged, zero new container code
// needed. Verified live against a real 1697-byte capture: 101 real guild
// entries, zero leftover bytes.
//
// `guildList` (delta index 0x04, confirmed from GuildObjectDeltaMessage3.h)
// IS delta-addressable in source, but via a nested ADD/REMOVE/REMOVE_ALL
// list-command shape (startList + a command byte + one string) -
// materially more complex than a simple field replace, and structurally
// the same class of situation as CreatureObjectBaseline4's skillMods -
// deferred as fieldBaselineOnly rather than guessed at. No other field has
// any delta setter in source at all.
struct GuildObjectBaseline3 {
    float complexity = 0.0f;         // always 1.0 in every real capture
    StringId objectName;              // always {file="String_id_table", key=""}
    std::u16string name;               // always empty
    int32_t unknownField = 0;          // always 0
    std::vector<std::string> guildList; // "<guildID>:<guildAbbrev>" per entry

    static ParseResult<GuildObjectBaseline3> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kGuildObjectBaseline3Fields[] = {
    fieldBaselineOnly<FieldKind::Float, &GuildObjectBaseline3::complexity>("complexity"),
    fieldBaselineOnly<FieldKind::StringIdField, &GuildObjectBaseline3::objectName>("objectName"),
    fieldBaselineOnly<FieldKind::Unicode, &GuildObjectBaseline3::name>("name"),
    fieldBaselineOnly<FieldKind::Int32, &GuildObjectBaseline3::unknownField>("unknownField"),
    fieldBaselineOnly<FieldKind::AbilityListLike, &GuildObjectBaseline3::guildList>("guildList"),
};

inline constexpr ObjectSchema kGuildObjectBaseline3Schema{nullptr, 0, kGuildObjectBaseline3Fields};

} // namespace swgproto
