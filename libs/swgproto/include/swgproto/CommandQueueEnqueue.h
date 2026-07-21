#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace swgproto {

// Builds the client->server "enqueue a command" message - the first outbound
// game-command message this project builds (everything before this has been
// either login/character/zone-setup traffic or pure decoding of server->client
// messages). This is how EVERY typed slash-command (movement aside) reaches
// the server: /teleport, /createcreature, /invulnerable, and ordinary
// gameplay commands alike all go through this same message, distinguished
// only by which command name hashes to `actionCRC`.
//
// Wire layout confirmed directly from Core3 source, but from TWO different
// places for two different reasons:
// - The shared `ObjectControllerMessage` envelope (opCount=0x05,
//   hash=hashCode("ObjControllerMessage"), header1=0x0B, header2=0x116,
//   objectId) is confirmed from the base `ObjectControllerMessage` C++
//   constructor. NOTE: unlike the server->client direction (where this
//   project's own `ObjControllerMessage::parse()` also consumes a trailing
//   unused int32 after objectId), the client->server envelope does NOT
//   include that trailing field - confirmed by live-testing against a real
//   Core3 server: including it shifted every downstream body field by 4
//   bytes, traced byte-for-byte from the server's logged (garbage)
//   actionCRC/actionCount/targetID values back to this project's own
//   remainingSize/actionCount/actionCRC fields.
// - CommandQueueEnqueue's own body (size/actionCount/actionCRC/targetID/
//   arguments) is confirmed from `CommandQueueEnqueueCallback::parse()` -
//   the server-side code that actually decodes real incoming client
//   packets - NOT from `CommandQueueEnqueue`'s own C++ constructor, which
//   writes a different, INCOMPLETE field set (missing `size` entirely) and
//   is confirmed dead code: grep of the whole Core3 source shows it's never
//   constructed anywhere, so it doesn't represent real wire traffic at all.
//   `size`'s exact meaning is genuinely unclear even in Core3's own source
//   (marked "//?" by its own author) - but it's read purely positionally and
//   never referenced anywhere in the callback's `run()` logic afterward, so
//   its value has zero effect on how the server processes the command; this
//   project fills it with the actual remaining-byte count as a defensible,
//   self-documenting choice, not because the server checks it.
//
// `actionCRC` is computed the same way as every other name-derived hash in
// this project (`soe::MessageHash::compute`) - confirmed directly from
// Core3's `CommandList::put()`/`getSlashCommand()`, which key their command
// table on `name.hashCode()`. Command names must be lowercase to match how
// Core3 registers them (e.g. `commandFactory.registerCommand<...>(String
// ("teleport").toLowerCase())`).
std::vector<uint8_t> buildCommandQueueEnqueue(uint64_t objectId, uint32_t actionCount,
                                               const std::string& commandName, uint64_t targetId,
                                               const std::u16string& arguments);

} // namespace swgproto
