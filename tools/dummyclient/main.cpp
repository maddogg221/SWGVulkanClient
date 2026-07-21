#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "clientcommon/HexDump.h"
#include "clientcommon/ObjControllerHandlers.h"
#include "soe/MessageDispatcher.h"
#include "soe/MessageHash.h"
#include "soe/PacketBuffer.h"
#include "soe/SoeSession.h"
#include "swgproto/AccountVersionMessage.h"
#include "swgproto/BaselineEnvelope.h"
#include "swgproto/ChatSystemMessage.h"
#include "swgproto/ClientCreateCharacter.h"
#include "swgproto/ClientCreateCharacterFailed.h"
#include "swgproto/CommandQueueEnqueue.h"
#include "swgproto/ClientCreateCharacterSuccess.h"
#include "swgproto/DataTransform.h"
#include "swgproto/DataTransformWithParent.h"
#include "swgproto/ClientIdMessage.h"
#include "swgproto/ClientPermissionsMessage.h"
#include "swgproto/ClientRandomNameRequestPacket.h"
#include "swgproto/ClientRandomNameResponse.h"
#include "swgproto/CmdSceneReady.h"
#include "swgproto/CmdStartScene.h"
#include "swgproto/CreatureObjectBaseline1.h"
#include "swgproto/CreatureObjectBaseline3.h"
#include "swgproto/CreatureObjectBaseline4.h"
#include "swgproto/CreatureObjectBaseline6.h"
#include "swgproto/CreatureObjectDelta.h"
#include "swgproto/EnumerateCharacterId.h"
#include "swgproto/ErrorMessage.h"
#include "swgproto/CellObjectBaseline3.h"
#include "swgproto/CellObjectBaseline6.h"
#include "swgproto/GuildObjectBaseline3.h"
#include "swgproto/GuildObjectBaseline6.h"
#include "swgproto/InstallationObjectBaseline3.h"
#include "swgproto/IntangibleObjectBaseline3.h"
#include "swgproto/LoginClientToken.h"
#include "swgproto/LoginClusterStatus.h"
#include "swgproto/LoginEnumCluster.h"
#include "swgproto/ObjControllerDispatcher.h"
#include "swgproto/ObjControllerMessage.h"
#include "swgproto/ObjectStateDispatcher.h"
#include "swgproto/PlayerObjectBaseline3.h"
#include "swgproto/PlayerObjectBaseline6.h"
#include "swgproto/PlayerObjectBaseline8.h"
#include "swgproto/PlayerObjectBaseline9.h"
#include "swgproto/FactoryCrateBaseline3.h"
#include "swgproto/FactoryCrateBaseline6.h"
#include "swgproto/PlayerObjectDelta.h"
#include "swgproto/SceneCreateObjectByCrc.h"
#include "swgproto/SceneDestroyObject.h"
#include "swgproto/StaticObjectBaseline3.h"
#include "swgproto/StaticObjectBaseline6.h"
#include "swgproto/SceneEndBaselines.h"
#include "swgproto/SchemaEngine.h"
#include "worldmodel/ObjectStore.h"
#include "worldmodel/SingletonRegistry.h"
#include "StringUtil.h"
#include "Visualizer.h"
#include "swgproto/SelectCharacter.h"
#include "swgproto/TangibleObjectBaseline3.h"
#include "swgproto/TangibleObjectBaseline6.h"
#include "swgproto/UpdateCellPermissionMessage.h"
#include "swgproto/UpdateContainmentMessage.h"
#include "swgproto/UpdatePvpStatusMessage.h"
#include "swgproto/UpdateTransformMessage.h"
#include "swgproto/UpdateTransformWithParentMessage.h"

