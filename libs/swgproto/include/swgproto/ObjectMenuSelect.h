#pragma once

#include <cstdint>
#include <vector>

namespace swgproto {

// The message hash Core3 dispatches this on
// (ZonePacketHandler.cpp: registerObject<ObjectMenuSelectCallback>(0x7CA18726)),
// registered as a raw hex literal rather than derived from
// hashCode("ObjectMenuSelect") - the two are NOT the same value (confirmed:
// hashCode("ObjectMenuSelect") computes to something else entirely), so this
// constant must be used as-is rather than computed from the class name like
// most other messages in this project.
constexpr uint32_t kObjectMenuSelectHash = 0x7CA18726;

// Builds the opCount(2)+hash(4)+fields payload for ObjectMenuSelect, the
// client->server reply to a real right-click radial menu (the outbound half
// of ObjectMenuResponse, which this project could already decode but had no
// way to answer). Wire layout - opCount=3 (confirmed from a real capture,
// including the raw opCount field itself - not just inferred by convention),
// uint64 objectId (the same value as ObjectMenuResponse's own `target`
// field), uint8 radialId (the selected item's own `radialId`, not its
// `itemIndex`) - confirmed two ways at once from a real Phase 17 capture: it
// byte-for-byte matches Core3's ObjectMenuSelectCallback::parse()
// (server/zone/packets/object/ObjectMenuSelect.h: parseLong() then
// parseByte()), AND every one of 4 real captured samples decodes to an
// (objectId, radialId) pair matching an ObjectMenuResponse item seen earlier
// in the SAME session - e.g. selecting "@elevator_text:up" (radialId=198)
// against the real elevator terminal objectId, or "@guild:menu_create"
// (radialId=185) against the real guild terminal objectId. This is a
// genuinely new message this project didn't have before - nothing sends a
// menu selection back to the server without it, which blocks not just
// elevators but every terminal-driven interaction (and, eventually,
// crafting).
std::vector<uint8_t> buildObjectMenuSelect(uint64_t objectId, uint8_t radialId);

} // namespace swgproto
