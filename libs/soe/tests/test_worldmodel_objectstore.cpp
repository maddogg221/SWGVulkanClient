// Permanent regression test for worldmodel::ObjectStore - the pilot
// migration (TangibleObject + ResourceContainer) proving the Object Model
// design end-to-end: real baseline/delta traffic dispatched through
// swgproto::ObjectStateDispatcher lands in a single, persistent, per-
// objectId record instead of being parsed-and-discarded. Reuses REAL
// captured byte fixtures already pinned elsewhere in this test suite
// (test_swgproto_weaponobject.cpp's TANO BASE3 payload,
// test_swgproto_resourcecontainer.cpp's RCNO BASE3/6 payloads) rather than
// re-capturing anything - this test's job is proving the STORE, not
// re-proving those parsers' own correctness (already covered).
#include <doctest/doctest.h>

#include <sstream>

#include "soe/PacketBuffer.h"
#include "swgproto/DataTransform.h"
#include "swgproto/DataTransformWithParent.h"
#include "swgproto/ObjectStateDispatcher.h"
#include "swgproto/UpdateTransformMessage.h"
#include "swgproto/UpdateTransformWithParentMessage.h"
#include "worldmodel/ObjectStore.h"

using soe::PacketBuffer;
using swgproto::ObjectStateDispatcher;
using worldmodel::GroupObject;
using worldmodel::InstallationObject;
using worldmodel::ObjectStore;
using worldmodel::ResourceContainer;
using worldmodel::TangibleObject;

namespace {

uint32_t fourCC(const std::string& tag) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(tag[0])) << 24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(tag[1])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(tag[2])) << 8) |
           static_cast<uint32_t>(static_cast<uint8_t>(tag[3]));
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    std::istringstream iss(hex);
    std::string token;
    while (iss >> token) {
        bytes.push_back(static_cast<uint8_t>(std::stoul(token, nullptr, 16)));
    }
    return bytes;
}

// For plain top-level message payloads (e.g. UpdateTransformMessage) that
// aren't handed through buildEnvelopeWithPayload()'s baseline/delta envelope
// wrapping - mirrors test_swgproto_transform.cpp's own helper of the same
// name.
PacketBuffer bufferFromHex(const std::string& hex) {
    auto bytes = hexToBytes(hex);
    return PacketBuffer(bytes.data(), bytes.size());
}

// Builds a full envelope-header-plus-payload buffer, ready to hand to
// dispatchBaseline()/dispatchDelta() - mirrors test_objectstatedispatcher.cpp's
// buildEnvelope() helper, extended to append a real per-type payload after
// the header instead of a plain ASCII probe string.
PacketBuffer buildEnvelopeWithPayload(uint64_t objectId, const std::string& objectType,
                                       uint8_t baselineNumber, uint16_t count,
                                       const std::string& payloadHex) {
    PacketBuffer buf;
    buf.writeUint64(objectId);
    buf.writeUint32(fourCC(objectType));
    buf.writeByte(baselineNumber);
    buf.writeUint32(2); // size - unused by BaselineEnvelope::parse's caller here
    buf.writeUint16(count);
    auto payload = hexToBytes(payloadHex);
    buf.writeBytes(payload.data(), payload.size());
    return buf;
}

// Real captured WeaponObject/TangibleObject BASE3 fixture, 135 bytes - see
// test_swgproto_weaponobject.cpp for the full citation.
const std::string kRealTangibleBase3Hex =
    "00 00 0c 42 0b 00 77 65 61 70 6f 6e 5f 6e 61 6d 65 00 00 00 00 0d 00 76 69 62 72 6f "
    "6b 6e 75 63 6b 6c 65 72 20 00 00 00 56 00 69 00 62 00 72 00 6f 00 20 00 4b 00 6e 00 "
    "75 00 63 00 6b 00 6c 00 65 00 72 00 20 00 7c 00 7c 00 20 00 50 00 75 00 72 00 65 00 "
    "20 00 50 00 6f 00 72 00 6b 00 20 00 41 00 72 00 6d 00 73 00 01 00 00 00 00 00 00 00 "
    "00 00 00 00 00 00 00 21 00 00 00 00 00 00 1e 00 00 00 56 04 00 00 01";

// Real captured ResourceContainer BASE3/6 fixtures (500 Klebe) - see
// test_swgproto_resourcecontainer.cpp for the full citation.
const std::string kRealResourceContainerBase3Hex =
    "00 00 c8 42 14 00 72 65 73 6f 75 72 63 65 5f 63 6f 6e 74 61 69 6e 65 72 5f 6e 00 00 "
    "00 00 12 00 6f 72 67 61 6e 69 63 5f 66 6f 6f 64 5f 73 6d 61 6c 6c 12 00 00 00 44 00 "
    "6f 00 6d 00 65 00 73 00 74 00 69 00 63 00 61 00 74 00 65 00 64 00 20 00 57 00 68 00 "
    "65 00 61 00 74 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 f4 01 00 00 "
    "00 00 00 00 e8 03 00 00 01 f4 01 00 00 2b 3f 00 01 00 00 14 00";