namespace {

// Widens a plain ASCII CLI argument into the UTF-16 form message fields
// need. Only meant for simple ASCII names passed on the command line, not
// general Unicode input.
std::u16string toU16(const std::string& s) {
    std::u16string out;
    out.reserve(s.size());
    for (char ch : s) {
        out.push_back(static_cast<char16_t>(static_cast<unsigned char>(ch)));
    }
    return out;
}

// Registers a handler that logs an ErrorMessage and sets `failed`. Meant to
// be called ONCE per session, right after that session's MessageDispatcher
// is constructed - every phase that runs on the session afterward shares
// this same dispatcher/failed pair (passed into each phase function) rather
// than each phase re-registering its own ErrorMessage handling.
void registerTerminalErrorHandler(soe::MessageDispatcher& dispatcher, bool& failed) {
    dispatcher.on(swgproto::kErrorMessageHash, [&failed](soe::PacketBuffer& buf) {
        auto err = swgproto::ErrorMessage::parse(buf);
        std::cerr << "Server ErrorMessage: type=\"" << err.errorType << "\" msg=\""
                   << err.errorMsg << "\" fatal=" << static_cast<int>(err.fatal) << "\n";
        failed = true;
    });
}

// Receives and dispatches messages on `session` via `dispatcher` until
// `done` returns true or `failed` is set (by the terminal ErrorMessage
// handler registered once on `dispatcher` - see registerTerminalErrorHandler
// above). Shared by every phase so the receive-loop shape lives in exactly
// one place.
void pumpUntil(soe::SoeSession& session, soe::MessageDispatcher& dispatcher, const bool& failed,
               const std::function<bool()>& done) {
    while (!done() && !failed) {
        auto messages = session.receiveMessages(std::chrono::milliseconds(5000));
        for (auto& msg : messages) {
            dispatcher.dispatch(msg);
        }
    }
}

// Sends ClientIdMsg on an already-connected zone session and waits for
// ClientPermissionsMessage - the zone server's signal that it has finished
// associating this connection with the account. Required before ANY
// character-related action on the zone server (SelectCharacter or
// ClientCreateCharacter both need it) - see DISCOVERY.txt's "ClientIdMsg"
// note for why skipping or racing this step fails. Returns false if the
// server sends an ErrorMessage instead.
//
// `dispatcher`/`failed` are the zone session's shared pair (constructed
// once in main(), reused by every zone-side phase) - this phase registers
// its own ClientPermissionsMessage handler on top and removes it again
// before returning, so a later phase's dispatch() calls on the same
// dispatcher can't land on a handler capturing this phase's own (by then
// destroyed) local `gotPermissions` flag.
bool authenticateZoneSession(soe::SoeSession& zoneSession, soe::MessageDispatcher& dispatcher,
                              bool& failed, const swgproto::LoginClientToken& clientToken) {
    zoneSession.sendMessage(
        swgproto::buildClientIdMessage(clientToken.accountId, clientToken.sessionToken));
    std::cout << "Sent ClientIdMsg\n";

    bool gotPermissions = false;
    dispatcher.on(swgproto::kClientPermissionsMessageHash, [&](soe::PacketBuffer& buf) {
        auto perms = swgproto::ClientPermissionsMessage::parse(buf);
        std::cout << "ClientPermissionsMessage: canConnect=" << perms.canConnect
                   << " canCreateCharacter=" << perms.canCreateCharacter << "\n";
        gotPermissions = true;
    });

    pumpUntil(zoneSession, dispatcher, failed, [&] { return gotPermissions; });
    dispatcher.off(swgproto::kClientPermissionsMessageHash);

    return !failed;
}

// Tracks which objects exist during the baseline/world-state flood that
// arrives while zoning in. This is PLAN.md's Phase 2 step 3 goal made
// concrete: "prove correct framing and object-existence tracking first" -
// it's a diagnostic, not real state used by anything downstream yet. If the
// new envelope/message parsers below were mis-framed, this would show up
// here as parse exceptions (logged and dropped by MessageDispatcher) or as
// created/completed counts that don't line up, not just as silent garbage.
struct ObjectTracker {
    std::unordered_set<uint64_t> baselinesComplete;  // saw that object's SceneEndBaselines
};

// True for the self player's attached PlayerObject "ghost" - a separate
// object from the CreatureObject, linked via containment. Reads live from
// ObjectStore's own containerId field (populated by the permanent
// UpdateContainmentMessage handler in main(), see its comment) rather than
// a separate per-phase tracked set - containment is now real, persistent
// ObjectStore state for the whole connection, not something this function
// needs its own bookkeeping for. Collapses the 3-clause check that used to
// be repeated at every PLAY baseline/delta branch below.
bool isSelfPlayerGhost(const worldmodel::ObjectStore& objectStore, uint64_t characterId,
                        const swgproto::BaselineEnvelope& envelope) {
    if (swgproto::objectTypeTag(envelope.objectType) != "PLAY") {
        return false;
    }
    const worldmodel::WorldObject* obj = objectStore.find(envelope.objectId);
    return obj != nullptr && obj->containmentMessagesSeen > 0 && obj->containerId == characterId;
}

// Prints a delta-decode result in the shared "N/M updates decoded:
// field=value ..." format used by every delta branch below. `DecodeResultT`
// is a template (not one shared struct) because CreatureObjectDeltaDecode/
// PlayerObjectDeltaDecode/DeltaDecodeResult are deliberately separate types
// (see PlayerObjectDelta.h) that happen to share the same 4 members.
// Registers handlers for the small set of world-state bookkeeping messages
// still owned directly by dummyclient (SceneCreateObjectByCrc/SceneEndBaselines
// existence-tracking, UpdateContainmentMessage-based self-ghost identification)
// plus the generic onUnknownBaseline/onUnknownDelta fallback labels. Every
// object type's actual field decode now lives in worldmodel::ObjectStore
// (registered separately, once, in main() - see PLAN.md's Object Model
// design) or worldmodel::SingletonRegistry (GuildObject) - this function no
// longer decodes any object type's fields itself, a big reduction from its
// original scope now that the migration is complete.
void registerObjectTrackingHandlers(soe::MessageDispatcher& dispatcher, ObjectTracker& tracker,
                                     uint64_t characterId,
                                     swgproto::ObjControllerDispatcher& objControllerDispatcher,
                                     swgproto::ObjectStateDispatcher& objectStateDispatcher,
                                     worldmodel::ObjectStore& objectStore) {
    // NOTE: kSceneCreateObjectByCrcHash is deliberately NOT registered here
    // anymore (Phase 17) - main() now owns it permanently (see its own
    // comment) for the same forwarding-kill reason as
    // kObjControllerMessageHash/kUpdateContainmentMessageHash before it: a
    // real, live-caught bug where terminal objects inside "Eese's House"
    // never rendered at all, because their SceneCreateObjectByCrc arrived
    // AFTER zoneInAsCharacter()'s own zone-in flood settled (self walking
    // near/into their cell in real time, well after this function's own
    // teardown), by which point this per-phase handler had already been
    // torn down and silently dropped it - not a rendering bug, a message
    // this client never even saw. `tracker.created`'s own diagnostic value
    // moved to counting ObjectStore's own transformMessagesSeen directly
    // (see zoneInAsCharacter's own "Scene ready" summary) rather than
    // duplicating tracking that now lives in real, permanent state anyway.
    dispatcher.on(swgproto::kSceneEndBaselinesHash, [&](soe::PacketBuffer& buf) {
        auto end = swgproto::SceneEndBaselines::parse(buf);
        tracker.baselinesComplete.insert(end.objectId);
    });

    // Full field decode of PLAY baselines other than #3/#6/#8/#9 is
    // deferred; just label it so it isn't lumped into the generic
    // unrecognized-object count. Every other unmatched (objectType,
    // baselineNumber) key stays silent, matching the if/else chain this
    // replaces (it simply did nothing for anything it didn't explicitly
    // check).
    objectStateDispatcher.onUnknownBaseline(
        [&objectStore, characterId](const swgproto::BaselineEnvelope& env, soe::PacketBuffer&) {
            if (isSelfPlayerGhost(objectStore, characterId, env)) {
                std::cout << "PlayerObject ghost baseline #"
                           << static_cast<int>(env.baselineNumber)
                           << " seen for objectId=" << env.objectId
                           << " (self-attached) - not decoded, see step 5\n";
            }
        });

    objectStateDispatcher.onUnknownDelta(
        [&objectStore, characterId](const swgproto::BaselineEnvelope& env, soe::PacketBuffer&) {
            if (isSelfPlayerGhost(objectStore, characterId, env)) {
                std::cout << "PlayerObject ghost delta #" << static_cast<int>(env.baselineNumber)
                           << " seen for objectId=" << env.objectId
                           << " (self-attached) - not decoded, see step 5\n";
            }
        });

    // NOTE: kBaselinesMessageHash/kDeltasMessageHash are deliberately NOT
    // registered here anymore - main() already registers a persistent
    // forwarding handler into `objectStateDispatcher` that outlives this
    // function's call, and registering a second one here (capturing this
    // function's own phase-local `tracker` by reference) would replace it
    // with a handler that dangles the moment this function returns, since
    // unregisterObjectTrackingHandlers no longer tears these two down (see
    // its own comment). This was a real bug from the Phase 7 migration,
    // found and fixed while extending this function further.
    //
    // kUpdateContainmentMessageHash is likewise deliberately NOT registered
    // here anymore (Phase 17) - it used to be, with a phase-local tracker.
    // selfChildren set torn down the same way kBaselinesMessageHash/
    // kDeltasMessageHash used to be - the exact same forwarding-kill bug
    // class, just not yet caught for this hash: a real capture showed 286
    // real UpdateContainmentMessage occurrences in one session, the vast
    // majority long after unregisterObjectTrackingHandlers() would have torn
    // the old per-phase handler down. main() now registers this hash once,
    // permanently, forwarding into ObjectStore::applyContainment() - see its
    // comment - and isSelfPlayerGhost() above reads the result straight from
    // ObjectStore instead of a separate tracked set.
    dispatcher.on(swgproto::kUpdateCellPermissionMessageHash, [](soe::PacketBuffer& buf) {
        swgproto::UpdateCellPermissionMessage::parse(buf);
    });
    dispatcher.on(swgproto::kUpdatePvpStatusMessageHash,
                  [](soe::PacketBuffer& buf) { swgproto::UpdatePvpStatusMessage::parse(buf); });
    // Confirmed from source: this is how the zone-server MOTD (which
    // includes the server's own build revision - "last 10 commits" - see
    // PlayerManagerImplementation::sendLoginMessage()) is delivered. This
    // hash has shown up as "unknown" in every session's captures so far -
    // only the simple (plain Unicode string) wire variant is decoded here,
    // see ChatSystemMessage.h. A leftover-byte warning here means the
    // StringIdChatParameter variant arrived instead, which this project
    // deliberately doesn't decode (no real capture of it exists yet).
    dispatcher.on(swgproto::kChatSystemMessageHash, [](soe::PacketBuffer& buf) {
        auto msg = swgproto::ChatSystemMessage::parse(buf);
        std::cout << "ChatSystemMessage (displayType=" << static_cast<int>(msg.displayType)
                   << "): \"" << toUtf8Preview(msg.message) << "\"\n";
        if (buf.remaining() != 0) {
            std::cerr << "Warning: " << buf.remaining()
                       << " unparsed byte(s) left over after ChatSystemMessage - likely the "
                          "undecoded StringIdChatParameter variant\n";
        }
    });
    // kObjControllerMessageHash forwarding is no longer registered here -
    // main() now owns it permanently (see its 2026-07-18 fix comment) for the
    // same reason kBaselinesMessageHash/kDeltasMessageHash/the transform
    // hashes already were: a per-phase register/unregister cycle left a real
    // gap where a message could arrive with no handler at all.
}

// Mirrors registerObjectTrackingHandlers() - see authenticateZoneSession's
// comment on why every phase removes its own handlers from the shared
// dispatcher before returning.
//
// NOTE: `objectStateDispatcher` (and the kBaselinesMessageHash/
// kDeltasMessageHash top-level hooks that forward into it) are no longer
// torn down here - both are now constructed once in main() and live for the
// whole connection (see PLAN.md's Object Model design: worldmodel::ObjectStore
// needs a persistent dispatcher to receive baseline/delta traffic across
// every phase, not just the zone-in flood). registerObjectTrackingHandlers no
// longer registers any per-(tag, baselineNumber) onBaseline/onDelta keys at
// all - every object type's decode now lives in worldmodel::ObjectStore/
// SingletonRegistry, registered once in main() and never torn down - so the
// only `objectStateDispatcher` state this function needs to reset is the two
// generic fallback handlers, which DO capture this phase's own local
// `tracker` by reference and would dangle if left registered past this
// function's return.
void unregisterObjectTrackingHandlers(soe::MessageDispatcher& dispatcher,
                                       swgproto::ObjectStateDispatcher& objectStateDispatcher) {
    dispatcher.off(swgproto::kSceneEndBaselinesHash);
    dispatcher.off(swgproto::kUpdateCellPermissionMessageHash);
    dispatcher.off(swgproto::kUpdatePvpStatusMessageHash);
    dispatcher.off(swgproto::kChatSystemMessageHash);
    // kObjControllerMessageHash/kUpdateContainmentMessageHash/
    // kSceneCreateObjectByCrcHash are NOT torn down here anymore - main()
    // owns all three registrations permanently now
    // (see its 2026-07-18/Phase 17 fix comments).

    objectStateDispatcher.offUnknownBaseline();
    objectStateDispatcher.offUnknownDelta();
}

// Sends SelectCharacter for `characterId` and waits for the full
// CmdStartScene/CmdSceneReady exchange to reach "scene ready". Shared
// between the existing-character path and the just-created-character path,
// since both zone in identically from here. Returns false if the server
// sends an ErrorMessage instead. See authenticateZoneSession's comment
// above for why this phase removes its own handlers before returning.
//
// `objectStateDispatcher` is now provided by the caller (constructed once in
// main(), alongside worldmodel::ObjectStore's own registration onto it) -
// see unregisterObjectTrackingHandlers's comment for why it's no longer a
// local variable owned by this function.
bool zoneInAsCharacter(soe::SoeSession& zoneSession, soe::MessageDispatcher& dispatcher,
                        bool& failed, uint64_t characterId,
                        swgproto::ObjControllerDispatcher& objControllerDispatcher,
                        swgproto::ObjectStateDispatcher& objectStateDispatcher,
                        worldmodel::ObjectStore& objectStore,
                        std::string* outTerrainName = nullptr) {
    zoneSession.sendMessage(swgproto::buildSelectCharacter(characterId));
    std::cout << "Sent SelectCharacter\n";

    bool gotServerSceneReady = false;
    ObjectTracker tracker;
    registerObjectTrackingHandlers(dispatcher, tracker, characterId, objControllerDispatcher,
                                    objectStateDispatcher, objectStore);

    dispatcher.on(swgproto::kCmdStartSceneHash, [&](soe::PacketBuffer& buf) {
        auto startScene = swgproto::CmdStartScene::parse(buf);
        std::cout << "CmdStartScene: selfObjectId=" << startScene.selfObjectId << " terrain=\""
                   << startScene.terrainName << "\" pos=(" << startScene.x << ", "
                   << startScene.y << ", " << startScene.z << ")\n";
        if (outTerrainName != nullptr) {
            *outTerrainName = startScene.terrainName;
        }
        if (startScene.selfObjectId != characterId) {
            std::cerr << "Warning: CmdStartScene selfObjectId does not match the selected "
                         "character\n";
        }

        zoneSession.sendMessage(swgproto::buildCmdSceneReady());
        std::cout << "Sent CmdSceneReady\n";
    });

    dispatcher.on(swgproto::kCmdSceneReadyHash, [&](soe::PacketBuffer&) {
        // The server's own CmdSceneReady reply - this is what actually
        // flips "scene ready" client-side.
        gotServerSceneReady = true;
    });

    pumpUntil(zoneSession, dispatcher, failed, [&] { return gotServerSceneReady; });
    dispatcher.off(swgproto::kCmdStartSceneHash);
    dispatcher.off(swgproto::kCmdSceneReadyHash);
    unregisterObjectTrackingHandlers(dispatcher, objectStateDispatcher);

    if (failed) {
        return false;
    }

    std::cout << "\nScene ready.\n";
    // "Objects created" now reads straight from ObjectStore's own
    // transformMessagesSeen (set by the now-permanent SceneCreateObjectByCrc
    // handler - see its own comment) instead of a phase-local tracked set,
    // now that main() owns that registration for the whole connection, not
    // just this zone-in window.
    std::vector<uint64_t> createdObjectIds;
    objectStore.forEach([&createdObjectIds](const auto& obj) {
        if (obj.transformMessagesSeen > 0) {
            createdObjectIds.push_back(obj.objectId);
        }
    });
    std::cout << "Object tracking: " << createdObjectIds.size() << " objects created, "
               << tracker.baselinesComplete.size() << " completed baselines\n";

    size_t incomplete = 0;
    for (uint64_t oid : createdObjectIds) {
        if (tracker.baselinesComplete.find(oid) == tracker.baselinesComplete.end()) {
            ++incomplete;
        }
    }
    if (incomplete > 0) {
        std::cerr << "Warning: " << incomplete
                   << " object(s) were created but never received SceneEndBaselines - possible "
                      "framing/reassembly issue\n";
    }

    return true;
}

// Watches for movement/position traffic for a fixed 45s window after zoning
// in. Both UpdateTransformMessage/UpdateTransformWithParentMessage are pure
// server-to-client broadcasts (the client never sends this shape back - real
// movement intent is DataTransform/DataTransformWithParent, out of scope
// here), and this headless client never moves its own character, so the
// only way to see real traffic is to watch other nearby objects (NPCs,
// other players) move for a while after zone-in rather than during the
// brief zone-in flood itself. Deliberately minimal: a hardcoded window (no
// CLI flag) and the existing pumpUntil() with a time-based `done` check
// instead of a new pump helper.
//
// Movement work, 2026-07-18: this function no longer registers
// kUpdateTransformMessageHash/kUpdateTransformWithParentMessageHash itself -
// main() now owns that registration permanently, forwarding real decoded
// updates into worldmodel::ObjectStore (see WorldObject.h's new position
// fields and ObjectStore::applyTransform()/applyTransformWithParent()) so
// position data actually persists somewhere a future consumer (e.g. a
// renderer) can read it, instead of living only in a throwaway local map as
// it did before. Registering these two hashes here as well would silently
// replace main()'s handler for the duration of this window and then delete
// it entirely on the way out - the exact bug class already found and fixed
// twice this session (kBaselinesMessageHash/kDeltasMessageHash in
// runCommandResponseCapture(), kSceneDestroyObjectHash's design avoided it
// from the start). Instead, this function just waits, then reports which
// objects it already knows about (via `objectStore`) received a transform
// update DURING this window by comparing WorldObject::lastTransformUpdate
// against the window's own start time - the store, not a local map, is now
// the single source of truth for position data.
void observeMovement(soe::SoeSession& zoneSession, soe::MessageDispatcher& dispatcher, bool& failed,
                      swgproto::ObjControllerDispatcher& objControllerDispatcher,
                      const worldmodel::ObjectStore& objectStore) {
    // Phase 4: ObjControllerMessage sub-types (Animation, PostureMessage,
    // DataTransform, DataTransformWithParent, ...) mostly arrive from OTHER
    // nearby players during normal play, not during the zone-in flood.
    // kObjControllerMessageHash forwarding is no longer registered locally
    // here - main() now owns it permanently (see its 2026-07-18 fix comment),
    // the same forwarding-kill bug class already fixed for the other hashes:
    // a per-phase register/unregister cycle left a real gap (no handler at
    // all) between zoneInAsCharacter's teardown and this function's own
    // registration, silently dropping self's zone-in DataTransform.
    std::cout << "\nObserving movement traffic for 45s...\n";
    auto windowStart = std::chrono::steady_clock::now();
    auto deadline = windowStart + std::chrono::seconds(45);
    auto timeUp = [&] { return std::chrono::steady_clock::now() >= deadline; };
    // A quiet 5s slice (pumpUntil's own receiveMessages() timeout) is
    // expected here - unlike every other pumpUntil() caller, silence isn't
    // a failure for an open-ended observation window, just "nothing moved
    // yet." Swallow it and keep watching until the real deadline.
    while (!timeUp() && !failed) {
        try {
            pumpUntil(zoneSession, dispatcher, failed, timeUp);
        } catch (const soe::TimeoutError&) {
        }
    }

    // transformMessagesSeen on WorldObject is a lifetime-cumulative count
    // (see WorldObject.h), not windowed - so "moved during this window" is
    // determined via lastTransformUpdate against windowStart, not by
    // snapshotting/diffing the counter. Deliberately not reporting a
    // per-window message total for the same reason: nothing here tracks a
    // window-scoped count, and computing one accurately would need
    // snapshotting every tracked object's counter at windowStart first -
    // more machinery than this diagnostic window needs.
    size_t movedObjectCount = 0;
    std::vector<std::string> lines;
    objectStore.forEach([&](const auto& obj) {
        if (obj.transformMessagesSeen == 0 || obj.lastTransformUpdate < windowStart) {
            return;
        }
        ++movedObjectCount;
        std::ostringstream line;
        line << "  objectId=" << obj.objectId << " pos=(" << obj.x << ", " << obj.y << ", " << obj.z
             << ") parentId=" << obj.parentId << " movementCounter=" << obj.movementCounter
             << " speed=" << static_cast<int>(obj.speed)
             << " direction=" << static_cast<int>(obj.direction) << "\n";
        lines.push_back(line.str());
    });

    std::cout << "Observed transform updates for " << movedObjectCount
               << " different tracked objects this window.\n";
    std::cout << "Live positions:\n";
    for (const auto& line : lines) {
        std::cout << line;
    }
}

// State shared between runObjControllerCapture()'s network-pumping main
// thread and its stdin-reading input thread. Plain atomics are sufficient -
// both fields are simple flags, and the actual capture data
// (countsByType/unknownSamples) is only ever touched by the main thread (the
// input thread never reaches it), so no additional locking is needed.
struct ObjControllerCapture {
    std::atomic<bool> capturing{false};
    std::atomic<bool> stopRequested{false};
    std::map<uint32_t, size_t> countsByType;
    // Raw payload bytes for not-yet-decoded sub-types, capped per type so a
    // noisy combat sub-type (e.g. CombatSpam) doesn't grow unbounded -
    // enough samples to spot the wire shape by hand afterward.
    static constexpr size_t kMaxSamplesPerType = 5;
    std::map<uint32_t, std::vector<std::vector<uint8_t>>> unknownSamples;
};

// Watches for a bounded window after sending an admin command (--send-command)
// for two things a command's *effect* typically shows up as, neither of
// which the normal zone-in tracking handlers are still registered to catch
// by this point in the flow (they unregister right after "scene ready", well
// before a command is ever sent): (1) ChatSystemMessage replies - most
// slash-commands that talk back to the player (e.g. `/resource list`, error
// messages, `sendSystemMessage()` in general) use this exact message type,
// already decoded elsewhere in this file but not listened for here; (2) any
// newly created object (via SceneCreateObjectByCrc) as a direct result of
// the command, e.g. spawning a resource stack via `/object createresource` -
// its baseline/delta traffic is hex-dumped if it isn't already a decoded
// (tag, baselineNumber) type, since the whole point of this window is
// usually capturing real bytes for a NOT-yet-decoded type. Deliberately
// scoped to only objects created DURING this window, not just any baseline
// traffic - a populated area can have hundreds of ambient objects/deltas
// that would drown out the one new thing this call actually cares about.
//
// `objectStateDispatcher` is the SAME persistent instance main() forwards
// kBaselinesMessageHash/kDeltasMessageHash into for the whole connection's
// worldmodel::ObjectStore - this function does NOT install its own
// competing top-level forwarding for those two hashes (a real bug, fixed
// 2026-07-18: it used to construct a separate, empty ObjectStateDispatcher
// and re-register kBaselinesMessageHash/kDeltasMessageHash to point at it,
// which replaced main()'s forwarding handler; then on return it called
// dispatcher.off() on both hashes, deleting that replacement too and
// leaving NOTHING forwarding into the ObjectStore for the rest of the
// connection - so any later observeMovement() window silently stopped
// updating the store). Instead, this function temporarily borrows
// `objectStateDispatcher`'s onUnknownBaseline/onUnknownDelta hooks (safe:
// by the time this function ever runs, zoneInAsCharacter's own
// unregisterObjectTrackingHandlers() has already reset both back to the
// silent no-op default, so there is nothing to save/restore) and resets
// them back to that same no-op default before returning, via
// offUnknownBaseline()/offUnknownDelta() - leaving the real
// kBaselinesMessageHash/kDeltasMessageHash forwarding completely untouched.
void runCommandResponseCapture(soe::SoeSession& zoneSession, soe::MessageDispatcher& dispatcher,
                                bool& failed, int durationSeconds,
                                swgproto::ObjectStateDispatcher& objectStateDispatcher,
                                uint64_t commandTargetId = 0) {
    std::unordered_set<uint64_t> newObjectIds;
    // A command's own explicit target (e.g. --command-target) is "interesting"
    // for baseline/delta capture even if it wasn't created DURING this window -
    // it may have been created by an earlier command (e.g. a harvester placed
    // in a prior run), and a command like /synchronizeduilisten can cause the
    // server to (re)send that pre-existing object's baseline as a direct
    // consequence of THIS command, which is exactly the kind of real, not-yet-
    // decoded traffic this tool exists to catch.
    if (commandTargetId != 0) {
        newObjectIds.insert(commandTargetId);
    }

    dispatcher.on(swgproto::kChatSystemMessageHash, [](soe::PacketBuffer& buf) {
        auto msg = swgproto::ChatSystemMessage::parse(buf);
        std::cout << "[COMMAND REPLY] ChatSystemMessage (displayType="
                   << static_cast<int>(msg.displayType) << "): \"" << toUtf8Preview(msg.message)
                   << "\"\n";
        if (buf.remaining() != 0) {
            std::cerr << "Warning: " << buf.remaining()
                       << " unparsed byte(s) left over after ChatSystemMessage\n";
        }
    });
    dispatcher.on(swgproto::kSceneCreateObjectByCrcHash, [&](soe::PacketBuffer& buf) {
        auto obj = swgproto::SceneCreateObjectByCrc::parse(buf);
        newObjectIds.insert(obj.objectId);
        std::cout << "[COMMAND REPLY] SceneCreateObjectByCrc objectId=" << obj.objectId
                   << " objectCrc=0x" << std::hex << obj.objectCrc << std::dec << " pos=("
                   << obj.x << ", " << obj.y << ", " << obj.z << ")\n";
    });

    auto dumpIfNew = [&](const char* kind) {
        return [&newObjectIds, kind](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            if (newObjectIds.find(env.objectId) == newObjectIds.end()) {
                return;
            }
            auto bytes = buf.readBytes(buf.remaining());
            std::cout << "[COMMAND REPLY] " << kind
                       << " tag=" << swgproto::objectTypeTag(env.objectType) << " #"
                       << static_cast<int>(env.baselineNumber) << " objectId=" << env.objectId
                       << " count=" << env.count << " (" << bytes.size() << " bytes): ";
            clientcommon::printHexBytes(bytes);
            std::cout << "\n";
        };
    };
    objectStateDispatcher.onUnknownBaseline(dumpIfNew("baseline"));
    objectStateDispatcher.onUnknownDelta(dumpIfNew("delta"));

    // Hex-dump ANY unrecognized top-level message hash for the duration of
    // this window, not just baseline/delta traffic - the whole point of this
    // capture is often investigating a NOT-yet-decoded message type (e.g.
    // HarvesterResourceDataMessage), and the default handler only logs the
    // hash, never the bytes. Restored via offUnknown() below before this
    // function returns.
    dispatcher.onUnknown([](uint32_t hash, soe::PacketBuffer& buf) {
        auto bytes = buf.readBytes(buf.remaining());
        std::cout << "[COMMAND REPLY] unknown message hash=0x" << std::hex << hash << std::dec
                   << " (" << bytes.size() << " bytes): ";
        clientcommon::printHexBytes(bytes);
        std::cout << "\n";
    });

    std::cout << "\n[COMMAND REPLY] Watching for " << durationSeconds << "s...\n";
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
    auto timeUp = [&] { return std::chrono::steady_clock::now() >= deadline; };
    while (!timeUp() && !failed) {
        try {
            pumpUntil(zoneSession, dispatcher, failed, timeUp);
        } catch (const soe::TimeoutError&) {
        }
    }

    dispatcher.off(swgproto::kChatSystemMessageHash);
    dispatcher.off(swgproto::kSceneCreateObjectByCrcHash);
    dispatcher.offUnknown();
    objectStateDispatcher.offUnknownBaseline();
    objectStateDispatcher.offUnknownDelta();

    std::cout << "[COMMAND REPLY] done - " << newObjectIds.size() << " new object(s) seen.\n";
}

// Long-lived capture for HarvesterObject BASE7's ACTIVE state (extraction
// rate, real hopper accumulation). Unlike runCommandResponseCapture (a
// bounded ~15s connect-command-disconnect window), this stays connected for
// `durationSeconds` and periodically re-sends /synchronizeduilisten every
// `resyncIntervalSeconds` - required because the server auto-deactivates a
// harvester roughly 100s after its "operator" stops listening (a real,
// live-confirmed game mechanic, not a bug - see KNOWN_UNKNOWNS.md's
// "HarvesterObject BASE7" entry). `resyncIntervalSeconds` must stay well
// under that ~100s window.
//
// Polls and prints `harvesterId`'s decoded state (extraction rate, hopper
// size/contents, active flag) out of the real worldmodel::ObjectStore after
// each resync, rather than hex-dumping raw baseline/delta bytes via a
// separate local dispatcher the way this function originally did. That
// original design predated BASE7's delta decoder - written the same day
// Phase 9 started, before `applyHarvesterObjectBaseline7Delta` existed, back
// when this really was "capturing raw bytes for a type that isn't decoded
// yet." HarvesterObject BASE7's baseline (tag "HINO") and delta (tag "INSO")
// are now both registered on `objectStore` (see ObjectStore.cpp), so hex
// output would only ever show unrelated unknown traffic, not the harvester
// itself - and the same forwarding-kill bug fixed in
// runCommandResponseCapture() (see KNOWN_UNKNOWNS.md's bug 1.1 entry) applied
// here too via the exact same pattern (a local ObjectStateDispatcher +
// `dispatcher.off(kBaselinesMessageHash)`/`off(kDeltasMessageHash)` at the
// end, clobbering main()'s permanent forwarding into the real ObjectStore).
// Fixed 2026-07-18 by switching to this poll-based design instead of
// reproducing bug 1.1's exact fix here: `objectStore`'s real decode is a
// strictly better data source now than raw hex ever was, and this function
// never needs to touch `kBaselinesMessageHash`/`kDeltasMessageHash` at all
// this way.
void runHarvesterWatch(soe::SoeSession& zoneSession, soe::MessageDispatcher& dispatcher, bool& failed,
                        uint64_t characterId, uint64_t harvesterId, int durationSeconds,
                        int resyncIntervalSeconds, const worldmodel::ObjectStore& objectStore) {
    auto printState = [&](const char* when) {
        const auto* installation = objectStore.findAs<worldmodel::InstallationObject>(harvesterId);
        if (installation == nullptr || !installation->base7.has_value()) {
            std::cout << "[HARVESTER WATCH] " << when << ": no BASE7 state yet for objectId="
                       << harvesterId << "\n";
            return;
        }
        const auto& base7 = *installation->base7;
        std::cout << "[HARVESTER WATCH] " << when << ": objectId=" << harvesterId
                   << " isActive=" << base7.isActive << " extractionRateMax="
                   << base7.extractionRateMax << " actualExtractRate=" << base7.actualExtractRate
                   << " hopperSize=" << base7.hopperSize << " hopperSizeMax=" << base7.hopperSizeMax
                   << " hopperEntries=" << base7.hopperList.size() << "\n";
        for (const auto& entry : base7.hopperList) {
            std::cout << "[HARVESTER WATCH]   hopper: resourceSpawnId=" << entry.resourceSpawnId
                       << " quantity=" << entry.quantity << "\n";
        }
    };

    auto sendResync = [&]() {
        std::cout << "[HARVESTER WATCH] sending synchronizeduilisten to objectId=" << harvesterId
                   << "...\n";
        zoneSession.sendMessage(swgproto::buildCommandQueueEnqueue(
            characterId, 1, "synchronizeduilisten", harvesterId, std::u16string()));
    };

    std::cout << "\n[HARVESTER WATCH] watching objectId=" << harvesterId << " for " << durationSeconds
               << "s, resyncing every " << resyncIntervalSeconds << "s...\n";
    printState("before");
    sendResync();

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
    auto nextResync = std::chrono::steady_clock::now() + std::chrono::seconds(resyncIntervalSeconds);
    auto timeUp = [&] { return std::chrono::steady_clock::now() >= deadline; };

    while (!timeUp() && !failed) {
        if (std::chrono::steady_clock::now() >= nextResync) {
            printState("resync tick");
            sendResync();
            nextResync = std::chrono::steady_clock::now() + std::chrono::seconds(resyncIntervalSeconds);
        }
        try {
            pumpUntil(zoneSession, dispatcher, failed,
                      [&] { return timeUp() || std::chrono::steady_clock::now() >= nextResync; });
        } catch (const soe::TimeoutError&) {
        }
    }

    printState("final");
    std::cout << "[HARVESTER WATCH] done.\n";
}


// Interactive combat-capture mode: after zone-in, waits for the user to type
// exactly "go" (start recording) and "stop" (end + print a summary) on
// stdin, via a small dedicated input thread so typing doesn't block the
// network pump loop. Deliberately built for a one-off live data-gathering
// session (user plays for real via the actual game client while this
// headless client observes nearby broadcast traffic) - registers ONLY the
// ObjControllerMessage handler (no baseline/tracking/movement handlers),
// matching the "clean capture, only ObjectControllerMessage traffic"
// request. Note: if the connection drops before "stop" is typed, the input
// thread stays blocked on stdin until the user types something - acceptable
// for an interactive tool the user is actively driving, not worth extra
// complexity to guard against.
//
// Builds its OWN local swgproto::ObjControllerDispatcher (rather than
// reusing the shared one main() passes to zoneInAsCharacter/observeMovement)
// since capture mode's needs are a real superset: every known sub-type
// still gets decoded+printed via the same shared
// clientcommon::registerObjControllerHandlers() used everywhere else (with
// the "[CAPTURE] " prefix, and an onDecoded hook for per-type counting),
// but capture mode ALSO needs to count and hex-dump UNKNOWN sub-types
// (the ones this capture exists to investigate) via onUnknown() - behavior
// specific enough to this one feature that it isn't part of the shared
// clientcommon registration.
void runObjControllerCapture(soe::SoeSession& zoneSession, soe::MessageDispatcher& dispatcher,
                              bool& failed) {
    ObjControllerCapture capture;
    swgproto::ObjControllerDispatcher objControllerDispatcher;
    clientcommon::registerObjControllerHandlers(
        objControllerDispatcher, std::cout, "[CAPTURE] ",
        [&capture](const swgproto::ObjControllerMessage& envelope) {
            ++capture.countsByType[envelope.header2];
        });
    objControllerDispatcher.onUnknown(
        [&capture](const swgproto::ObjControllerMessage& envelope, soe::PacketBuffer& buf) {
            ++capture.countsByType[envelope.header2];
            auto bytes = buf.readBytes(buf.remaining());
            std::cout << "[CAPTURE] UNKNOWN header1=0x" << std::hex << envelope.header1
                       << " header2=0x" << envelope.header2 << std::dec
                       << " objectId=" << envelope.objectId << " (" << bytes.size()
                       << " bytes): ";
            clientcommon::printHexBytes(bytes);
            std::cout << "\n";

            auto& samples = capture.unknownSamples[envelope.header2];
            if (samples.size() < ObjControllerCapture::kMaxSamplesPerType) {
                samples.push_back(bytes);
            }
        });

    std::thread inputThread([&capture] {
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line == "go" && !capture.capturing.load()) {
                capture.capturing.store(true);
                std::cout << "\n>>> Capture STARTED - recording ObjControllerMessage traffic "
                             "<<<\n";
            } else if (line == "stop") {
                capture.stopRequested.store(true);
                std::cout << "\n>>> Capture STOPPED <<<\n";
                break;
            }
        }
    });

    // The "only record after 'go'" gate lives HERE, outside
    // objControllerDispatcher entirely - it decides whether to dispatch at
    // all, rather than threading a capturing check through every handler.
    dispatcher.on(swgproto::kObjControllerMessageHash,
                  [&capture, &objControllerDispatcher](soe::PacketBuffer& buf) {
                      if (!capture.capturing.load()) {
                          return;
                      }
                      objControllerDispatcher.dispatch(buf);
                  });

    std::cout << "\nReady for combat capture. Type 'go' to start recording "
                 "ObjControllerMessage traffic, 'stop' when done.\n";

    auto shouldStop = [&] { return capture.stopRequested.load(); };
    while (!shouldStop() && !failed) {
        try {
            pumpUntil(zoneSession, dispatcher, failed, shouldStop);
        } catch (const soe::TimeoutError&) {
        }
    }

    dispatcher.off(swgproto::kObjControllerMessageHash);

    if (inputThread.joinable()) {
        inputThread.join();
    }

    std::cout << "\n=== Capture summary ===\n";
    size_t total = 0;
    for (const auto& [header2, count] : capture.countsByType) {
        total += count;
        std::cout << "  header2=0x" << std::hex << header2 << std::dec << " count=" << count
                   << "\n";
    }
    std::cout << "Total: " << total << " ObjControllerMessage instances captured.\n";

    if (!capture.unknownSamples.empty()) {
        std::cout << "\nRaw samples for undecoded sub-types:\n";
        for (const auto& [header2, samples] : capture.unknownSamples) {
            std::cout << "  header2=0x" << std::hex << header2 << std::dec << ":\n";
            for (const auto& bytes : samples) {
                std::cout << "    (" << bytes.size() << " bytes): ";
                clientcommon::printHexBytes(bytes);
                std::cout << "\n";
            }
        }
    }
}

