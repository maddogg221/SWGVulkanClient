// Permanent regression test for worldmodel::SingletonRegistry - proves
// GuildObject (GILD) traffic is claimed independently of
// worldmodel::ObjectStore and never enters its per-objectId map, matching
// the design rationale in SingletonRegistry.h (GuildObject is a zone-wide
// manager's directory, not a per-instance world object). Uses a small
// synthetic-but-schema-accurate GuildObjectBaseline3 payload rather than
// re-embedding test_swgproto_guildobject.cpp's real 1697-byte/101-entry
// fixture - this test's job is proving DISPATCH WIRING and the store/
// registry separation, not re-proving GuildObjectBaseline3::parse's own
// correctness (already exhaustively covered there with real bytes), the
// same "wiring tests can be synthetic" precedent test_objectstatedispatcher.cpp
// itself already establishes.
#include <doctest/doctest.h>

#include "soe/PacketBuffer.h"
#include "swgproto/ObjectStateDispatcher.h"
#include "worldmodel/ObjectStore.h"
#include "worldmodel/SingletonRegistry.h"

using soe::PacketBuffer;
using swgproto::ObjectStateDispatcher;
using worldmodel::ObjectStore;
using worldmodel::SingletonRegistry;

namespace {

uint32_t fourCC(const std::string& tag) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(tag[0])) << 24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(tag[1])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(tag[2])) << 8) |
           static_cast<uint32_t>(static_cast<uint8_t>(tag[3]));
}

} // namespace

TEST_CASE("SingletonRegistry: a GILD baseline populates the guild directory, keyed on the "
          "manager's own object id") {
    ObjectStateDispatcher dispatcher;
    SingletonRegistry registry;
    registry.registerHandlers(dispatcher);

    PacketBuffer buf;
    buf.writeUint64(4017234474ULL); // the GuildManager's own object id, not a per-guild id
    buf.writeUint32(fourCC("GILD"));
    buf.writeByte(3);
    buf.writeUint32(2);
    buf.writeUint16(5); // count

    // Synthetic-but-schema-accurate GuildObjectBaseline3 body: complexity=1.0,
    // objectName={file="String_id_table", id=""}, name="" (unicode),
    // unknownField=0, guildList=["111:AAA", "222:BBB"].
    buf.writeFloat(1.0f);
    buf.writeAscii("String_id_table");
    buf.writeUint32(0);
    buf.writeAscii("");
    buf.writeUint32(0); // name (unicode, 0 chars)
    buf.writeUint32(0); // unknownField
    buf.writeUint32(2); // guildList count
    buf.writeUint32(2); // guildList updateCounter
    buf.writeAscii("111:AAA");
    buf.writeAscii("222:BBB");

    dispatcher.dispatchBaseline(buf);

    const auto* directory = registry.guildDirectory();
    REQUIRE(directory != nullptr);
    CHECK(directory->managerObjectId == 4017234474ULL);
    REQUIRE(directory->base3.has_value());
    REQUIRE(directory->base3->guildList.size() == 2);
    CHECK(directory->base3->guildList[0] == "111:AAA");
    CHECK(directory->base3->guildList[1] == "222:BBB");
    CHECK_FALSE(directory->base6.has_value()); // BASE6 never arrived - independent slot
}

TEST_CASE("SingletonRegistry: BASE3 and BASE6 slots populate independently on the same "
          "directory") {
    ObjectStateDispatcher dispatcher;
    SingletonRegistry registry;
    registry.registerHandlers(dispatcher);

    PacketBuffer buf6;
    buf6.writeUint64(4017234474ULL);
    buf6.writeUint32(fourCC("GILD"));
    buf6.writeByte(6);
    buf6.writeUint32(2);
    buf6.writeUint16(1);
    buf6.writeUint32(0x3B); // unknownConst - real captured value

    dispatcher.dispatchBaseline(buf6);

    const auto* directory = registry.guildDirectory();
    REQUIRE(directory != nullptr);
    CHECK_FALSE(directory->base3.has_value()); // BASE3 never arrived in this test
    REQUIRE(directory->base6.has_value());
    CHECK(directory->base6->unknownConst == 0x3B);
}

TEST_CASE("SingletonRegistry: GILD traffic never enters ObjectStore's per-objectId map") {
    ObjectStateDispatcher dispatcher;
    ObjectStore store(/*selfCharacterId=*/0);
    SingletonRegistry registry;
    store.registerHandlers(dispatcher);
    registry.registerHandlers(dispatcher);

    PacketBuffer buf;
    buf.writeUint64(4017234474ULL);
    buf.writeUint32(fourCC("GILD"));
    buf.writeByte(3);
    buf.writeUint32(2);
    buf.writeUint16(5);
    buf.writeFloat(1.0f);
    buf.writeAscii("String_id_table");
    buf.writeUint32(0);
    buf.writeAscii("");
    buf.writeUint32(0);
    buf.writeUint32(0);
    buf.writeUint32(1);
    buf.writeUint32(1);
    buf.writeAscii("999:SOLO");

    dispatcher.dispatchBaseline(buf);

    CHECK(registry.guildDirectory() != nullptr);
    CHECK(store.size() == 0); // confirms the store/registry separation holds
    CHECK(store.find(4017234474ULL) == nullptr);
}