const std::string kRealResourceContainerBase6Hex =
    "00 00 00 00 00 00 00 00 00 00 00 00 a0 86 01 00 18 00 77 68 65 61 74 5f 64 6f 6d 65 "
    "73 74 69 63 61 74 65 64 5f 6e 61 62 6f 6f 05 00 00 00 4b 00 6c 00 65 00 62 00 65 00";

// Real captured GroupObject (tag "GRUP") fixtures, objectId=17287767, a real
// 2-member group formed via /invite+/join between two live test accounts -
// see DISCOVERY.txt's "GroupObject BASE3/6" entry. All three payloads decode
// with zero leftover bytes (39/39, 106/106, 122/122).
const std::string kRealGroupBase3Hex =
    "00 00 80 3f 0f 00 73 74 72 69 6e 67 5f 69 64 5f 74 61 62 6c 65 00 00 00 00 00 00 00 "
    "00 00 00 00 00 00 00 00 00 80 3f";
const std::string kRealGroupBase6Hex =
    "00 00 00 00 02 00 00 00 02 00 00 00 28 de 06 01 00 00 01 00 0c 00 53 6e 69 6b 61 20 "
    "43 68 6f 6e 65 69 03 c9 07 01 00 00 01 00 0a 00 4b 69 68 69 65 20 4e 6f 6c 61 02 00 "
    "00 00 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 "
    "00 00 00 00 01 00 00 00 00 00 28 de 06 01 00 00 01 00 00 00 00 00";
const std::string kRealGroupDelta6Hex =
    "01 00 02 00 00 00 02 00 00 00 01 00 00 28 de 06 01 00 00 01 00 0c 00 53 6e 69 6b 61 "
    "20 43 68 6f 6e 65 69 01 01 00 03 c9 07 01 00 00 01 00 0a 00 4b 69 68 69 65 20 4e 6f "
    "6c 61 02 00 02 00 00 00 02 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 "
    "01 00 00 00 00 00 00 00 00 00 01 00 00 00 03 00 00 00 04 00 01 00 06 00 28 de 06 01 "
    "00 00 01 00 07 00 00 00 00 00";

// Real captured InstallationObject BASE3/6 + HarvesterObject BASE7
// fixtures, objectId=562949970715735 - a moisture vaporator placed on
// Naboo, an INACTIVE-STATE capture (see test_swgproto_harvesterobject.cpp's
// file header for why). BASE3/6 arrive under tag "INSO" (generic
// InstallationObject baselines, HarvesterObjectMessage3/6 are confirmed
// dead code); BASE7 arrives under a DIFFERENT tag "HINO" for the SAME
// objectId - this fixture set exists specifically to prove that real tag
// mismatch composes onto one stored InstallationObject rather than
// colliding (see InstallationObject.h's `base7` comment).
const std::string kRealInstallationBase3Hex =
    "00 00 c8 42 0e 00 69 6e 73 74 61 6c 6c 61 74 69 6f 6e 5f 6e 00 00 00 00 0d 00 6d 6f 69 73 "
    "74 75 72 65 5f 6d 69 6e 65 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 "
    "00 00 00 00 00 00 00 00 00 e8 03 00 00 01 01 00 00 00 00 00 00 c8 41";
const std::string kRealInstallationBase6Hex = "76 00 00 00 00 00 00 00 01 00 00 00";
const std::string kRealHarvesterBase7Hex =
    "01 02 00 00 00 02 00 00 00 2a 3f 00 01 00 00 14 00 06 3e 00 01 00 00 14 00 02 00 00 00 "
    "02 00 00 00 2a 3f 00 01 00 00 14 00 06 3e 00 01 00 00 14 00 02 00 00 00 02 00 00 00 05 "
    "00 45 6a 6f 67 61 03 00 4f 63 69 02 00 00 00 02 00 00 00 11 00 77 61 74 65 72 5f 76 61 "
    "70 6f 72 5f 6e 61 62 6f 6f 11 00 77 61 74 65 72 5f 76 61 70 6f 72 5f 6e 61 62 6f 6f 2a "
    "3f 00 01 00 00 14 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 02 "
    "01 00 00 00 02 00 00 00 2a 3f 00 01 00 00 14 00 00 00 00 00 64";