// Runs the full headless character-creation sequence on an already
// zone-authenticated session: optionally requests a server-suggested name
// (if the caller didn't provide one), sends ClientCreateCharacter, and
// waits for success/failure. Returns the new character's object ID via
// `outObjectId`, the actual resolved name (server-suggested or as given)
// via `outCharacterName` - needed so the caller can pass it to
// zoneInAsCharacter's name cross-check the same way the existing-character
// path already can from its login-time character list - and true/false via
// the return value. See authenticateZoneSession's comment above for why
// each sub-phase removes its own handlers before returning.
bool createCharacterHeadless(soe::SoeSession& zoneSession, soe::MessageDispatcher& dispatcher,
                              bool& failed, std::u16string characterName,
                              const std::string& templateName, const std::string& profession,
                              uint64_t& outObjectId, std::u16string& outCharacterName) {
    if (characterName.empty()) {
        zoneSession.sendMessage(swgproto::buildClientRandomNameRequestPacket(templateName));
        std::cout << "Sent ClientRandomNameRequest\n";

        bool gotName = false;
        dispatcher.on(swgproto::kClientRandomNameResponseHash, [&](soe::PacketBuffer& buf) {
            auto resp = swgproto::ClientRandomNameResponse::parse(buf);
            characterName = resp.name;
            std::cout << "ClientRandomNameResponse: name=\"" << toUtf8Preview(resp.name)
                       << "\"\n";
            gotName = true;
        });

        pumpUntil(zoneSession, dispatcher, failed, [&] { return gotName; });
        dispatcher.off(swgproto::kClientRandomNameResponseHash);

        if (failed) {
            return false;
        }
    }

    outCharacterName = characterName;

    swgproto::ClientCreateCharacterParams params;
    params.characterName = characterName;
    params.templateName = templateName;
    params.profession = profession;

    zoneSession.sendMessage(swgproto::buildClientCreateCharacter(params));
    std::cout << "Sent ClientCreateCharacter for \"" << toUtf8Preview(characterName) << "\"\n";

    bool done = false;
    bool success = false;

    dispatcher.on(swgproto::kClientCreateCharacterSuccessHash, [&](soe::PacketBuffer& buf) {
        auto result = swgproto::ClientCreateCharacterSuccess::parse(buf);
        std::cout << "ClientCreateCharacterSuccess: objectId=" << result.objectId << "\n";
        outObjectId = result.objectId;
        success = true;
        done = true;
    });
    dispatcher.on(swgproto::kClientCreateCharacterFailedHash, [&](soe::PacketBuffer& buf) {
        auto result = swgproto::ClientCreateCharacterFailed::parse(buf);
        std::cerr << "ClientCreateCharacterFailed: " << result.errorString << "\n";
        done = true;
    });

    pumpUntil(zoneSession, dispatcher, failed, [&] { return done; });
    dispatcher.off(swgproto::kClientCreateCharacterSuccessHash);
    dispatcher.off(swgproto::kClientCreateCharacterFailedHash);

    return success;
}

