#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"

namespace swgproto {

constexpr uint32_t kDataTransformControllerType = 0x71;

// A server-initiated position "synchronize" confirmation (an idle-state
// ping, not ongoing movement - see UpdateTransformMessage for the broadcast
// that covers real movement). Confirmed via Core3's DataTransform.h: the
// same wire shape is used bidirectionally (the real client sends this to
// report its own movement; the server sends one BACK, via
// creO->sendMessage(), when Transform::isSynchronizeUpdate() fires - which
// requires moveCount >= 50 and speed == 0 on both sides, i.e. "this object
// has been sitting still for a while, here's a confirmation"). CORRECTED
// live (2026-07-15): source reading alone suggested this send is
// necessarily client-specific ("self-only"), but real captured traffic
// shows objectIds OTHER than this connection's own character receiving the
// same treatment too (speed=0 in every live sample, consistent with the
// idle-confirmation theory) - most likely idle NPCs the server directly
// drives, though that's not yet confirmed by object-type tracking. Treat
// "self-only" as disproven; this is a general idle-sync mechanism, not
// exclusively about the owning client's own character.
//
// FIELD-COUNT AMBIGUITY, resolved in favor of what we actually receive:
// Core3's CLIENT-side parse function (Transform::parseDataTransform, used
// server-side to read what a real client sends) reads TWO leading uint32
// fields (timeStamp, then moveCount) before direction/position/speed. The
// SERVER's own outgoing constructor (used for exactly the synchronize case
// this client receives) inserts only ONE uint32 (movementCounter) before
// direction/position/speed - a real source-level asymmetry between the two
// directions, not a transcription error (both read directly from Core3's
// real source). This struct follows the CONSTRUCTOR's field count since
// that's the code path that actually builds what we receive; verified
// against real captured traffic (zero leftover bytes) before being trusted.
struct DataTransform {
    uint32_t counter = 0;
    // Raw quaternion components, wire insertion order X, Y, Z, W (not W
    // first).
    float directionX = 0.0f;
    float directionY = 0.0f;
    float directionZ = 0.0f;
    float directionW = 0.0f;
    // Wire order X, [height], [other horizontal] (same convention as
    // UpdateTransformMessage - see that header's own comment for the full
    // story on why the field names here are y=height/z=other-horizontal,
    // not a literal mirror of Core3's own Vector3 accessor names), but RAW
    // floats here, not scaled int16.
    float x = 0.0f;
    float y = 0.0f; // vertical height
    float z = 0.0f; // the other horizontal (north-south) coordinate
    float speed = 0.0f;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed). Wire layout:
    // uint32 counter + float dirX,dirY,dirZ,dirW + float x,(height),
    // (other horizontal) + float speed.
    static DataTransform parse(soe::PacketBuffer& buf);
};

// Builds the CLIENT->SERVER outbound shape (Phase 13, player movement) - a
// DIFFERENT, longer wire shape than the struct above, which decodes the
// SERVER's own shorter idle-sync broadcast (single `counter` field). The
// real shape a client sends is confirmed directly from Core3's own
// Transform::parseDataTransform() (server-side code that reads what a real
// client sends): uint32 timeStamp + uint32 moveCount + float
// dirX,dirY,dirZ,dirW (quaternion) + float x,(height),(other horizontal)
// (this project's field convention already matches real wire order here -
// see UpdateTransformMessage.h's own comment for the full y/z story) +
// float speed. Envelope mirrors buildCommandQueueEnqueue()'s
// already-proven pattern (opCount=0x05, hash("ObjControllerMessage"),
// header1=0x0B, header2=kDataTransformControllerType, objectId), confirmed
// live against a real Core3 server to need no trailing unused field on the
// client->server direction (unlike this project's own ObjControllerMessage
// parser for the server->client direction).
std::vector<uint8_t> buildDataTransform(uint64_t objectId, uint32_t timeStamp,
                                         uint32_t moveCount, float dirX, float dirY,
                                         float dirZ, float dirW, float x, float y, float z,
                                         float speed);

} // namespace swgproto