// Real captured ACTIVE-state BASE7 baseline + a real delta, same objectId -
// obtained via a temporary server-side fix for the admin-generated-deed
// hopperSizeMax/extractionRate=0 gap (see test_swgproto_harvesterobject.cpp's
// file header). Proves the delta composes onto the SAME InstallationObject
// via ObjectStore's real dispatch path, not just the standalone
// swgproto-level applyHarvesterObjectBaseline7Delta already covered in
// test_swgproto_harvesterobject.cpp.
const std::string kRealHarvesterBase7ActiveHex =
    "01 02 00 00 00 02 00 00 00 2a 3f 00 01 00 00 14 00 06 3e 00 01 00 00 14 00 02 00 00 00 "
    "02 00 00 00 2a 3f 00 01 00 00 14 00 06 3e 00 01 00 00 14 00 02 00 00 00 02 00 00 00 05 "
    "00 45 6a 6f 67 61 03 00 4f 63 69 02 00 00 00 02 00 00 00 11 00 77 61 74 65 72 5f 76 61 "
    "70 6f 72 5f 6e 61 62 6f 6f 11 00 77 61 74 65 72 5f 76 61 70 6f 72 5f 6e 61 62 6f 6f 2a "
    "3f 00 01 00 00 14 00 01 0a 00 00 00 00 00 20 41 6e bd eb 3f 00 00 00 40 10 27 00 00 02 "
    "01 00 00 00 0f 00 00 00 2a 3f 00 01 00 00 14 00 00 00 00 40 64";
const std::string kRealHarvesterDelta7Hex =
    "0c 00 01 0d 00 01 00 00 00 11 00 00 00 02 00 00 2a 3f 00 01 00 00 "
    "14 00 00 00 a0 40 0a 00 00 00 a0 40 09 00 6e bd eb 3f";

} // namespace

TEST_CASE("ObjectStore: real TANO BASE3 baseline populates a stored TangibleObject") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    auto buf = buildEnvelopeWithPayload(281474976710656ULL, "TANO", 3, 11, kRealTangibleBase3Hex);
    dispatcher.dispatchBaseline(buf);

    REQUIRE(store.size() == 1);
    const auto* obj = store.findAs<TangibleObject>(281474976710656ULL);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->base3.has_value());
    CHECK(obj->base3->complexity == doctest::Approx(35.0f));
    CHECK(obj->base3->customObjectName == u"Vibro Knuckler || Pure Pork Arms");
    CHECK(obj->objectId == 281474976710656ULL);
    CHECK(obj->baselineMessagesSeen == 1);
    CHECK_FALSE(obj->base6.has_value()); // BASE6 never arrived - independent slot
}

TEST_CASE("ObjectStore: ResourceContainer's composed BASE3 and standalone BASE6 slots populate "
          "independently on the same stored object") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    uint64_t objectId = 281474993941082ULL;
    auto buf3 =
        buildEnvelopeWithPayload(objectId, "RCNO", 3, 15, kRealResourceContainerBase3Hex);
    dispatcher.dispatchBaseline(buf3);
    auto buf6 =
        buildEnvelopeWithPayload(objectId, "RCNO", 6, 5, kRealResourceContainerBase6Hex);
    dispatcher.dispatchBaseline(buf6);

    REQUIRE(store.size() == 1); // same objectId, one stored entry, not two
    const auto* obj = store.findAs<ResourceContainer>(objectId);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->base3.has_value());
    REQUIRE(obj->base6.has_value());
    CHECK(obj->base3->quantity == 500);
    CHECK(obj->base3->tangible.customObjectName == u"Domesticated Wheat");
    CHECK(obj->base6->resourceName == u"Klebe");
    CHECK(obj->baselineMessagesSeen == 2);
}

TEST_CASE("ObjectStore: a delta applies onto an already-stored object's field in place") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    uint64_t objectId = 12345ULL;
    auto baselineBuf = buildEnvelopeWithPayload(objectId, "TANO", 3, 11, kRealTangibleBase3Hex);
    dispatcher.dispatchBaseline(baselineBuf);

    // conditionDamage, index 0x08 (TangibleObjectBaseline3.h) - a real,
    // live-confirmed delta field. Synthetic value/bytes, not a real capture -
    // the point here is proving apply-onto-store works, not re-proving the
    // field index (already pinned by test_schema_engine.cpp's applyDeltaMessage
    // tests).
    PacketBuffer deltaBuf;
    deltaBuf.writeUint64(objectId);
    deltaBuf.writeUint32(fourCC("TANO"));
    deltaBuf.writeByte(3);
    deltaBuf.writeUint32(2);
    deltaBuf.writeUint16(1); // count
    deltaBuf.writeUint16(0x08);
    deltaBuf.writeUint32(999);
    dispatcher.dispatchDelta(deltaBuf);

    const auto* obj = store.findAs<TangibleObject>(objectId);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->base3.has_value());
    CHECK(obj->base3->conditionDamage == 999);
    // Untouched fields prove this was a targeted write, not a re-decode.
    CHECK(obj->base3->complexity == doctest::Approx(35.0f));
    CHECK(obj->deltaMessagesSeen == 1);
}