// ---- CLI parsing --------------------------------------------------------

struct CliOptions {
    // Real, public SWGEmu test server ("Finalizer") - a reasonable default to
    // point at without arguments. username/password have NO default on
    // purpose (previously hardcoded to a real personal test account here -
    // removed before this project's public release; every user must supply
    // their own real account via --username/--password).
    std::string host = "35.170.225.60";
    uint16_t port = 44453;
    std::string username;
    std::string password;
    std::string characterName; // empty = default to the first character on the account

    bool createCharacter = false;
    std::string newCharName; // empty = request a server-suggested name
    std::string charTemplate = "object/creature/player/human_male.iff";
    std::string charProfession = "crafting_artisan";
    int32_t galaxyId = -1; // -1 = default to the first galaxy in the list

    bool interactiveCapture = false; // run runObjControllerCapture() instead of observeMovement()

    std::string sendCommand;       // empty = don't send a command after zone-in
    std::string commandArgs;       // command argument text, e.g. coordinates for /teleport
    uint64_t commandTargetId = 0;  // 0 = no target (self/coordinate-based commands)

    uint64_t watchHarvesterId = 0;  // 0 = disabled; runs runHarvesterWatch() instead of --send-command
    int watchDurationSeconds = 300; // total time to stay connected and capture traffic
    int watchResyncSeconds = 60;    // how often to re-send synchronizeduilisten (must stay well under
                                     // the server's ~100s auto-deactivation window - see
                                     // KNOWN_UNKNOWNS.md's "HarvesterObject BASE7" entry)

