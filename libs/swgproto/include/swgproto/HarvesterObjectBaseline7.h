#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/HarvesterHopperEntry.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// HarvesterObject's BASE7 baseline (tag "HINO", 0x48494E4F). Standalone, no
// ancestor - HarvesterObjectMessage7 extends BaseLineMessage directly (does
// NOT compose InstallationObjectMessage7). HarvesterObject's BASE3/BASE6
// traffic reuses InstallationObject's own baselines unchanged (tag "INSO" -
// HarvesterObjectMessage3/6 are confirmed dead code, zero references outside
// their own files) - see worldmodel::InstallationObject's `base7` slot,
// which this composes onto the SAME stored object rather than a separate
// HarvesterObject concrete type, avoiding a real type-collision the two
// tags would otherwise cause in ObjectStore.
//
// Field order confirmed directly from HarvesterObjectMessage7.h's insert*()
// sequence, and now confirmed byte-for-byte against a real live capture
// (166/166 bytes, zero leftover) via a placed, live-verified moisture
// vaporator (objectId 562949970715735, Naboo). The capture is INACTIVE-STATE
// only (isActive=false, every rate/hopper-size field genuinely 0) -
// activating a harvester for real requires a working power grid (a Power
// Generator structure + wiring), a separate structure mechanic this project
// hasn't touched; see PLAN.md's Phase 9 writeup. Every field here is still
// modeled faithfully from source AND confirmed-present-on-the-wire (the
// inactive capture exercises every field's real byte position, just with
// zero-valued dynamic fields) - nothing here is guessed from source alone.
//
// `resourceIdList1`/`resourceIdList2` are written TWICE, back-to-back,
// byte-identical shape both times (Core3 itself calls insertResourceIDList()
// twice in its constructor) - kept as two separate fields matching the real
// wire order, not assumed to be a transcription mistake to collapse into
// one (confirmed live: both copies genuinely present and identical).
// `hopperLeadByte` (hopperList.size()==0 ? 0 : size+1) is confirmed live
// too: a real 1-entry hopper produced lead byte 2, exactly matching size+1.
//
// DELTA: deliberately NOT implemented here. No HarvesterObjectDeltaMessage7
// class exists in source at all - real BASE7 deltas are sent via
// InstallationObjectDeltaMessage7 (tag "INSO", NOT "HINO" - a real,
// source-confirmed tag mismatch between this baseline and its own delta,
// see KNOWN_UNKNOWNS.md), whose live indices (0x05/0x06/0x09/0x0A/0x0C/0x0D)
// are documented in DISCOVERY.txt's Tier 4 research but have never been
// observed in real traffic (activation never succeeded this session) - per
// this project's standing rule, decoders only get implemented against
// confirmed real traffic, not source alone. Flagged as a follow-up once a
// power grid can be set up to produce real delta traffic to verify against.
struct HarvesterObjectBaseline7 {
    uint8_t unknownFlag = 0; // always 1 on the wire, no getter behind it in source - purpose unknown
    std::vector<uint64_t> resourceIdList1;
    std::vector<uint64_t> resourceIdList2; // byte-identical duplicate of resourceIdList1, see above
    std::vector<std::string> resourceNameList;
    std::vector<std::string> resourceTypeList;
    uint64_t activeResourceSpawnId = 0;
    bool isActive = false;
    int32_t extractionRateDisplayed = 0; // "Extraction Rate Displayed"
    float extractionRateMax = 0.0f;      // "Extract Rate Max"
    float actualExtractRate = 0.0f;      // "Current Extract Rate"
    float hopperSize = 0.0f;             // current total hopper quantity
    int32_t hopperSizeMax = 0;           // hopper capacity
    uint8_t hopperLeadByte = 0;          // hopperList.size()==0 ? 0 : size+1, unexplained, kept faithfully
    std::vector<HarvesterHopperEntry> hopperList;
    uint8_t condition = 0; // condition percentage, hardcoded 100 in source

    static ParseResult<HarvesterObjectBaseline7> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kHarvesterObjectBaseline7Fields[] = {
    fieldBaselineOnly<FieldKind::Byte, &HarvesterObjectBaseline7::unknownFlag>("unknownFlag"),
    fieldBaselineOnly<FieldKind::Uint64Container, &HarvesterObjectBaseline7::resourceIdList1>(
        "resourceIdList1"),
    fieldBaselineOnly<FieldKind::Uint64Container, &HarvesterObjectBaseline7::resourceIdList2>(
        "resourceIdList2"),
    fieldBaselineOnly<FieldKind::AbilityListLike, &HarvesterObjectBaseline7::resourceNameList>(
        "resourceNameList"),
    fieldBaselineOnly<FieldKind::AbilityListLike, &HarvesterObjectBaseline7::resourceTypeList>(
        "resourceTypeList"),
    fieldBaselineOnly<FieldKind::Uint64, &HarvesterObjectBaseline7::activeResourceSpawnId>(
        "activeResourceSpawnId"),
    fieldBaselineOnly<FieldKind::Bool, &HarvesterObjectBaseline7::isActive>("isActive"),
    fieldBaselineOnly<FieldKind::Int32, &HarvesterObjectBaseline7::extractionRateDisplayed>(
        "extractionRateDisplayed"),
    fieldBaselineOnly<FieldKind::Float, &HarvesterObjectBaseline7::extractionRateMax>(
        "extractionRateMax"),
    fieldBaselineOnly<FieldKind::Float, &HarvesterObjectBaseline7::actualExtractRate>(
        "actualExtractRate"),
    fieldBaselineOnly<FieldKind::Float, &HarvesterObjectBaseline7::hopperSize>("hopperSize"),
    fieldBaselineOnly<FieldKind::Int32, &HarvesterObjectBaseline7::hopperSizeMax>("hopperSizeMax"),
    fieldBaselineOnly<FieldKind::Byte, &HarvesterObjectBaseline7::hopperLeadByte>("hopperLeadByte"),
    fieldBaselineOnly<FieldKind::HarvesterHopperListField, &HarvesterObjectBaseline7::hopperList>(
        "hopperList"),
    fieldBaselineOnly<FieldKind::Byte, &HarvesterObjectBaseline7::condition>("condition"),
};

inline constexpr ObjectSchema kHarvesterObjectBaseline7Schema{nullptr, 0,
                                                                kHarvesterObjectBaseline7Fields};

} // namespace swgproto