TEST_CASE("ObjectStore: a delta for an objectId with no prior baseline is dropped, not crashed") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    PacketBuffer deltaBuf;
    deltaBuf.writeUint64(999999ULL); // never seen via any baseline
    deltaBuf.writeUint32(fourCC("TANO"));
    deltaBuf.writeByte(3);
    deltaBuf.writeUint32(2);
    deltaBuf.writeUint16(1);
    deltaBuf.writeUint16(0x08);
    deltaBuf.writeUint32(1);

    CHECK_NOTHROW(dispatcher.dispatchDelta(deltaBuf));
    CHECK(store.size() == 0);
    CHECK(store.find(999999ULL) == nullptr);
}

TEST_CASE("ObjectStore: real GRUP BASE3/BASE6 baselines populate a stored GroupObject") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    uint64_t objectId = 17287767ULL;
    auto buf3 = buildEnvelopeWithPayload(objectId, "GRUP", 3, 5, kRealGroupBase3Hex);
    dispatcher.dispatchBaseline(buf3);
    auto buf6 = buildEnvelopeWithPayload(objectId, "GRUP", 6, 8, kRealGroupBase6Hex);
    dispatcher.dispatchBaseline(buf6);

    REQUIRE(store.size() == 1);
    const auto* obj = store.findAs<GroupObject>(objectId);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->base3.has_value());
    CHECK(obj->base3->unknownComplexity1 == doctest::Approx(1.0f));
    CHECK(obj->base3->stringIdTable == "string_id_table");

    REQUIRE(obj->base6.has_value());
    REQUIRE(obj->base6->members.size() == 2);
    CHECK(obj->base6->members[0].objectId == 281474993937960ULL);
    CHECK(obj->base6->members[0].name == "Snika Chonei");
    CHECK(obj->base6->members[1].objectId == 281474993998083ULL);
    CHECK(obj->base6->members[1].name == "Kihie Nola");
    REQUIRE(obj->base6->ships.size() == 2); // count/counter reused from members - a real quirk
    CHECK(obj->base6->groupLevel == 1);
    CHECK(obj->base6->masterLooterId == 281474993937960ULL);
    CHECK(obj->base6->lootRule == 0);
    CHECK(obj->baselineMessagesSeen == 2);
}

TEST_CASE("ObjectStore: a real GRUP delta mutates the stored member/ship lists and bundled "
          "scalar fields in place") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    uint64_t objectId = 17287767ULL;
    auto buf6 = buildEnvelopeWithPayload(objectId, "GRUP", 6, 8, kRealGroupBase6Hex);
    dispatcher.dispatchBaseline(buf6);

    // The real delta's own declared count (6) matches exactly 6 top-level
    // field-index blocks (0x01, 0x02, 0x03, 0x04, 0x06, 0x07) - each 0x01/
    // 0x02 block itself bundles 2 nested per-entry ops.
    auto deltaBuf = buildEnvelopeWithPayload(objectId, "GRUP", 6, 6, kRealGroupDelta6Hex);
    dispatcher.dispatchDelta(deltaBuf);

    const auto* obj = store.findAs<GroupObject>(objectId);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->base6.has_value());
    REQUIRE(obj->base6->members.size() == 2);
    CHECK(obj->base6->members[0].objectId == 281474993937960ULL);
    CHECK(obj->base6->members[0].name == "Snika Chonei");
    CHECK(obj->base6->members[1].objectId == 281474993998083ULL);
    CHECK(obj->base6->members[1].name == "Kihie Nola");
    CHECK(obj->base6->groupName.empty());
    CHECK(obj->base6->groupLevel == 1);
    CHECK(obj->base6->masterLooterId == 281474993937960ULL);
    CHECK(obj->base6->lootRule == 0);
    CHECK(obj->deltaMessagesSeen == 1);
    CHECK(obj->deltaFieldsSkipped == 0); // all 6 field indices matched, nothing stopped early
}