    bool captureObjController = false; // hex-dump unrecognized ObjControllerMessage sub-types on
                                        // the PERSISTENT connection-lifetime dispatcher, spanning
                                        // zone-in through observeMovement - unlike
                                        // --interactive-capture's own separate dispatcher (built
                                        // fresh only once that mode starts), this catches
                                        // sub-types that fire during zone-in itself (e.g.
                                        // WeaponRanges, sent once per setWeapon() call, which
                                        // includes the default-weapon assignment at character load).

    bool visualize = false; // run runVisualizer() instead of observeMovement() - the crude
                             // wireframe debug visualizer (Windows/D3D11 only, see
                             // libs/renderer). Not built at all on non-Windows configurations.

    // Real SWG client install directory - used by the visualizer's
    // RealMeshResolver to open real .tre archives and render actual
    // geometry instead of placeholder boxes where resolution succeeds.
    // Defaults to this project's own dev machine's real install path;
    // override with --client-path on any other machine.
    std::string clientPath = "C:\\Program Files (x86)\\StarWarsGalaxies";
};

CliOptions parseCommandLine(int argc, char** argv) {
    CliOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("missing value for ") + name);
            }
            return argv[++i];
        };
        if (arg == "--host") opts.host = next("--host");
        else if (arg == "--port") opts.port = static_cast<uint16_t>(std::stoi(next("--port")));
        else if (arg == "--username") opts.username = next("--username");
        else if (arg == "--password") opts.password = next("--password");
        else if (arg == "--character-name") opts.characterName = next("--character-name");
        else if (arg == "--create-character") opts.createCharacter = true;
        else if (arg == "--char-name") opts.newCharName = next("--char-name");
        else if (arg == "--char-template") opts.charTemplate = next("--char-template");
        else if (arg == "--char-profession") opts.charProfession = next("--char-profession");
        else if (arg == "--galaxy-id") opts.galaxyId = std::stoi(next("--galaxy-id"));
        else if (arg == "--interactive-capture") opts.interactiveCapture = true;
        else if (arg == "--send-command") opts.sendCommand = next("--send-command");
        else if (arg == "--command-args") opts.commandArgs = next("--command-args");
        else if (arg == "--command-target") opts.commandTargetId = std::stoull(next("--command-target"));
        else if (arg == "--watch-harvester") opts.watchHarvesterId = std::stoull(next("--watch-harvester"));
        else if (arg == "--watch-duration") opts.watchDurationSeconds = std::stoi(next("--watch-duration"));
        else if (arg == "--watch-resync") opts.watchResyncSeconds = std::stoi(next("--watch-resync"));
        else if (arg == "--capture-objcontroller") opts.captureObjController = true;
        else if (arg == "--visualize") opts.visualize = true;
        else if (arg == "--client-path") opts.clientPath = next("--client-path");
    }

    return opts;
}