TEST_CASE("ObjectStore: a GRUP member-list ADD op grows the stored vector at an explicit index") {
    // Synthetic (not a real capture) - proves the incremental-mutation
    // contract itself (adding a THIRD member to an already-2-member group),
    // which the real fixture above can't exercise since it only ever
    // observed a 2-member initialUpdate(). Real ADD/UPDATE(tag=1) shape is
    // otherwise identical to what's already traffic-confirmed above.
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    uint64_t objectId = 17287767ULL;
    auto buf6 = buildEnvelopeWithPayload(objectId, "GRUP", 6, 8, kRealGroupBase6Hex);
    dispatcher.dispatchBaseline(buf6);

    PacketBuffer deltaBuf;
    deltaBuf.writeUint64(objectId);
    deltaBuf.writeUint32(fourCC("GRUP"));
    deltaBuf.writeByte(6);
    deltaBuf.writeUint32(2);
    deltaBuf.writeUint16(1); // count - one field-index block
    deltaBuf.writeUint16(0x01); // members
    deltaBuf.writeUint32(1);    // opCount
    deltaBuf.writeUint32(3);    // updateCounter
    deltaBuf.writeByte(1);      // ADD tag
    deltaBuf.writeUint16(2);    // index
    deltaBuf.writeUint64(9999ULL);
    deltaBuf.writeAscii("New Member");
    dispatcher.dispatchDelta(deltaBuf);

    const auto* obj = store.findAs<GroupObject>(objectId);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->base6.has_value());
    REQUIRE(obj->base6->members.size() == 3);
    CHECK(obj->base6->members[2].objectId == 9999ULL);
    CHECK(obj->base6->members[2].name == "New Member");
    // Untouched entries prove this was a targeted write, not a re-decode.
    CHECK(obj->base6->members[0].objectId == 281474993937960ULL);
}

TEST_CASE("ObjectStore: a GRUP member-list REMOVE op erases the stored entry at that index") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    uint64_t objectId = 17287767ULL;
    auto buf6 = buildEnvelopeWithPayload(objectId, "GRUP", 6, 8, kRealGroupBase6Hex);
    dispatcher.dispatchBaseline(buf6);

    PacketBuffer deltaBuf;
    deltaBuf.writeUint64(objectId);
    deltaBuf.writeUint32(fourCC("GRUP"));
    deltaBuf.writeByte(6);
    deltaBuf.writeUint32(2);
    deltaBuf.writeUint16(1);
    deltaBuf.writeUint16(0x01);
    deltaBuf.writeUint32(1);
    deltaBuf.writeUint32(3);
    deltaBuf.writeByte(0);   // REMOVE tag
    deltaBuf.writeUint16(0); // index - remove the leader
    dispatcher.dispatchDelta(deltaBuf);

    const auto* obj = store.findAs<GroupObject>(objectId);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->base6.has_value());
    REQUIRE(obj->base6->members.size() == 1);
    CHECK(obj->base6->members[0].objectId == 281474993998083ULL); // the remaining member
}

TEST_CASE("ObjectStore: real HINO#7 composes onto the same InstallationObject its INSO 3/6 "
          "baselines already populated, not a colliding type") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    uint64_t objectId = 562949970715735ULL;
    auto buf3 = buildEnvelopeWithPayload(objectId, "INSO", 3, 5, kRealInstallationBase3Hex);
    dispatcher.dispatchBaseline(buf3);
    auto buf6 = buildEnvelopeWithPayload(objectId, "INSO", 6, 5, kRealInstallationBase6Hex);
    dispatcher.dispatchBaseline(buf6);
    auto buf7 = buildEnvelopeWithPayload(objectId, "HINO", 7, 5, kRealHarvesterBase7Hex);
    dispatcher.dispatchBaseline(buf7);

    REQUIRE(store.size() == 1); // one stored object, not three
    const auto* obj = store.findAs<InstallationObject>(objectId);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->base3.has_value());
    REQUIRE(obj->base6.has_value());
    REQUIRE(obj->base7.has_value());
    CHECK(obj->base3->tangible.customObjectName == u"");
    CHECK(obj->base7->isActive == false);
    REQUIRE(obj->base7->resourceNameList.size() == 2);
    CHECK(obj->base7->resourceNameList[0] == "Ejoga");
    CHECK(obj->baselineMessagesSeen == 3);
}

TEST_CASE("ObjectStore: a real INSO#7 delta mutates the stored HarvesterObject BASE7 hopper in "
          "place, despite arriving under a different tag than its own baseline") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    uint64_t objectId = 562949970715735ULL;
    auto buf7 = buildEnvelopeWithPayload(objectId, "HINO", 7, 5, kRealHarvesterBase7ActiveHex);
    dispatcher.dispatchBaseline(buf7);

    auto deltaBuf = buildEnvelopeWithPayload(objectId, "INSO", 7, 4, kRealHarvesterDelta7Hex);
    dispatcher.dispatchDelta(deltaBuf);

    const auto* obj = store.findAs<InstallationObject>(objectId);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->base7.has_value());
    CHECK(obj->base7->isActive == true); // untouched by this delta - proves a targeted write
    REQUIRE(obj->base7->hopperList.size() == 1);
    CHECK(obj->base7->hopperList[0].quantity == doctest::Approx(5.0f)); // was 2.0 in the baseline
    CHECK(obj->base7->hopperSize == doctest::Approx(5.0f));
    CHECK(obj->deltaMessagesSeen == 1);
    CHECK(obj->deltaFieldsSkipped == 0);
}

TEST_CASE("ObjectStore: forEach visits every stored object regardless of concrete type") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    auto tanoBuf = buildEnvelopeWithPayload(1ULL, "TANO", 3, 11, kRealTangibleBase3Hex);
    dispatcher.dispatchBaseline(tanoBuf);
    auto rcnoBuf = buildEnvelopeWithPayload(2ULL, "RCNO", 3, 15, kRealResourceContainerBase3Hex);
    dispatcher.dispatchBaseline(rcnoBuf);

    size_t visited = 0;
    store.forEach([&](const auto&) { ++visited; });
    CHECK(visited == 2);
}

// Movement work, 2026-07-18: applyTransform()/applyTransformWithParent() are
// plain top-level messages, not baseline/delta-enveloped, so these tests
// call them directly rather than through ObjectStateDispatcher. Both real
// fixtures below are the exact real captured payloads already pinned in
// test_swgproto_transform.cpp ("Kalda Ulzo", Finalizer, Tatooine) - reused
// here rather than re-captured, since this test's job is proving the STORE
// wiring, not re-proving UpdateTransformMessage/UpdateTransformWithParentMessage's
// own parse correctness (already covered there). The baseline used to first
// populate each object is real TANO BASE3 payload content
// (kRealTangibleBase3Hex) - only the objectId it's stored under is chosen to
// match the real transform fixture's own embedded objectId, a deliberate
// cross-fixture pairing (this project's established "hybrid fixture"
// technique - see PHASE_06_STATUS.md's InstallationObject entry for
// precedent) since no single real capture happens to contain both a baseline
// and a transform update for the same object.
TEST_CASE("ObjectStore: a real UpdateTransformMessage updates an already-tracked object's position") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    const uint64_t objectId = 281479010137060ULL; // matches the real transform fixture below
    auto baselineBuf = buildEnvelopeWithPayload(objectId, "TANO", 3, 11, kRealTangibleBase3Hex);
    dispatcher.dispatchBaseline(baselineBuf);

    auto transformBuf =
        bufferFromHex("e4 33 69 f0 00 00 01 00 fd 32 14 00 46 b1 bf 07 00 00 01 03");
    auto msg = swgproto::UpdateTransformMessage::parse(transformBuf);
    REQUIRE(msg.objectId == objectId);
    store.applyTransform(msg);

    const auto* obj = store.findAs<TangibleObject>(objectId);
    REQUIRE(obj != nullptr);
    CHECK(obj->x == doctest::Approx(3263.25f));
    CHECK(obj->y == doctest::Approx(5.0f));
    CHECK(obj->z == doctest::Approx(-5038.5f));
    CHECK(obj->parentId == 0);
    CHECK(obj->movementCounter == 1983);
    CHECK(obj->speed == 1);
    CHECK(obj->direction == 3);
    CHECK(obj->transformMessagesSeen == 1);
}

TEST_CASE("ObjectStore: a real UpdateTransformWithParentMessage updates an already-tracked "
          "object's cell-relative position") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    const uint64_t objectId = 281478993986991ULL; // matches the real transform fixture below
    auto baselineBuf = buildEnvelopeWithPayload(objectId, "TANO", 3, 11, kRealTangibleBase3Hex);
    dispatcher.dispatchBaseline(baselineBuf);

    auto transformBuf = bufferFromHex(
        "fd 85 10 00 00 00 00 00 af c5 72 ef 00 00 01 00 72 00 f9 ff 0b 00 86 fd 14 00 00 59");
    auto msg = swgproto::UpdateTransformWithParentMessage::parse(transformBuf);
    REQUIRE(msg.objectId == objectId);
    store.applyTransformWithParent(msg);

    const auto* obj = store.findAs<TangibleObject>(objectId);
    REQUIRE(obj != nullptr);
    CHECK(obj->x == doctest::Approx(14.25f));
    CHECK(obj->y == doctest::Approx(-0.875f));
    CHECK(obj->z == doctest::Approx(1.375f));
    CHECK(obj->parentId == 1082877ULL);
    CHECK(obj->movementCounter == 1375622);
    CHECK(obj->speed == 0);
    CHECK(obj->direction == 89);
    CHECK(obj->transformMessagesSeen == 1);
}