// ---- Login (Milestone 1) -------------------------------------------------

struct LoginResult {
    swgproto::LoginClientToken clientToken;
    swgproto::LoginEnumCluster galaxies;
    swgproto::LoginClusterStatus galaxyStatus;
    swgproto::EnumerateCharacterId characters;
};

// Sends AccountVersionMessage, collects the bundled login-response messages
// via `dispatcher` (the login session's dispatcher, constructed once in
// main() with its terminal ErrorMessage handler already registered), and
// prints the galaxy + character list. Returns std::nullopt if the server
// rejects the login (ErrorMessage).
std::optional<LoginResult> performLogin(soe::SoeSession& session, soe::MessageDispatcher& dispatcher,
                                         bool& failed, const CliOptions& opts) {
    session.sendMessage(swgproto::buildAccountVersionMessage(opts.username, opts.password));
    std::cout << "Sent AccountVersionMessage for \"" << opts.username << "\"\n";

    LoginResult result;
    bool gotClientToken = false;
    bool gotEnumCluster = false;
    bool gotClusterStatus = false;
    bool gotCharacterList = false;

    dispatcher.on(swgproto::kLoginClientTokenHash, [&](soe::PacketBuffer& buf) {
        result.clientToken = swgproto::LoginClientToken::parse(buf);
        std::cout << "LoginClientToken: accountId=" << result.clientToken.accountId
                   << " stationId=" << result.clientToken.stationId << " username=\""
                   << result.clientToken.username << "\"\n";
        gotClientToken = true;
    });
    dispatcher.on(swgproto::kLoginEnumClusterHash, [&](soe::PacketBuffer& buf) {
        result.galaxies = swgproto::LoginEnumCluster::parse(buf);
        gotEnumCluster = true;
    });
    dispatcher.on(swgproto::kLoginClusterStatusHash, [&](soe::PacketBuffer& buf) {
        result.galaxyStatus = swgproto::LoginClusterStatus::parse(buf);
        gotClusterStatus = true;
    });
    dispatcher.on(swgproto::kEnumerateCharacterIdHash, [&](soe::PacketBuffer& buf) {
        result.characters = swgproto::EnumerateCharacterId::parse(buf);
        gotCharacterList = true;
    });

    pumpUntil(session, dispatcher, failed, [&] {
        return gotClientToken && gotEnumCluster && gotClusterStatus && gotCharacterList;
    });

    dispatcher.off(swgproto::kLoginClientTokenHash);
    dispatcher.off(swgproto::kLoginEnumClusterHash);
    dispatcher.off(swgproto::kLoginClusterStatusHash);
    dispatcher.off(swgproto::kEnumerateCharacterIdHash);

    if (failed) {
        return std::nullopt;
    }

    std::cout << "\nGalaxies:\n";
    for (auto& g : result.galaxies.galaxies) {
        std::cout << "  [" << g.galaxyId << "] " << g.name;
        for (auto& s : result.galaxyStatus.galaxies) {
            if (s.galaxyId == g.galaxyId) {
                std::cout << " - " << s.address << ":" << s.zonePort << " (ping " << s.pingPort
                           << "), pop=" << s.population << ", status=" << s.status;
            }
        }
        std::cout << "\n";
    }

    std::cout << "\nCharacters:\n";
    for (auto& c : result.characters.characters) {
        std::cout << "  \"" << toUtf8Preview(c.name) << "\" (objectId=" << c.objectId
                   << ", galaxyId=" << c.galaxyId << ", type=" << c.characterType << ")\n";
    }

    return result;
}

// ---- Galaxy/character selection -----------------------------------------

struct TargetSelection {
    uint32_t galaxyId = 0;
    uint64_t existingCharacterId = 0; // 0 when creating a new character
    std::u16string existingCharacterName; // empty when creating a new character
};

// Decides which galaxy (and, for an existing character, which character) to
// zone into, based on --galaxy-id/--character-name overrides and the login
// response. Returns std::nullopt if there's nothing valid to select (e.g.
// an empty character list when not creating a new one).
std::optional<TargetSelection> selectGalaxyAndCharacter(const CliOptions& opts,
                                                          const LoginResult& login) {
    if (opts.createCharacter) {
        TargetSelection target;
        target.galaxyId = (opts.galaxyId >= 0) ? static_cast<uint32_t>(opts.galaxyId)
                                                 : login.galaxies.galaxies.front().galaxyId;
        return target;
    }

    if (login.characters.characters.empty()) {
        std::cerr << "No characters on this account - cannot proceed to zone connect.\n";
        return std::nullopt;
    }

    const swgproto::CharacterEntry* chosen = &login.characters.characters.front();
    if (!opts.characterName.empty()) {
        for (auto& c : login.characters.characters) {
            if (toUtf8Preview(c.name) == opts.characterName) {
                chosen = &c;
                break;
            }
        }
    }

    std::cout << "\nSelecting character \"" << toUtf8Preview(chosen->name)
               << "\" (objectId=" << chosen->objectId << ") on galaxy " << chosen->galaxyId
               << "\n";

    TargetSelection target;
    target.galaxyId = chosen->galaxyId;
    target.existingCharacterId = chosen->objectId;
    target.existingCharacterName = chosen->name;
    return target;
}

// Looks up the zone server address/port for `galaxyId` in a
// LoginClusterStatus response. Returns nullptr if the galaxy isn't present.
const swgproto::GalaxyStatus* findZoneInfo(const swgproto::LoginClusterStatus& galaxyStatus,
                                            uint32_t galaxyId) {
    for (auto& s : galaxyStatus.galaxies) {
        if (s.galaxyId == galaxyId) {
            return &s;
        }
    }
    return nullptr;
}

// ---- Character creation (Milestone 3) ------------------------------------

// Runs Milestone 3's headless character-creation flow on `zoneSession` and,
// on success, zones in as the new character. Returns the new character's
// object ID, or std::nullopt on failure.
std::optional<uint64_t> createNewCharacter(soe::SoeSession& zoneSession,
                                            soe::MessageDispatcher& dispatcher, bool& failed,
                                            const CliOptions& opts,
                                            swgproto::ObjControllerDispatcher& objControllerDispatcher,
                                            swgproto::ObjectStateDispatcher& objectStateDispatcher,
                                            worldmodel::ObjectStore& objectStore) {
    uint64_t newObjectId = 0;
    std::u16string createdName;
    bool created = createCharacterHeadless(zoneSession, dispatcher, failed, toU16(opts.newCharName),
                                            opts.charTemplate, opts.charProfession, newObjectId,
                                            createdName);
    if (!created) {
        std::cerr << "Character creation failed.\n";
        return std::nullopt;
    }

    std::cout << "\nMilestone 3 complete: created character objectId=" << newObjectId << ".\n";

    if (!zoneInAsCharacter(zoneSession, dispatcher, failed, newObjectId, objControllerDispatcher,
                            objectStateDispatcher, objectStore)) {
        std::cerr << "Zone-in failed after character creation.\n";
        return std::nullopt;
    }

    std::cout << "Milestone 3 complete (zoned in as the new character).\n";

    observeMovement(zoneSession, dispatcher, failed, objControllerDispatcher, objectStore);

    return newObjectId;
}

// Prints a one-line summary per stored object, read back OUT of ObjectStore/
// SingletonRegistry after zone-in - not a re-decode, proving the migration
// is behaviorally real, not just "it compiles" (see PHASE_07_STATUS.md's
// "done" criteria, originally scoped to the 2-type pilot, now covering
// every migrated type). Deliberately a compact identifying line per object
// (objectId + type + a couple of the most useful fields), not an exhaustive
// re-print of every single field every old inline handler used to show -
// that level of per-field detail is already covered by each type's own
// real-byte swgproto-level tests; this dump's job is proving the STORE
// populated correctly, not re-auditing every field.
void dumpObjectStoreSummary(const worldmodel::ObjectStore& store,
                             const worldmodel::SingletonRegistry& singletonRegistry) {
    std::cout << "\n=== Object Model dump (" << store.size() << " tracked objects) ===\n";
    store.forEach([](const auto& obj) {
        using T = std::decay_t<decltype(obj)>;
        std::cout << "objectId=" << obj.objectId;
        if constexpr (std::is_same_v<T, worldmodel::TangibleObject>) {
            std::cout << " TangibleObject";
            if (obj.base3.has_value()) {
                std::cout << " name=\"" << toUtf8Preview(obj.base3->customObjectName) << "\"";
            }
        } else if constexpr (std::is_same_v<T, worldmodel::ResourceContainer>) {
            std::cout << " ResourceContainer";
            if (obj.base3.has_value()) {
                std::cout << " quantity=" << obj.base3->quantity;
            }
            if (obj.base6.has_value()) {
                std::cout << " resourceName=\"" << toUtf8Preview(obj.base6->resourceName) << "\"";
            }
        } else if constexpr (std::is_same_v<T, worldmodel::CreatureObject>) {
            std::cout << " CreatureObject";
            if (obj.base3.has_value()) {
                std::cout << " name=\""
                           << toUtf8Preview(obj.base3->tangible.customObjectName) << "\"";
            }
            if (obj.base6.has_value()) {
                std::cout << " level=" << obj.base6->level << " weaponId=" << obj.base6->weaponId;
            }
        } else if constexpr (std::is_same_v<T, worldmodel::PlayerObject>) {
            std::cout << " PlayerObject";
            if (obj.base3.has_value()) {
                std::cout << " name=\""
                           << toUtf8Preview(obj.base3->intangible.customObjectName) << "\"";
            }
        } else if constexpr (std::is_same_v<T, worldmodel::WeaponObject>) {
            std::cout << " WeaponObject";
            if (obj.base3.has_value()) {
                std::cout << " name=\"" << toUtf8Preview(obj.base3->customObjectName) << "\"";
            }
        } else if constexpr (std::is_same_v<T, worldmodel::IntangibleObject>) {
            std::cout << " IntangibleObject";
            if (obj.base3.has_value()) {
                std::cout << " name=\"" << toUtf8Preview(obj.base3->customObjectName) << "\"";
            }
        } else if constexpr (std::is_same_v<T, worldmodel::InstallationObject>) {
            std::cout << " InstallationObject";
            if (obj.base3.has_value()) {
                std::cout << " name=\""
                           << toUtf8Preview(obj.base3->tangible.customObjectName)
                           << "\" active=" << obj.base3->activeFlag;
            }
            if (obj.base7.has_value()) {
                std::cout << " [harvester isActive=" << obj.base7->isActive
                           << " hopper=<" << obj.base7->hopperList.size() << " entries>]";
            }
        } else if constexpr (std::is_same_v<T, worldmodel::CellObject>) {
            std::cout << " CellObject";
            if (obj.base3.has_value()) {
                std::cout << " cellNumber=" << obj.base3->cellNumber;
            }
        } else if constexpr (std::is_same_v<T, worldmodel::FactoryCrate>) {
            std::cout << " FactoryCrate";
            if (obj.base3.has_value()) {
                std::cout << " useCount=" << obj.base3->useCount << " name=\""
                           << toUtf8Preview(obj.base3->customObjectName) << "\"";
            }
        } else if constexpr (std::is_same_v<T, worldmodel::StaticObject>) {
            std::cout << " StaticObject";
            if (obj.base3.has_value()) {
                std::cout << " name=\"" << toUtf8Preview(obj.base3->customObjectName) << "\"";
            }
        } else if constexpr (std::is_same_v<T, worldmodel::GroupObject>) {
            std::cout << " GroupObject";
            if (obj.base6.has_value()) {
                std::cout << " members=<" << obj.base6->members.size() << ">";
                for (const auto& member : obj.base6->members) {
                    std::cout << " [" << member.objectId << " \"" << member.name << "\"]";
                }
                std::cout << " level=" << obj.base6->groupLevel
                           << " masterLooterId=" << obj.base6->masterLooterId
                           << " lootRule=" << obj.base6->lootRule;
            }
        }
        std::cout << "\n";
    });

    if (const auto* guild = singletonRegistry.guildDirectory()) {
        std::cout << "GuildObject singleton: managerObjectId=" << guild->managerObjectId;
        if (guild->base3.has_value()) {
            std::cout << " guildList=<" << guild->base3->guildList.size() << " entries>";
        }
        if (guild->base6.has_value()) {
            std::cout << " unknownConst=" << guild->base6->unknownConst;
        }
        std::cout << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    // Flush after every operator<<, not just on a full buffer or clean exit -
    // when stdout is redirected to a file (as opposed to a real console),
    // the CRT switches to full block buffering, and a forceful process kill
    // (TerminateProcess, e.g. via PowerShell's Stop-Process -Force) never
    // runs C++ stream destructors/flush at all, silently discarding
    // whatever hadn't been flushed yet - a real diagnostic trap hit live
    // during Phase 17 (redirected log files appeared "frozen" for tens of
    // seconds while the process was actually fine, then stayed frozen even
    // after killing it, because the kill itself lost the buffered tail).
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    try {
        CliOptions opts = parseCommandLine(argc, argv);

        // One io_context for the whole run, shared by both SoeSession
        // instances below (login, then zone) - SoeSession doesn't own its
        // own io_context, so this is the only one that ever exists.
        asio::io_context io;

        std::cout << "Connecting to " << opts.host << ":" << opts.port << "...\n";
        soe::SoeSession session(io, opts.host, opts.port);
        session.connect();
        std::cout << "SOE handshake complete, crcSeed=0x" << std::hex << session.crcSeed()
                   << std::dec << "\n";

        soe::MessageDispatcher loginDispatcher;
        bool loginFailed = false;
        registerTerminalErrorHandler(loginDispatcher, loginFailed);

        auto login = performLogin(session, loginDispatcher, loginFailed, opts);
        if (!login) {
            std::cerr << "Login failed.\n";
            return 1;
        }
        std::cout << "\nMilestone 1 complete.\n";

        auto target = selectGalaxyAndCharacter(opts, *login);
        if (!target) {
            return 1;
        }

        const swgproto::GalaxyStatus* zoneInfo = findZoneInfo(login->galaxyStatus, target->galaxyId);
        if (zoneInfo == nullptr) {
            std::cerr << "Could not find zone server info for galaxyId=" << target->galaxyId
                       << "\n";
            return 1;
        }

        // Best-effort: a failure here shouldn't abort the run - the login
        // phase already fully succeeded, and this session is being
        // abandoned regardless. Not wrapping this meant a single failed
        // send here could kill the entire zone-connect phase before it
        // even started.
        try {
            session.disconnect();
        } catch (const std::exception& e) {
            std::cerr << "Warning: failed to gracefully disconnect login session: " << e.what()
                       << "\n";
        }

        std::cout << "Connecting to zone server " << zoneInfo->address << ":" << zoneInfo->zonePort
                   << "...\n";
        soe::SoeSession zoneSession(io, zoneInfo->address, zoneInfo->zonePort);
        zoneSession.connect();
        std::cout << "Zone SOE handshake complete, crcSeed=0x" << std::hex << zoneSession.crcSeed()
                   << std::dec << "\n";

        soe::MessageDispatcher zoneDispatcher;
        bool zoneFailed = false;
        registerTerminalErrorHandler(zoneDispatcher, zoneFailed);

        // Constructed once, shared by every zone-side phase that decodes
        // ObjControllerMessage traffic (zoneInAsCharacter, observeMovement) -
        // see PHASE_05_STATUS.md's dispatch-architecture refactor.
        // Interactive capture mode (below) builds its own separate instance
        // instead, since its needs (per-type counting, unknown-type
        // hex-dumping) are a real superset of this one's plain decode+print.
        swgproto::ObjControllerDispatcher objControllerDispatcher;
        clientcommon::registerObjControllerHandlers(objControllerDispatcher, std::cout,
                                                      "ObjController ");
        if (opts.captureObjController) {
            objControllerDispatcher.onUnknown(
                [](const swgproto::ObjControllerMessage& envelope, soe::PacketBuffer& buf) {
                    auto bytes = buf.readBytes(buf.remaining());
                    std::cout << "[OBJCONTROLLER CAPTURE] header1=0x" << std::hex << envelope.header1
                               << " header2=0x" << envelope.header2 << std::dec
                               << " objectId=" << envelope.objectId << " (" << bytes.size()
                               << " bytes): ";
                    clientcommon::printHexBytes(bytes);
                    std::cout << "\n";
                });
        }

        // Object Model (see PLAN.md/PHASE_07_STATUS.md): unlike
        // objControllerDispatcher above, this - and the BaselinesMessage/
        // DeltasMessage forwarding into it below - is genuinely NEW state,
        // not a pre-existing local moved here. Both now span the WHOLE
        // connection (zone-in + observeMovement + runCommandResponseCapture),
        // not just the zone-in flood zoneInAsCharacter's own local
        // ObjectTracker was scoped to - a deliberate change that fixes a real
        // gap (previously, no baseline/delta tracking happened at all outside
        // zoneInAsCharacter's own window). worldmodel::ObjectStore now claims
        // every per-instance object type's baseline+delta keys;
        // worldmodel::SingletonRegistry claims GuildObject (not a per-instance
        // object - see SingletonRegistry.h). registerObjectTrackingHandlers
        // no longer registers any object-type decode at all - just bookkeeping
        // (existence tracking, self-ghost identification) and the generic
        // unknown-key fallback labels.
        swgproto::ObjectStateDispatcher objectStateDispatcher;
        worldmodel::ObjectStore objectStore(target->existingCharacterId);
        worldmodel::SingletonRegistry singletonRegistry;
        objectStore.registerHandlers(objectStateDispatcher);
        singletonRegistry.registerHandlers(objectStateDispatcher);

        // DataTransform/DataTransformWithParent -> Object Model, 2026-07-18.
        // Overrides (not adds to - ObjControllerDispatcher::on() replaces on
        // re-registration, confirmed via its own header comment)
        // registerObjControllerHandlers()'s plain print-only handlers for
        // these two keys just above, since clientcommon's shared registration
        // is deliberately stateless glue and shouldn't own a worldmodel
        // dependency. Keeps the exact same print output (so nothing
        // regresses for anyone watching the console) while ALSO feeding
        // ObjectStore - closing the self/stationary-object rendering gap
        // confirmed live this session (a real DataTransform reliably arrives
        // for the self character right at zone-in, even on a fully passive
        // connection - see SESSION_LOG.md).
        objControllerDispatcher.on(
            swgproto::kDataTransformControllerType,
            [&objectStore](const swgproto::ObjControllerMessage& envelope, soe::PacketBuffer& buf) {
                auto dt = swgproto::DataTransform::parse(buf);
                std::cout << "ObjController DataTransform (idle sync): objectId="
                           << envelope.objectId << " counter=" << dt.counter << " pos=(" << dt.x
                           << ", " << dt.y << ", " << dt.z << ") speed=" << dt.speed << "\n";
                objectStore.applyDataTransform(envelope.objectId, dt);
            });
        objControllerDispatcher.on(
            swgproto::kDataTransformWithParentControllerType,
            [&objectStore](const swgproto::ObjControllerMessage& envelope, soe::PacketBuffer& buf) {
                auto dtp = swgproto::DataTransformWithParent::parse(buf);
                std::cout << "ObjController DataTransformWithParent (idle sync): objectId="
                           << envelope.objectId << " parentId=" << dtp.parentId
                           << " counter=" << dtp.counter << " pos=(" << dtp.x << ", " << dtp.y
                           << ", " << dtp.z << ") speed=" << dtp.speed << "\n";
                objectStore.applyDataTransformWithParent(envelope.objectId, dtp);
            });

        zoneDispatcher.on(swgproto::kBaselinesMessageHash,
                           [&objectStateDispatcher](soe::PacketBuffer& buf) {
                               objectStateDispatcher.dispatchBaseline(buf);
                           });
        zoneDispatcher.on(swgproto::kDeltasMessageHash,
                           [&objectStateDispatcher](soe::PacketBuffer& buf) {
                               objectStateDispatcher.dispatchDelta(buf);
                           });
        // Closes a real, previously-documented gap (see GAP_ANALYSIS.md
        // section 5 / KNOWN_UNKNOWNS.md): ObjectStore::remove() existed but
        // nothing ever called it, so objects that left the world
        // accumulated in the store forever. SceneDestroyObject
        // (kSceneDestroyObjectHash) is Core3's real, general-purpose
        // "remove this object from the world" signal - see
        // SceneDestroyObject.h's own comment for the full trace of real
        // server-side trigger sites. Registered once, permanently, same
        // lifetime as the baseline/delta forwarding above.
        zoneDispatcher.on(swgproto::kSceneDestroyObjectHash,
                           [&objectStore](soe::PacketBuffer& buf) {
                               auto msg = swgproto::SceneDestroyObject::parse(buf);
                               std::cout << "SceneDestroyObject: objectId=" << msg.objectId
                                          << " hyperspacing=" << msg.hyperspacing << "\n";
                               objectStore.remove(msg.objectId);
                           });
        // Movement work, 2026-07-18: wires the already-decoded
        // UpdateTransformMessage/UpdateTransformWithParentMessage broadcasts
        // into the Object Model, closing a real gap - position data used to
        // live only in observeMovement()'s own throwaway local map, never
        // reaching worldmodel::ObjectStore at all (so nothing downstream,
        // e.g. a future renderer, could ever read a tracked object's
        // position). Registered here, once, permanently - the ONLY place
        // these two hashes are ever registered now; observeMovement() no
        // longer registers them itself (see its own updated comment for why
        // a second local registration would replicate the exact
        // forwarding-kill bug class already found and fixed twice this
        // session for kBaselinesMessageHash/kDeltasMessageHash and
        // kSceneDestroyObjectHash).
        zoneDispatcher.on(swgproto::kUpdateTransformMessageHash,
                           [&objectStore](soe::PacketBuffer& buf) {
                               auto msg = swgproto::UpdateTransformMessage::parse(buf);
                               objectStore.applyTransform(msg);
                           });
        zoneDispatcher.on(swgproto::kUpdateTransformWithParentMessageHash,
                           [&objectStore](soe::PacketBuffer& buf) {
                               auto msg = swgproto::UpdateTransformWithParentMessage::parse(buf);
                               objectStore.applyTransformWithParent(msg);
                           });
        // Phase 17: UpdateContainmentMessage -> ObjectStore, permanent for the
        // whole connection (same pattern/rationale as the two transform
        // hashes just above) - previously registered/torn down per-phase
        // (registerObjectTrackingHandlers/unregisterObjectTrackingHandlers),
        // which lost every containment update outside the zone-in window. A
        // real capture showed 286 real occurrences in one session, most long
        // after that window closes (self walking between building cells,
        // riding an elevator, etc.) - exactly the traffic Step 2 of Phase 17
        // needs, and exactly the same forwarding-kill bug class already fixed
        // for kObjControllerMessageHash below.
        zoneDispatcher.on(swgproto::kUpdateContainmentMessageHash,
                           [&objectStore](soe::PacketBuffer& buf) {
                               auto msg = swgproto::UpdateContainmentMessage::parse(buf);
                               objectStore.applyContainment(msg.objectId, msg.containerId);
                           });
        // Phase 17: SceneCreateObjectByCrc -> ObjectStore, permanent for the
        // whole connection (same pattern as the two hashes just above) -
        // previously registered/torn down per-phase
        // (registerObjectTrackingHandlers/unregisterObjectTrackingHandlers),
        // which meant any object whose SceneCreateObjectByCrc arrived AFTER
        // zone-in settled was silently lost - real, live-caught impact:
        // terminal objects inside "Eese's House" never rendered at all
        // (transformMessagesSeen stayed 0 forever, so the render loop's own
        // "nothing to draw yet" guard skipped them permanently), because
        // their own creation message only arrives once self is actually
        // near/inside their cell, well after this window closes. Exactly
        // the same forwarding-kill bug class already fixed for
        // kObjControllerMessageHash/kUpdateContainmentMessageHash.
        zoneDispatcher.on(swgproto::kSceneCreateObjectByCrcHash,
                           [&objectStore](soe::PacketBuffer& buf) {
                               auto obj = swgproto::SceneCreateObjectByCrc::parse(buf);
                               objectStore.seedPosition(obj.objectId, obj.x, obj.y, obj.z,
                                                         obj.directionX, obj.directionY,
                                                         obj.directionZ, obj.directionW,
                                                         obj.objectCrc);
                           });
        // FIXED 2026-07-18: kObjControllerMessageHash forwarding used to be
        // registered/torn down per-phase (registerObjectTrackingHandlers /
        // unregisterObjectTrackingHandlers / observeMovement / runVisualizer
        // each had their own local copy) - the exact same forwarding-kill bug
        // class already fixed above for kBaselinesMessageHash/kDeltasMessageHash/
        // kSceneDestroyObjectHash/the two transform hashes, just not yet
        // migrated for this one. Confirmed as the real cause of self's
        // DataTransform going missing: unregisterObjectTrackingHandlers tears
        // this down the instant zoneInAsCharacter's pumpUntil sees the
        // server's CmdSceneReady, and runVisualizer/observeMovement don't
        // re-register their own copy until code later in main() runs - a real
        // gap with no handler at all, landing exactly when the server sends
        // self's zone-in DataTransform. Registered here once, permanently,
        // same lifetime as everything else above.
        zoneDispatcher.on(swgproto::kObjControllerMessageHash,
                           [&objControllerDispatcher](soe::PacketBuffer& buf) {
                               objControllerDispatcher.dispatch(buf);
                           });

        if (!authenticateZoneSession(zoneSession, zoneDispatcher, zoneFailed, login->clientToken)) {
            std::cerr << "Zone session authentication failed.\n";
            return 1;
        }

        if (opts.createCharacter) {
            return createNewCharacter(zoneSession, zoneDispatcher, zoneFailed, opts,
                                       objControllerDispatcher, objectStateDispatcher, objectStore)
                       ? 0
                       : 1;
        }

        std::string terrainName;
        if (!zoneInAsCharacter(zoneSession, zoneDispatcher, zoneFailed, target->existingCharacterId,
                                objControllerDispatcher, objectStateDispatcher, objectStore,
                                &terrainName)) {
            std::cerr << "Zone-in failed.\n";
            return 1;
        }
        std::cout << "Milestone 2 complete.\n";
        dumpObjectStoreSummary(objectStore, singletonRegistry);

        if (!opts.sendCommand.empty()) {
            std::cout << "Sending command \"" << opts.sendCommand << "\" args=\"" << opts.commandArgs
                       << "\" target=" << opts.commandTargetId << "...\n";
            zoneSession.sendMessage(swgproto::buildCommandQueueEnqueue(
                target->existingCharacterId, 1, opts.sendCommand, opts.commandTargetId,
                toU16(opts.commandArgs)));
            runCommandResponseCapture(zoneSession, zoneDispatcher, zoneFailed, 15,
                                       objectStateDispatcher, opts.commandTargetId);
        }

        if (opts.watchHarvesterId != 0) {
            runHarvesterWatch(zoneSession, zoneDispatcher, zoneFailed, target->existingCharacterId,
                               opts.watchHarvesterId, opts.watchDurationSeconds,
                               opts.watchResyncSeconds, objectStore);
        } else if (opts.interactiveCapture) {
            runObjControllerCapture(zoneSession, zoneDispatcher, zoneFailed);
#ifdef _WIN32
        } else if (opts.visualize) {
            runVisualizer(zoneSession, zoneDispatcher, zoneFailed, objControllerDispatcher,
                           objectStore, opts.clientPath, terrainName);
#endif
        } else {
            observeMovement(zoneSession, zoneDispatcher, zoneFailed, objControllerDispatcher,
                             objectStore);
        }

        return 0;
    } catch (const soe::TimeoutError& e) {
        std::cerr << "Timeout: " << e.what() << "\n";
        return 1;
    } catch (const soe::DisconnectedError& e) {
        std::cerr << "Disconnected: " << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