// Real fixture bytes reused verbatim from test_swgproto_objcontroller.cpp's
// "DataTransform::parse - real payload (Kalda Ulzo, Finalizer, Tatooine)"
// case - this test's job is proving ObjectStore::applyDataTransform wires
// that already-verified parse into a stored object correctly (position,
// derived yaw byte, AND the raw quaternion), not re-proving the parse
// itself. Confirmed live 2026-07-18 this is exactly the message type/shape
// self receives at zone-in (see SESSION_LOG.md), closing the gap
// applyTransform() alone can't: nothing about a stationary/idle object ever
// triggers an UpdateTransformMessage broadcast.
TEST_CASE("ObjectStore: a real DataTransform updates an already-tracked object's position, "
          "yaw, and quaternion") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    const uint64_t objectId = 12345ULL; // arbitrary - DataTransform carries no objectId of its own
    auto baselineBuf = buildEnvelopeWithPayload(objectId, "TANO", 3, 11, kRealTangibleBase3Hex);
    dispatcher.dispatchBaseline(baselineBuf);

    auto dataTransformBuf = bufferFromHex(
        "97 02 00 00 00 00 00 00 45 b9 88 be 00 00 00 00 1e b4 76 3f 3a 4a 5b 45 00 b0 b0 40 "
        "e7 da 99 c5 00 00 00 00");
    auto msg = swgproto::DataTransform::parse(dataTransformBuf);
    store.applyDataTransform(objectId, msg);

    const auto* obj = store.findAs<TangibleObject>(objectId);
    REQUIRE(obj != nullptr);
    CHECK(obj->x == doctest::Approx(3508.639f));
    CHECK(obj->y == doctest::Approx(5.521484f));
    CHECK(obj->z == doctest::Approx(-4923.363f));
    CHECK(obj->parentId == 0);
    CHECK(obj->speed == 0);
    CHECK(obj->direction == 91);
    CHECK(obj->transformMessagesSeen == 1);
    CHECK(obj->quatX == doctest::Approx(0.0f));
    CHECK(obj->quatY == doctest::Approx(-0.2670385f));
    CHECK(obj->quatZ == doctest::Approx(0.0f));
    CHECK(obj->quatW == doctest::Approx(0.9636859f));
    CHECK(obj->dataTransformMessagesSeen == 1);
}

// Real fixture bytes reused verbatim from test_swgproto_objcontroller.cpp's
// "DataTransformWithParent::parse - real payload (Kalda Ulzo, Finalizer,
// Tatooine)" case - the cell-relative counterpart to the test above, proving
// parentId is carried through onto the stored object too.
TEST_CASE("ObjectStore: a real DataTransformWithParent updates an already-tracked object's "
          "cell-relative position, yaw, and quaternion") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    const uint64_t objectId = 67890ULL; // arbitrary - DataTransformWithParent's own objectId
                                         // field is its parentId, not the target object's id
    auto baselineBuf = buildEnvelopeWithPayload(objectId, "TANO", 3, 11, kRealTangibleBase3Hex);
    dispatcher.dispatchBaseline(baselineBuf);

    auto dataTransformBuf = bufferFromHex(
        "01 07 09 00 fd 85 10 00 00 00 00 00 00 00 00 00 00 00 80 3f 00 00 00 00 2e bd 3b b3 "
        "19 9a 0a c1 66 66 66 bf e5 6c 08 c0 00 00 00 00");
    auto msg = swgproto::DataTransformWithParent::parse(dataTransformBuf);
    store.applyDataTransformWithParent(objectId, msg);

    const auto* obj = store.findAs<TangibleObject>(objectId);
    REQUIRE(obj != nullptr);
    CHECK(obj->x == doctest::Approx(-8.662621f));
    CHECK(obj->y == doctest::Approx(-0.9f));
    CHECK(obj->z == doctest::Approx(-2.131646f));
    CHECK(obj->parentId == 1082877ULL);
    CHECK(obj->speed == 0);
    CHECK(obj->direction == 50);
    CHECK(obj->transformMessagesSeen == 1);
    CHECK(obj->quatX == doctest::Approx(0.0f));
    CHECK(obj->quatY == doctest::Approx(1.0f));
    CHECK(obj->quatZ == doctest::Approx(0.0f));
    CHECK(obj->quatW == doctest::Approx(-4.371139e-08f));
    CHECK(obj->dataTransformMessagesSeen == 1);
}

TEST_CASE("ObjectStore: a transform update for an untracked objectId is silently a no-op") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    auto transformBuf =
        bufferFromHex("e4 33 69 f0 00 00 01 00 fd 32 14 00 46 b1 bf 07 00 00 01 03");
    auto msg = swgproto::UpdateTransformMessage::parse(transformBuf);
    store.applyTransform(msg); // no baseline was ever dispatched for this objectId

    CHECK(store.size() == 0);
    CHECK(store.find(msg.objectId) == nullptr);
}

// applyContainment() - Phase 17 Step 2. Real objectIds/containerId from the
// same Phase 17 capture ObjectMenuSelect's own permanent test uses (a real
// elevator cell inside "Eese's House" on Naritus) - not a synthetic
// placeholder, since UpdateContainmentMessage's wire parsing was already
// pinned by an earlier test; what's new here is ObjectStore's own storage
// behavior, so real IDs are used to keep this test traceable back to an
// actual session rather than because the parse itself needed re-proving.
TEST_CASE("ObjectStore: a real UpdateContainmentMessage updates an already-tracked object's "
          "containerId") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/281474993774038ULL);
    store.registerHandlers(dispatcher);

    const uint64_t objectId = 281474993774038ULL;    // self, from the real Phase 17 capture
    const uint64_t containerId = 281474994287358ULL;  // the real elevator cell from that capture
    auto baselineBuf = buildEnvelopeWithPayload(objectId, "TANO", 3, 11, kRealTangibleBase3Hex);
    dispatcher.dispatchBaseline(baselineBuf);

    store.applyContainment(objectId, containerId);

    const auto* obj = store.find(objectId);
    REQUIRE(obj != nullptr);
    CHECK(obj->containerId == containerId);
    CHECK(obj->containmentMessagesSeen == 1);

    // A second update (e.g. leaving the cell) overwrites, not accumulates.
    store.applyContainment(objectId, 0);
    CHECK(obj->containerId == 0);
    CHECK(obj->containmentMessagesSeen == 2);
}

TEST_CASE("ObjectStore: a containment update for an untracked objectId is silently a no-op") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    store.applyContainment(/*objectId=*/281474993774038ULL, /*containerId=*/281474994287358ULL);

    CHECK(store.size() == 0);
}

// seedPosition() replaced DataTransform as the fix for self/stationary
// objects never rendering (see ObjectStore.h's comment) - unlike every
// apply*() above, an untracked objectId is explicitly NOT a no-op here,
// since SceneCreateObjectByCrc routinely arrives before its own object's
// baseline. This case covers the already-tracked path (baseline arrived
// first).
TEST_CASE("ObjectStore: seedPosition updates an already-tracked object's position and "
          "orientation immediately") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    const uint64_t objectId = 111ULL;
    auto baselineBuf = buildEnvelopeWithPayload(objectId, "TANO", 3, 11, kRealTangibleBase3Hex);
    dispatcher.dispatchBaseline(baselineBuf);

    store.seedPosition(objectId, /*x=*/100.0f, /*y=*/5.0f, /*z=*/-200.0f, /*quatX=*/0.0f,
                        /*quatY=*/1.0f, /*quatZ=*/0.0f, /*quatW=*/0.0f, /*objectCrc=*/0xABCD1234u);

    const auto* obj = store.findAs<TangibleObject>(objectId);
    REQUIRE(obj != nullptr);
    CHECK(obj->x == doctest::Approx(100.0f));
    CHECK(obj->y == doctest::Approx(5.0f));
    CHECK(obj->z == doctest::Approx(-200.0f));
    CHECK(obj->quatX == doctest::Approx(0.0f));
    CHECK(obj->quatY == doctest::Approx(1.0f));
    CHECK(obj->quatZ == doctest::Approx(0.0f));
    CHECK(obj->quatW == doctest::Approx(0.0f));
    CHECK(obj->transformMessagesSeen == 1);
    CHECK(obj->dataTransformMessagesSeen == 1);
    CHECK(obj->objectCrc == 0xABCD1234u);
}

// The common real-world ordering: SceneCreateObjectByCrc (carrying position)
// arrives BEFORE the object's own baseline, so seedPosition() must buffer
// rather than drop it, and applyBaseline() must apply the buffered seed the
// moment it creates the entry.
TEST_CASE("ObjectStore: seedPosition buffers for an untracked objectId and applies once its "
          "baseline arrives") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    store.registerHandlers(dispatcher);

    const uint64_t objectId = 222ULL;
    store.seedPosition(objectId, /*x=*/42.0f, /*y=*/-7.5f, /*z=*/13.25f, /*quatX=*/0.0f,
                        /*quatY=*/0.0f, /*quatZ=*/0.0f, /*quatW=*/1.0f, /*objectCrc=*/0x11223344u);

    // Not yet tracked - the seed is buffered, not applied or dropped.
    CHECK(store.size() == 0);
    CHECK(store.find(objectId) == nullptr);

    auto baselineBuf = buildEnvelopeWithPayload(objectId, "TANO", 3, 11, kRealTangibleBase3Hex);
    dispatcher.dispatchBaseline(baselineBuf);

    const auto* obj = store.findAs<TangibleObject>(objectId);
    REQUIRE(obj != nullptr);
    CHECK(obj->x == doctest::Approx(42.0f));
    CHECK(obj->y == doctest::Approx(-7.5f));
    CHECK(obj->z == doctest::Approx(13.25f));
    CHECK(obj->quatW == doctest::Approx(1.0f));
    CHECK(obj->objectCrc == 0x11223344u);
    CHECK(obj->transformMessagesSeen == 1);
    CHECK(obj->dataTransformMessagesSeen == 1);
}
