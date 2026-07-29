// The crude wireframe/real-mesh visualizer - Windows/Vulkan only (see
// libs/renderer). Extracted out of main.cpp 2026-07-18 (was ~640 lines
// embedded in a 2200+-line file, the single largest concentration of
// application-level logic in tools/dummyclient) so future rendering work
// (terrain, SUI-driven UI) has somewhere bounded to grow instead of piling
// onto an already-large main.cpp. No behavior changed by this move - see
// SESSION_LOG.md for the extraction itself; every comment below predates
// the move and still describes the same code it always did.
#ifdef _WIN32

#include "Visualizer.h"

#include "AnimationDebugControls.h"
#include "AnimationDiagnosticLogging.h"
#include "PngWriter.h"
#include "creatureanim/AnimationStateSelection.h"
#include "RestPoseAutoTest.h"
#include "ScreenshotCapture.h"
#include "StringUtil.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "animation/SkeletalPose.h"
#include "assets/AnimationClip.h"
#include "assets/AnimationStateTable.h"
#include "assets/AppearanceTemplate.h"
#include "assets/BuildingLayout.h"
#include "assets/DdsImage.h"
#include "assets/LodFile.h"
#include "assets/SharedObjectTemplate.h"
#include "assets/ShaderTemplate.h"
#include "assets/SkeletalAppearance.h"
#include "assets/SkeletalMesh.h"
#include "assets/SkeletalMeshLod.h"
#include "assets/Skeleton.h"
#include "assets/StaticMesh.h"
#include "assets/TreArchive.h"
#include "renderer/Camera.h"
#include "renderer/VulkanRenderer.h"
#include "renderer/Window.h"
#include "soe/MessageDispatcher.h"
#include "soe/MessageHash.h"
#include "soe/SoeSession.h"
#include "soe/ThreadSafeQueue.h"
#include "swgproto/DataTransform.h"
#include "swgproto/DataTransformWithParent.h"
#include "swgproto/ObjControllerDispatcher.h"
#include "swgproto/ObjectMenuSelect.h"
#include "terrain/Layer.h"
#include "terrain/ProceduralTerrainSource.h"
#include "terrain/TerrainChunkManager.h"
#include "terrain/TerrainGenerator.h"
#include "terrain/TerrainMesh.h"
#include "worldmodel/ObjectStore.h"
#include "worldmodel/PortalVisibility.h"

namespace {

// A rough per-type box size (world units, roughly meters) for the crude
// visualizer - this project has never decoded real model/bounding-box
// dimensions (that's asset-pipeline work, explicitly out of scope for a
// wireframe proof-of-pipeline), so these are hand-picked, honestly
// approximate placeholders, not derived from anything real.
DirectX::XMFLOAT3 visualizerBoxHalfExtentsFor(worldmodel::ObjectTypeTag tag) {
    using worldmodel::ObjectTypeTag;
    switch (tag) {
        case ObjectTypeTag::Creature:
        case ObjectTypeTag::Player:
            return {0.35f, 0.9f, 0.35f}; // tall and narrow - roughly human-sized, standing
        case ObjectTypeTag::Installation:
        case ObjectTypeTag::Static:
            return {5.0f, 2.0f, 5.0f}; // wide, flat footprint - buildings/harvesters/turrets
        case ObjectTypeTag::ResourceContainer:
            return {0.3f, 0.3f, 0.3f};
        case ObjectTypeTag::FactoryCrate:
            return {0.5f, 0.35f, 0.5f}; // crate - wider than tall
        case ObjectTypeTag::Weapon:
            return {0.1f, 0.1f, 0.4f}; // thin and long
        case ObjectTypeTag::Tangible:
        case ObjectTypeTag::Intangible:
        case ObjectTypeTag::Unknown:
        default:
            return {0.5f, 0.5f, 0.5f};
    }
}

DirectX::XMFLOAT4 visualizerColorFor(worldmodel::ObjectTypeTag tag, bool isSelf) {
    using worldmodel::ObjectTypeTag;
    if (isSelf) {
        return {0.2f, 0.9f, 0.9f, 1.0f}; // cyan - stands out regardless of type
    }
    switch (tag) {
        case ObjectTypeTag::Creature:
            return {0.2f, 0.9f, 0.2f, 1.0f}; // green
        case ObjectTypeTag::Player:
            return {0.5f, 0.9f, 0.9f, 1.0f};
        case ObjectTypeTag::Installation:
            return {0.9f, 0.5f, 0.1f, 1.0f}; // orange
        case ObjectTypeTag::ResourceContainer:
            return {0.7f, 0.3f, 0.9f, 1.0f}; // purple
        case ObjectTypeTag::FactoryCrate:
            return {0.6f, 0.4f, 0.2f, 1.0f}; // brown
        case ObjectTypeTag::Weapon:
            return {0.9f, 0.9f, 0.2f, 1.0f}; // yellow
        case ObjectTypeTag::Static:
            return {0.9f, 0.9f, 0.9f, 1.0f}; // white
        case ObjectTypeTag::Tangible:
            return {0.6f, 0.6f, 0.6f, 1.0f}; // gray
        case ObjectTypeTag::Intangible:
            return {0.8f, 0.8f, 0.95f, 1.0f};
        case ObjectTypeTag::Unknown:
        default:
            return {1.0f, 0.1f, 0.1f, 1.0f}; // red - stands out as unexpected
    }
}

// Phase 19 - the debug per-type tint above (green creatures, orange
// installations, etc.) exists to distinguish UNTEXTURED objects from each
// other; multiplying a real, resolved texture by a saturated tint would
// discolor it (a real building rendering green-ish, purple-ish, etc.), which
// defeats the whole point of adding real textures. A textured submesh draws
// with neutral white instead - `texel.rgb * white == texel.rgb` in
// Mesh.hlsl's PSMain, i.e. the real texture shows through unmodified (still
// shaded by the existing half-lambert lighting term). An untextured submesh
// (or the placeholder wireframe box) keeps the debug tint exactly as before.
DirectX::XMFLOAT4 colorForSubmesh(const renderer::MeshHandle& handle,
                                   const DirectX::XMFLOAT4& debugTint) {
    return handle.textureDescriptorSet != VK_NULL_HANDLE
               ? DirectX::XMFLOAT4{1.0f, 1.0f, 1.0f, 1.0f}
               : debugTint;
}

// Resolves an object's real world-space position, accounting for cell-
// relative containment (Phase 17 Step 3 - fixes a real, previously-latent
// bug: this render loop used to treat every object's raw x/y/z as
// world-space unconditionally, regardless of parentId, confirmed via a
// direct grep finding zero occurrences of parentId anywhere in this file
// before this fix). If parentId is set, the object's own x/y/z are LOCAL
// coordinates within its containing building's shared coordinate space -
// NOT a further-nested per-cell transform - confirmed via a real Phase 17
// capture: self's cell-relative coordinates stayed within the same small,
// shared numeric range across every distinct cell of the same building
// (e.g. x/z roughly [-20, 20], y banded by floor), rather than each cell
// having its own independent local origin. Resolved by walking
// parentId (the containing cell's objectId) -> that cell's own containerId
// (Step 2 - the building's objectId) -> the building's own already-tracked
// world position/yaw (the same real position Phase 16 already draws the
// building's mesh at), then rotating+translating the local offset into
// world space using the identical yaw convention predictedSelfPos's own
// facing-vector math uses (forward = (sin(yaw), cos(yaw))). Returns
// nullopt if any link in that chain is missing (parentId!=0 but the cell
// isn't tracked, the cell's own containerId hasn't arrived yet, or the
// building's own position isn't resolved yet) - a REAL, live-caught bug
// fixed here: this used to silently fall back to treating LOCAL x/y/z as
// world-space directly, which is nonsensically wrong (not "close but off",
// actual local coordinates are tiny numbers like (-0.05, 2.75, 11.48) -
// treating those as world-space places the object near the world ORIGIN,
// nowhere near any loaded terrain, looking like it's "ignoring terrain
// height entirely"). Confirmed live: a cell's own UpdateContainmentMessage
// can genuinely never arrive in a given session (observed for the real
// elevator cell after walking in live rather than inheriting a
// server-side position) even though Step 2's permanent-registration fix
// means it isn't a forwarding-kill bug - some cells just don't get resent
// once already established before this client discovered them. Callers
// must handle nullopt by holding the LAST known-good position instead of
// guessing - the same "degrade gracefully, don't guess" bar buildingCache's
// own resolution failures use, just correctly extended to include "we
// don't know yet," not only "we know it isn't a building."
//
// Takes a pre-taken WorldObject SNAPSHOT (ObjectStore::snapshotWorldObjects()),
// NOT the live ObjectStore itself - a real, live-caught bug (Release build:
// "resource deadlock would occur"; Debug build: silently hung) from calling
// ObjectStore::find() for the cell/building lookups below from inside an
// ObjectStore::forEach() callback, which re-locks that store's own
// non-recursive mutex on the same thread it's already held on. Every call
// site takes one snapshot per frame before any forEach() pass, not a fresh
// store lookup per object.
std::optional<DirectX::XMFLOAT3> resolveWorldPosition(
    const std::unordered_map<uint64_t, worldmodel::WorldObject>& snapshot,
    const worldmodel::WorldObject& obj) {
    // Real, live-caught gap (Phase 17): a STATIONARY object (a terminal,
    // never moving) never receives any spatial transform message at all -
    // parentId (set only by applyTransform()/applyTransformWithParent()/
    // applyDataTransform()/applyDataTransformWithParent(), all real-movement
    // or idle-sync-of-a-moving-object messages) simply stays 0 forever for
    // it. Its ONLY containment signal is the logical one
    // (UpdateContainmentMessage -> containerId, pointing straight at its
    // cell) - GAP_ANALYSIS.md already warned these are different real
    // mechanisms, but this function used to check parentId exclusively,
    // silently treating a stationary object's real LOCAL coordinates as
    // world-space (placing it wherever happened to be near world origin,
    // nowhere near its real building - confirmed live: terminals appeared
    // to spawn near self instead of near the real house). Falls back to
    // containerId as the "effective parent" only when parentId itself is
    // unset, so a real spatial parent (self, or anything that DOES move)
    // still takes priority.
    uint64_t effectiveParentId = obj.parentId != 0 ? obj.parentId : obj.containerId;
    if (effectiveParentId == 0) {
        return DirectX::XMFLOAT3{obj.x, obj.y, obj.z};
    }
    auto cellIt = snapshot.find(effectiveParentId);
    if (cellIt == snapshot.end() || cellIt->second.containmentMessagesSeen == 0 ||
        cellIt->second.containerId == 0) {
        return std::nullopt;
    }
    auto buildingIt = snapshot.find(cellIt->second.containerId);
    if (buildingIt == snapshot.end() || buildingIt->second.transformMessagesSeen == 0) {
        return std::nullopt;
    }
    const worldmodel::WorldObject& building = buildingIt->second;
    float yaw = (static_cast<float>(building.direction) / 100.0f) * DirectX::XM_2PI;
    float cosYaw = std::cos(yaw);
    float sinYaw = std::sin(yaw);
    return DirectX::XMFLOAT3{building.x + obj.x * cosYaw + obj.z * sinYaw, building.y + obj.y,
                              building.z - obj.x * sinYaw + obj.z * cosYaw};
}

// Phase 17 Step 5 - real mouse-cursor picking. Unprojects the cursor's
// screen-space position into a world-space ray (origin + normalized
// direction) using the same view/projection matrices the frame is rendered
// with. Two points (near/far plane) are unprojected and the ray direction
// is their difference - the standard technique DirectXMath's own
// XMVector3Unproject() is designed for. World matrix is identity (every
// object position this project tracks is already in world space by the
// time it reaches here).
void screenPointToWorldRay(int screenX, int screenY, int viewportWidth, int viewportHeight,
                            const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection,
                            DirectX::XMFLOAT3& outOrigin, DirectX::XMFLOAT3& outDirection) {
    DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
    DirectX::XMVECTOR nearPoint = DirectX::XMVector3Unproject(
        DirectX::XMVectorSet(static_cast<float>(screenX), static_cast<float>(screenY), 0.0f, 0.0f),
        0.0f, 0.0f, static_cast<float>(viewportWidth), static_cast<float>(viewportHeight), 0.0f, 1.0f,
        projection, view, world);
    DirectX::XMVECTOR farPoint = DirectX::XMVector3Unproject(
        DirectX::XMVectorSet(static_cast<float>(screenX), static_cast<float>(screenY), 1.0f, 0.0f),
        0.0f, 0.0f, static_cast<float>(viewportWidth), static_cast<float>(viewportHeight), 0.0f, 1.0f,
        projection, view, world);
    DirectX::XMStoreFloat3(&outOrigin, nearPoint);
    DirectX::XMVECTOR dir =
        DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(farPoint, nearPoint));
    DirectX::XMStoreFloat3(&outDirection, dir);
}

// Ray-vs-sphere test, returning the near intersection distance along the
// ray if it exists and is closer than `maxDistance` (which callers shrink
// progressively as closer hits are found, naturally converging on the
// nearest one across a whole-scene scan), else nullopt. Objects have no
// real collision geometry in this project (explicitly out of scope per the
// Phase 17 plan), so picking uses a fixed-radius sphere centered on each
// object's resolved world position - good enough to click on it, not real
// hit-testing against mesh triangles.
std::optional<float> raySphereIntersect(const DirectX::XMFLOAT3& rayOrigin,
                                         const DirectX::XMFLOAT3& rayDir,
                                         const DirectX::XMFLOAT3& sphereCenter, float radius,
                                         float maxDistance) {
    DirectX::XMVECTOR o = DirectX::XMLoadFloat3(&rayOrigin);
    DirectX::XMVECTOR d = DirectX::XMLoadFloat3(&rayDir);
    DirectX::XMVECTOR c = DirectX::XMLoadFloat3(&sphereCenter);
    DirectX::XMVECTOR l = DirectX::XMVectorSubtract(c, o);
    float tca = DirectX::XMVectorGetX(DirectX::XMVector3Dot(l, d));
    if (tca < 0.0f) {
        return std::nullopt;
    }
    float d2 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(l, l)) - tca * tca;
    float r2 = radius * radius;
    if (d2 > r2) {
        return std::nullopt;
    }
    float thc = std::sqrt(r2 - d2);
    float t0 = tca - thc;
    if (t0 < 0.0f) {
        t0 = tca + thc;
    }
    if (t0 < 0.0f || t0 > maxDistance) {
        return std::nullopt;
    }
    return t0;
}

// Real elevator terminal templates carry their direction right in the
// filename (terminal_elevator_up.iff / terminal_elevator_down.iff -
// confirmed in RealMeshResolver's bundled candidate list) - no separate
// wire signal distinguishes them, so picking checks the resolved template
// PATH, not just objectCrc. The plain "terminal_elevator.iff" variant (no
// up/down suffix) is deliberately left unhandled: no real capture exists of
// its actual radialId, and this project doesn't guess wire values it
// hasn't seen (see the project's own "detect and stop, don't guess" rule).
enum class ElevatorDirection { None, Up, Down };

ElevatorDirection elevatorDirectionFor(const std::string* templatePath) {
    if (templatePath == nullptr) {
        return ElevatorDirection::None;
    }
    if (templatePath->find("terminal_elevator_up") != std::string::npos) {
        return ElevatorDirection::Up;
    }
    if (templatePath->find("terminal_elevator_down") != std::string::npos) {
        return ElevatorDirection::Down;
    }
    return ElevatorDirection::None;
}

// Extracts whatever name field a given stored type actually has, mirroring
// dumpObjectStoreSummary()'s own per-type field dispatch exactly (same
// customObjectName/resourceName locations) - this is deliberately NOT a new
// lookup, just reused for a label instead of a console line. Returns nullopt
// for types with no meaningful display name (CellObject/GroupObject aren't
// even drawn, so they never reach this) or whose baseline hasn't decoded yet.
template <typename T>
std::optional<std::u16string> labelTextFor(const T& obj) {
    using worldmodel::CreatureObject;
    using worldmodel::FactoryCrate;
    using worldmodel::InstallationObject;
    using worldmodel::IntangibleObject;
    using worldmodel::PlayerObject;
    using worldmodel::ResourceContainer;
    using worldmodel::StaticObject;
    using worldmodel::TangibleObject;
    using worldmodel::WeaponObject;

    if constexpr (std::is_same_v<T, TangibleObject> || std::is_same_v<T, WeaponObject> ||
                  std::is_same_v<T, IntangibleObject> || std::is_same_v<T, FactoryCrate> ||
                  std::is_same_v<T, StaticObject>) {
        if (obj.base3.has_value() && !obj.base3->customObjectName.empty()) {
            return obj.base3->customObjectName;
        }
    } else if constexpr (std::is_same_v<T, CreatureObject>) {
        if (obj.base3.has_value() && !obj.base3->tangible.customObjectName.empty()) {
            return obj.base3->tangible.customObjectName;
        }
    } else if constexpr (std::is_same_v<T, PlayerObject>) {
        if (obj.base3.has_value() && !obj.base3->intangible.customObjectName.empty()) {
            return obj.base3->intangible.customObjectName;
        }
    } else if constexpr (std::is_same_v<T, InstallationObject>) {
        if (obj.base3.has_value() && !obj.base3->tangible.customObjectName.empty()) {
            return obj.base3->tangible.customObjectName;
        }
    } else if constexpr (std::is_same_v<T, ResourceContainer>) {
        if (obj.base6.has_value() && !obj.base6->resourceName.empty()) {
            return obj.base6->resourceName;
        }
    }
    return std::nullopt;
}

// Widens a u16string field straight to wchar_t for GDI (both are 16-bit on
// Windows) - element-wise, matching toUtf8Preview()'s own "this project only
// ever sees ASCII names" assumption, just preserving the full 16 bits
// instead of truncating to char.
std::wstring toWString(const std::u16string& s) {
    std::wstring out;
    out.reserve(s.size());
    for (char16_t ch : s) {
        out.push_back(static_cast<wchar_t>(ch));
    }
    return out;
}

// Real archive files (and, as of 2026-07-18's deed-CRC investigation,
// objectCrc itself) use a "shared_" filename prefix that Core3's own
// Lua-registered template path strings do not include. Originally believed
// to only matter for locating the file inside the .tre archives (see
// RealMeshResolver::resolveUncached() below) - proven wrong by the
// deed-placed-structure investigation: a live-captured objectCrc for a real
// player guild hall (0x6ef998e2) only matches
// soe::MessageHash::compute("object/building/player/shared_player_guildhall_generic_style_01.iff"),
// NOT the un-prefixed form. This means every candidate hashed by
// RealMeshResolver's constructor before this fix was hashed WRONG - not a
// guild-hall-specific bug, a universal one affecting every category's live
// resolution.
std::string toSharedTemplatePath(const std::string& templatePath) {
    size_t slash = templatePath.rfind('/');
    std::string dir = (slash == std::string::npos) ? "" : templatePath.substr(0, slash + 1);
    std::string base = (slash == std::string::npos) ? templatePath : templatePath.substr(slash + 1);
    return dir + "shared_" + base;
}

// One label texture per distinct name string, created on first sight via
// VulkanRenderer::createTextTexture() and reused every frame after - avoids
// re-rasterizing the same name through GDI every single frame.
struct LabelTexture {
    renderer::TextureHandle texture;
    float aspectRatio = 1.0f; // pixelWidth / pixelHeight, for sizing the world-space quad
};

// One real shader-group submesh's geometry plus its resolved real texture
// (Phase 19) - `texture` is nullopt whenever the submesh's own shaderFilename
// is empty, or ANY step of shader/texture resolution fails (missing .sht,
// no MAIN slot, missing .dds, unsupported DDS variant) - never a hard
// failure for the whole submesh, matching this project's established
// per-part skip/fallback convention. Callers upload `mesh` unconditionally
// and `texture` only when present, falling back to the renderer's shared
// white texture otherwise (see MeshHandle::textureDescriptorSet's comment).
struct ResolvedSubmesh {
    assets::MeshData mesh;
    std::optional<assets::DdsImageData> texture;
};

// Resolves one real submesh's shaderFilename all the way to real decoded
// texture bytes: .sht (assets::ShaderTemplate, MAIN texture slot) -> .dds
// (assets::DdsImage). `tryExtract` is injected rather than this function
// owning any archive itself, since RealMeshResolver/RealBuildingResolver
// each already have their own private tryExtract() searching their own
// distinct archive lists - this avoids duplicating that logic a third time.
std::optional<assets::DdsImageData> resolveSubmeshTexture(
    const std::string& shaderFilename,
    const std::function<std::optional<std::vector<uint8_t>>(const std::string&)>& tryExtract) {
    if (shaderFilename.empty()) {
        return std::nullopt;
    }
    auto shtBytes = tryExtract(shaderFilename);
    if (!shtBytes.has_value()) {
        return std::nullopt;
    }
    assets::ShaderTemplateData sht;
    try {
        sht = assets::ShaderTemplate::parse(*shtBytes);
    } catch (const std::exception&) {
        return std::nullopt;
    }
    if (sht.mainTextureFilename.empty()) {
        return std::nullopt;
    }
    auto ddsBytes = tryExtract(sht.mainTextureFilename);
    if (!ddsBytes.has_value()) {
        return std::nullopt;
    }
    try {
        return assets::DdsImage::parse(*ddsBytes);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// A cell's real axis-aligned bounding box, in the building's own local mesh
// space (the same coordinate space every real cell's raw vertex data is
// already authored in - confirmed no per-cell transform exists, see
// BuildingLayout.h's own comment) - the union of every one of the cell's
// real submeshes' own vertex extents. Computed once at resolve time
// (cheap - the vertex data is already being walked to upload it) and used
// to let self's OWN locally-known position determine which real room it's
// standing in, instead of waiting on a server-reported cellNumber that (a
// real, live-caught gap this session) turns out to only ever update right
// at zone-in or after a genuine server-driven relocation (an elevator
// ride) - ordinary walking between adjacent rooms produces no such update
// at all, since the server has no reason to echo a client's own predicted
// movement back to that same client.
struct CellBounds {
    DirectX::XMFLOAT3 min{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 max{0.0f, 0.0f, 0.0f};
};

std::optional<CellBounds> computeCellBounds(const std::vector<ResolvedSubmesh>& submeshes) {
    std::optional<CellBounds> bounds;
    for (const auto& submesh : submeshes) {
        for (const auto& p : submesh.mesh.positions) {
            if (!bounds.has_value()) {
                bounds = CellBounds{{p.x, p.y, p.z}, {p.x, p.y, p.z}};
                continue;
            }
            bounds->min.x = std::min(bounds->min.x, p.x);
            bounds->min.y = std::min(bounds->min.y, p.y);
            bounds->min.z = std::min(bounds->min.z, p.z);
            bounds->max.x = std::max(bounds->max.x, p.x);
            bounds->max.y = std::max(bounds->max.y, p.y);
            bounds->max.z = std::max(bounds->max.z, p.z);
        }
    }
    return bounds;
}

// One real cell's resolved submeshes plus its own real bounds (see
// CellBounds' own comment) - the unit RealBuildingResolver now resolves per
// cell, and BuildingHandleCache uploads per cell.
//
// collisionMesh (Phase 20) is the real inline CMSH collision geometry
// (assets::BuildingLayout - see BuildingCell::collisionMesh's own comment)
// - deliberately kept as plain CPU-side data here, never uploaded to the
// GPU (unlike `submeshes`), since collision queries run on the CPU every
// frame. May be empty (zero positions) if the real cell had no CMSH data -
// callers must treat that as "no collision geometry for this cell" and
// fall back gracefully, never a hard failure.
struct ResolvedCell {
    std::vector<ResolvedSubmesh> submeshes;
    std::optional<CellBounds> bounds;
    assets::MeshData collisionMesh;

    // Real portal data (Phase 20b) - this cell's own portal placements
    // (assets::BuildingCell::portals) plus a full copy of the building-wide
    // portalShapes list (assets::BuildingLayoutData::portalShapes,
    // duplicated per cell rather than plumbed through as a separate
    // building-level return value - simpler, and negligible cost: at most
    // a few dozen small polygons per building). See
    // assets::BuildingCell::portals' own comment for the real format.
    std::vector<assets::CellPortal> portals;
    std::vector<assets::PortalShape> portalShapes;

    // Real dedicated floor-collision navmesh (Phase 20c) - see
    // assets::FloorCollisionMesh's own comment. May be empty (zero
    // positions) if the real cell had no real floor-collision filename, or
    // the file failed to load/parse - callers must fall back to
    // collisionMesh's own raycast query for that cell, never a hard
    // failure.
    assets::FloorCollisionMesh floorMesh;
};

// Transforms a world-space position into ONE specific building instance's
// own local mesh space - the exact inverse of the rotate-then-translate
// convention resolveWorldPosition() already uses to place cell-relative
// objects in world space, and the same formula already proven correct by
// the outbound-movement code's own local-position encoding (see its
// comment) - not a new, unverified transform.
DirectX::XMFLOAT3 worldToBuildingLocal(const DirectX::XMFLOAT3& worldPos,
                                        const worldmodel::WorldObject& building) {
    float buildingYaw = (static_cast<float>(building.direction) / 100.0f) * DirectX::XM_2PI;
    float cosYaw = std::cos(buildingYaw);
    float sinYaw = std::sin(buildingYaw);
    float dx = worldPos.x - building.x;
    float dz = worldPos.z - building.z;
    return DirectX::XMFLOAT3{dx * cosYaw - dz * sinYaw, worldPos.y - building.y,
                              dx * sinYaw + dz * cosYaw};
}

// The exact inverse of worldToBuildingLocal() above - rotating a
// building-local offset back by -buildingYaw (a rotation matrix's inverse
// is its transpose) then re-adding the building's own world position.
// Real portal-visibility use (Phase 22): real `.pob` portal vertices are
// building-local, but the frustum visibility test needs world space.
DirectX::XMFLOAT3 buildingLocalToWorld(const DirectX::XMFLOAT3& localPos,
                                        const worldmodel::WorldObject& building) {
    float buildingYaw = (static_cast<float>(building.direction) / 100.0f) * DirectX::XM_2PI;
    float cosYaw = std::cos(buildingYaw);
    float sinYaw = std::sin(buildingYaw);
    float dx = localPos.x * cosYaw + localPos.z * sinYaw;
    float dz = -localPos.x * sinYaw + localPos.z * cosYaw;
    return DirectX::XMFLOAT3{building.x + dx, building.y + localPos.y, building.z + dz};
}

// Resolves a real objectCrc (from SceneCreateObjectByCrc, now stored on
// WorldObject - see its own comment) to real uploaded mesh geometry,
// walking the chain this session discovered and verified against real
// data: .iff (SharedObjectTemplate) -> .apt (AppearanceTemplate) -> .lod
// (LodFile, the common case) -> .msh (StaticMesh). Deliberately searches a
// small, FIXED set of client archives (the base archives plus data_sku1_*,
// not every patch archive on the machine) - matches this pass' "minimal"
// scope; content only present in later content patches simply won't
// resolve, falling back to the existing wireframe placeholder box like any
// other resolution failure, never an error that stops rendering. The
// candidate list covers static/tangible/installation/building/intangible/
// weapon/resource_container/factory templates - explicitly NOT mobile/
// creature (skeletal meshes are out of scope this pass, per StaticMesh's
// own name).
class RealMeshResolver {
public:
    explicit RealMeshResolver(const std::string& clientPath) {
        static const char* kArchiveNames[] = {
            "data_other_00.tre",
            "data_static_mesh_00.tre",
            "data_static_mesh_01.tre",
            "data_sku1_00.tre",
            "data_sku1_01.tre",
            "data_sku1_02.tre",
            "data_sku1_03.tre",
            "data_sku1_04.tre",
            "data_sku1_05.tre",
            "data_sku1_06.tre",
            "data_sku1_07.tre",
            // Real .dds texture files (Phase 19) - .sht shader files
            // themselves live in data_other_00.tre above (already opened),
            // but the .dds textures they reference live in these separate
            // archives, confirmed via a direct header dump this session.
            "data_texture_00.tre",
            "data_texture_01.tre",
            "data_texture_02.tre",
            "data_texture_03.tre",
            "data_texture_04.tre",
            "data_texture_05.tre",
            "data_texture_06.tre",
            "data_texture_07.tre",
        };
        for (const char* name : kArchiveNames) {
            std::string path = clientPath + "\\" + name;
            try {
                archives_.push_back({name, assets::TreArchive(path)});
                std::cout << "[MESH] opened " << path << " (" << archives_.back().archive.fileCount()
                           << " files)\n";
            } catch (const std::exception& e) {
                std::cerr << "[MESH] failed to open " << path << ": " << e.what() << "\n";
            }
        }

        // Bundled, mechanically-extracted list of real object .iff template
        // paths (see libs/assets/data/object_template_candidates.txt) -
        // a KNOWN, bounded set (~10,682 paths across static/tangible/
        // installation/building/intangible/weapon/resource_container/
        // factory - explicitly NOT mobile/creature, which need skeletal
        // mesh support this pass doesn't have), not a generic reverse-hash
        // resolver. Hashed once here via the exact same CRC-32 algorithm
        // Core3 uses server-side for objectCrc (soe::MessageHash::compute(),
        // confirmed identical to SharedObjectTemplate::getClientObjectCRC()
        // by reading the Core3 reference source directly).
        std::ifstream listFile(std::string(SWG_ASSETS_DATA_DIR) + "/object_template_candidates.txt");
        if (listFile.is_open()) {
            std::string line;
            while (std::getline(listFile, line)) {
                if (line.empty()) {
                    continue;
                }
                // Hash the "shared_"-prefixed form, not the raw Lua-registered
                // path - see toSharedTemplatePath()'s comment for why. The map
                // still stores the RAW path as the value; resolveUncached()
                // re-derives the shared_ form from it for archive lookup.
                uint32_t crc = soe::MessageHash::compute(toSharedTemplatePath(line));
                crcToTemplatePath_[crc] = line;
            }
        }
        std::cout << "[MESH] loaded " << crcToTemplatePath_.size()
                   << " candidate template paths\n";
    }

    // Pure-CPU resolution (Phase 14) - archive extraction + IFF/APT/LOD/MSH
    // parsing, no GPU upload. Callable from the asset worker thread: only
    // touches archives_/crcToTemplatePath_, both immutable after this
    // object's constructor finishes, so a single worker thread calling this
    // repeatedly (never concurrently with itself, and never from the render
    // thread once Phase 14's split is in place) is safe with no locking.
    // Throws on any resolution failure - caller (the worker thread) catches
    // and reports a failed MeshReady, same graceful-degradation-to-
    // placeholder-box behavior as before, just relocated.
    std::vector<ResolvedSubmesh> resolveMeshDataOnly(uint32_t objectCrc) {
        auto candidateIt = crcToTemplatePath_.find(objectCrc);
        if (candidateIt == crcToTemplatePath_.end()) {
            throw std::runtime_error("objectCrc not in the known candidate list");
        }
        const std::string& templatePath = candidateIt->second;
        std::string realIffPath = toSharedTemplatePath(templatePath);

        auto iffBytes = tryExtract(realIffPath);
        if (!iffBytes.has_value()) {
            throw std::runtime_error("template file not found: " + realIffPath);
        }
        auto sharedTemplate = assets::SharedObjectTemplate::parse(*iffBytes);

        auto aptBytes = tryExtract(sharedTemplate.appearanceFilename);
        if (!aptBytes.has_value()) {
            throw std::runtime_error("appearance file not found: " +
                                      sharedTemplate.appearanceFilename);
        }
        auto appearance = assets::AppearanceTemplate::parse(*aptBytes);

        // The .apt's own reference was a .lod for every real case checked
        // this session, but tries the referenced file directly first in
        // case some templates point straight at a .msh instead.
        std::string meshRelativePath;
        if (appearance.referencedFilename.size() >= 4 &&
            appearance.referencedFilename.compare(appearance.referencedFilename.size() - 4, 4,
                                                    ".lod") == 0) {
            auto lodBytes = tryExtract(appearance.referencedFilename);
            if (!lodBytes.has_value()) {
                throw std::runtime_error("lod file not found: " + appearance.referencedFilename);
            }
            auto lod = assets::LodFile::parse(*lodBytes);
            meshRelativePath = "appearance/" + lod.highestDetailMeshFilename;
        } else {
            meshRelativePath = appearance.referencedFilename;
        }

        auto meshBytes = tryExtract(meshRelativePath);
        if (!meshBytes.has_value()) {
            throw std::runtime_error("mesh file not found: " + meshRelativePath);
        }
        // A real mesh almost always has more than one real shader-group
        // submesh (see StaticMeshSubmesh's own comment) - each one becomes
        // its own ResolvedSubmesh, with its own independently-resolved real
        // texture (Phase 19) via resolveSubmeshTexture() - a submesh whose
        // texture fails to resolve still renders (untextured fallback), it
        // just doesn't get one, matching this project's per-part tolerance.
        auto staticSubmeshes = assets::StaticMesh::parse(*meshBytes);
        std::vector<ResolvedSubmesh> result;
        result.reserve(staticSubmeshes.size());
        size_t totalVerts = 0;
        size_t totalTris = 0;
        size_t texturedCount = 0;
        for (auto& submesh : staticSubmeshes) {
            totalVerts += submesh.mesh.positions.size();
            totalTris += submesh.mesh.indices.size() / 3;
            ResolvedSubmesh resolved;
            resolved.texture = resolveSubmeshTexture(
                submesh.shaderFilename,
                [this](const std::string& name) { return tryExtract(name); });
            if (resolved.texture.has_value()) {
                ++texturedCount;
            }
            resolved.mesh = std::move(submesh.mesh);
            result.push_back(std::move(resolved));
        }

        std::cout << "[MESH] resolved objectCrc=0x" << std::hex << objectCrc << std::dec << " -> "
                   << meshRelativePath << " (" << result.size() << " submeshes, " << texturedCount
                   << " textured, " << totalVerts << " verts, " << totalTris << " tris)\n";

        return result;
    }

    // Cheap map lookup only (no archive I/O, no parsing) - safe to call from
    // the render thread every frame, unlike resolveMeshDataOnly() above.
    // Added for Phase 17 Step 5 (real mouse picking): identifying whether a
    // picked object is a known interactable (e.g. an elevator terminal)
    // needs its real template PATH, not just its objectCrc hash, and
    // crcToTemplatePath_ already has exactly that mapping built at
    // construction time.
    const std::string* templatePathFor(uint32_t objectCrc) const {
        auto it = crcToTemplatePath_.find(objectCrc);
        return it != crcToTemplatePath_.end() ? &it->second : nullptr;
    }

    // Exposes an already-open archive by name so other resolvers (e.g.
    // TerrainResolver, which also needs data_other_00.tre) can reuse it
    // instead of opening and parsing the same .tre a second time. nullptr
    // if that archive failed to open (see the constructor's own try/catch).
    const assets::TreArchive* findArchiveNamed(const std::string& name) const {
        for (const auto& named : archives_) {
            if (named.name == name) {
                return &named.archive;
            }
        }
        return nullptr;
    }

private:
    struct NamedArchive {
        std::string name;
        assets::TreArchive archive;
    };

    std::optional<std::vector<uint8_t>> tryExtract(const std::string& name) {
        for (auto& named : archives_) {
            if (named.archive.contains(name)) {
                return named.archive.extract(name);
            }
        }
        return std::nullopt;
    }

    std::vector<NamedArchive> archives_;
    std::unordered_map<uint32_t, std::string> crcToTemplatePath_;
};

// Resolves a real objectCrc to real BUILDING geometry (Phase 16) - walks a
// real, distinct chain from RealMeshResolver's own: .iff
// (SharedObjectTemplate) -> .pob (BuildingLayout, a "Portal Object" -
// FORM PRTO) -> one real .msh/.lod per cell (room). Real, confirmed-live
// discovery: buildings leave SharedObjectTemplateData::appearanceFilename
// genuinely EMPTY and set `portalLayoutFilename` instead - a real, direct
// signal in the template data itself (not a guessed type-tag check) for
// which chain to use. Reuses RealMeshResolver's ALREADY-OPEN archives
// (buildings/their cell meshes live in the exact same archive set - no
// second copy opened) and its own bundled candidate list convention
// (`object_template_candidates.txt` already covers "building" templates
// per RealMeshResolver's own doc comment - no separate archive-enumeration
// needed here, unlike RealSkeletalMeshResolver's creature/mobile case,
// which needed enumeration specifically because creatures were EXCLUDED
// from that bundled list).
class RealBuildingResolver {
public:
    explicit RealBuildingResolver(RealMeshResolver& meshResolver) {
        static const char* kArchiveNames[] = {
            "data_other_00.tre",
            "data_static_mesh_00.tre",
            "data_static_mesh_01.tre",
            "data_sku1_00.tre",
            "data_sku1_01.tre",
            "data_sku1_02.tre",
            "data_sku1_03.tre",
            "data_sku1_04.tre",
            "data_sku1_05.tre",
            "data_sku1_06.tre",
            "data_sku1_07.tre",
            "data_texture_00.tre",
            "data_texture_01.tre",
            "data_texture_02.tre",
            "data_texture_03.tre",
            "data_texture_04.tre",
            "data_texture_05.tre",
            "data_texture_06.tre",
            "data_texture_07.tre",
        };
        for (const char* name : kArchiveNames) {
            const assets::TreArchive* archive = meshResolver.findArchiveNamed(name);
            if (archive != nullptr) {
                archives_.push_back(archive);
            } else {
                std::cerr << "[BUILDING] archive not available (not opened by RealMeshResolver): "
                           << name << "\n";
            }
        }

        std::ifstream listFile(std::string(SWG_ASSETS_DATA_DIR) + "/object_template_candidates.txt");
        if (listFile.is_open()) {
            std::string line;
            while (std::getline(listFile, line)) {
                if (line.empty()) {
                    continue;
                }
                uint32_t crc = soe::MessageHash::compute(toSharedTemplatePath(line));
                crcToTemplatePath_[crc] = line;
            }
        }
        std::cout << "[BUILDING] loaded " << crcToTemplatePath_.size() << " candidate template paths\n";

        // Real terrain-grading map (Phase 22) - ground truth, not a
        // heuristic: extracted directly from every real, non-empty
        // `terrainModificationFileName` across Core3's own
        // `bin/scripts/object/building/**/objects.lua` (496 real
        // template->`.lay` pairs, only 7 distinct real `.lay` files -
        // confirmed live-caught bug session, 2026-07-28: an EARLIER
        // heuristic guess (matching by planet/size-token substring against
        // generic candidate filenames) applied grading to a player
        // guildhall, but Core3's own real source confirms every single
        // player house/guildhall template - every size, every planet, no
        // exceptions - has `terrainModificationFileName = ""`, and
        // `GroundZoneContainerComponent.cpp`'s own real gate
        // (`if (!modFile.isEmpty()) addTerrainModification(...)`) means the
        // real live game applies ZERO terrain grading to player
        // construction, period. Real grading is reserved for military
        // installations, starports, faction-perk HQs, and creature lairs -
        // never player housing. A template not present in this map (which
        // includes every real player house/guildhall) gets no grading at
        // all now, matching real behavior exactly instead of guessing.
        std::ifstream gradingMapFile(std::string(SWG_ASSETS_DATA_DIR) + "/real_terrain_grading_map.txt");
        if (gradingMapFile.is_open()) {
            std::string line;
            while (std::getline(gradingMapFile, line)) {
                if (line.empty()) {
                    continue;
                }
                size_t tab = line.find('\t');
                if (tab == std::string::npos) {
                    continue;
                }
                realGradingMap_[line.substr(0, tab)] = line.substr(tab + 1);
            }
        }
        std::cout << "[BUILDING] loaded " << realGradingMap_.size() << " real terrain-grading entries\n";
    }

    // Pure-CPU resolution, same threading contract as every other resolver
    // this session (RealMeshResolver/RealSkeletalMeshResolver) - callable
    // from the asset worker thread, throws on any resolution failure for
    // the caller to catch and fall back to the wireframe box. A real
    // building can have a cell whose own mesh fails to resolve without
    // failing the WHOLE building - skipped, not fatal, UNLESS every cell
    // fails, matching RealSkeletalMeshResolver's own per-body-part
    // tolerance.
    // Outer vector: one entry per real cell (same indexing the Phase 17/18
    // draw/filter loop relies on - `buildingCells->size()`/`cellIndex`
    // still mean the same thing). Inner vector: that cell's own real
    // shader-group submeshes, kept SEPARATE (not merged, unlike an earlier
    // version of this pass) so each one gets its own correctly-matched real
    // texture - merging would apply one submesh's texture to every OTHER
    // submesh's geometry too (e.g. a wall showing a floor's marble texture),
    // which is worse than no texture at all.
    std::vector<ResolvedCell> resolveMeshDataOnly(uint32_t objectCrc) {
        auto candidateIt = crcToTemplatePath_.find(objectCrc);
        if (candidateIt == crcToTemplatePath_.end()) {
            throw std::runtime_error("objectCrc not in the known candidate list");
        }
        const std::string& templatePath = candidateIt->second;
        std::string realIffPath = toSharedTemplatePath(templatePath);

        auto iffBytes = tryExtract(realIffPath);
        if (!iffBytes.has_value()) {
            throw std::runtime_error("template file not found: " + realIffPath);
        }
        auto sharedTemplate = assets::SharedObjectTemplate::parse(*iffBytes);
        if (sharedTemplate.portalLayoutFilename.empty()) {
            throw std::runtime_error("not a building (no portalLayoutFilename): " + realIffPath);
        }

        auto pobBytes = tryExtract(sharedTemplate.portalLayoutFilename);
        if (!pobBytes.has_value()) {
            throw std::runtime_error("portal layout file not found: " +
                                      sharedTemplate.portalLayoutFilename);
        }
        auto layout = assets::BuildingLayout::parse(*pobBytes);

        std::vector<ResolvedCell> result;
        result.reserve(layout.cells.size());
        for (const auto& cell : layout.cells) {
            std::vector<ResolvedSubmesh> cellSubmeshResults;
            try {
                // Cell 0 (the exterior shell) real-confirmed to reference a
                // .lod file; every other cell references a real .msh
                // directly - same generic branch RealMeshResolver already
                // uses for ordinary objects, just per-cell here.
                std::string meshPath;
                if (cell.modelFilename.size() >= 4 &&
                    cell.modelFilename.compare(cell.modelFilename.size() - 4, 4, ".lod") == 0) {
                    auto lodBytes = tryExtract(cell.modelFilename);
                    if (!lodBytes.has_value()) {
                        throw std::runtime_error("lod file not found: " + cell.modelFilename);
                    }
                    auto lod = assets::LodFile::parse(*lodBytes);
                    meshPath = "appearance/" + lod.highestDetailMeshFilename;
                } else {
                    meshPath = cell.modelFilename;
                }

                auto meshBytes = tryExtract(meshPath);
                if (!meshBytes.has_value()) {
                    throw std::runtime_error("cell mesh file not found: " + meshPath);
                }
                // A real cell mesh almost always has more than one real
                // shader-group submesh (confirmed live: the elevator cell
                // alone has 5, not the single 4-triangle platform earlier
                // logs showed) - each becomes its own ResolvedSubmesh with
                // its own independently-resolved real texture.
                auto cellSubmeshes = assets::StaticMesh::parse(*meshBytes);
                size_t totalVerts = 0;
                size_t totalTris = 0;
                size_t texturedCount = 0;
                for (auto& submesh : cellSubmeshes) {
                    totalVerts += submesh.mesh.positions.size();
                    totalTris += submesh.mesh.indices.size() / 3;
                    ResolvedSubmesh resolved;
                    resolved.texture = resolveSubmeshTexture(
                        submesh.shaderFilename,
                        [this](const std::string& name) { return tryExtract(name); });
                    if (resolved.texture.has_value()) {
                        ++texturedCount;
                    }
                    resolved.mesh = std::move(submesh.mesh);
                    cellSubmeshResults.push_back(std::move(resolved));
                }
                // TEMP diagnostic (Phase 17 live debug) - per-cell vertex/
                // triangle counts were never logged before (only the
                // resolved-cell COUNT), so a cell that "resolves" but
                // carries zero/degenerate geometry was invisible in every
                // prior log.
                std::cout << "[BUILDING]   cell \"" << cell.name << "\" -> " << meshPath << " ("
                           << cellSubmeshes.size() << " submeshes, " << texturedCount << " textured, "
                           << totalVerts << " verts, " << totalTris << " tris)\n";
            } catch (const std::exception& e) {
                std::cerr << "[BUILDING] cell \"" << cell.name << "\" failed, skipping: " << e.what()
                           << "\n";
            }
            ResolvedCell resolvedCell;
            resolvedCell.bounds = computeCellBounds(cellSubmeshResults);
            resolvedCell.submeshes = std::move(cellSubmeshResults);
            // Real collision-only geometry (Phase 20) - small (position +
            // index only), a plain copy from the already-parsed layout is
            // fine; `cell` is a const& into `layout.cells` so it can't be
            // moved from here.
            resolvedCell.collisionMesh = cell.collisionMesh;
            // TEMPORARY diagnostic (2026-07-28, ramp collision-gap
            // investigation) - real vertex extents of this cell's own CMSH
            // collision mesh, to check whether it actually covers the real
            // entrance ramp at all (self measured reaching local z~44,
            // vs. the VISUAL submesh bounds only reaching z~32.5) or
            // whether this is a genuine content gap - the collision mesh
            // never having ramp geometry at all, which no margin value
            // could ever work around. Strip once resolved.
            if (!resolvedCell.collisionMesh.positions.empty()) {
                assets::Float3 cmin = resolvedCell.collisionMesh.positions[0];
                assets::Float3 cmax = resolvedCell.collisionMesh.positions[0];
                for (const auto& p : resolvedCell.collisionMesh.positions) {
                    cmin.x = std::min(cmin.x, p.x);
                    cmin.y = std::min(cmin.y, p.y);
                    cmin.z = std::min(cmin.z, p.z);
                    cmax.x = std::max(cmax.x, p.x);
                    cmax.y = std::max(cmax.y, p.y);
                    cmax.z = std::max(cmax.z, p.z);
                }
                std::cout << "[COLLISION DIAG] cell \"" << cell.name << "\" CMSH real extents min=("
                           << cmin.x << "," << cmin.y << "," << cmin.z << ") max=(" << cmax.x << ","
                           << cmax.y << "," << cmax.z << ") vertCount="
                           << resolvedCell.collisionMesh.positions.size() << "\n";
            } else {
                std::cout << "[COLLISION DIAG] cell \"" << cell.name << "\" CMSH is EMPTY\n";
            }
            // Real portal data (Phase 20b) - same reasoning as
            // collisionMesh above (`cell`/`layout` are both const& here).
            resolvedCell.portals = cell.portals;
            resolvedCell.portalShapes = layout.portalShapes;
            // Real dedicated floor-collision navmesh (Phase 20c) - a
            // SEPARATE real file from the inline CMSH data (see
            // assets::FloorCollisionMesh's own comment for why this exists
            // alongside CMSH: it fixes a real switchback-staircase height
            // ambiguity CMSH's own single-raycast query can't). Graceful
            // skip on any failure (missing file, unexpected format) -
            // resolvedCell.floorMesh just stays empty, and callers fall
            // back to the CMSH raycast for that cell, same "degrade, don't
            // fail" posture as everywhere else in this pass.
            if (!cell.floorCollisionFilename.empty()) {
                auto flrBytes = tryExtract(cell.floorCollisionFilename);
                if (flrBytes.has_value()) {
                    try {
                        resolvedCell.floorMesh = assets::FloorCollision::parse(*flrBytes);
                        std::cout << "[FLR] cell \"" << cell.name << "\" -> "
                                   << cell.floorCollisionFilename << " ("
                                   << resolvedCell.floorMesh.positions.size() << " verts, "
                                   << resolvedCell.floorMesh.triangleVertexIndices.size() / 3
                                   << " tris)\n";
                        // TEMPORARY diagnostic - see the matching CMSH one
                        // above.
                        if (!resolvedCell.floorMesh.positions.empty()) {
                            assets::Float3 fmin = resolvedCell.floorMesh.positions[0];
                            assets::Float3 fmax = resolvedCell.floorMesh.positions[0];
                            for (const auto& p : resolvedCell.floorMesh.positions) {
                                fmin.x = std::min(fmin.x, p.x);
                                fmin.y = std::min(fmin.y, p.y);
                                fmin.z = std::min(fmin.z, p.z);
                                fmax.x = std::max(fmax.x, p.x);
                                fmax.y = std::max(fmax.y, p.y);
                                fmax.z = std::max(fmax.z, p.z);
                            }
                            std::cout << "[COLLISION DIAG] cell \"" << cell.name
                                       << "\" FLR real extents min=(" << fmin.x << "," << fmin.y << ","
                                       << fmin.z << ") max=(" << fmax.x << "," << fmax.y << ","
                                       << fmax.z << ")\n";
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "[FLR] cell \"" << cell.name << "\" parse failed: " << e.what()
                                   << "\n";
                    }
                } else {
                    std::cerr << "[FLR] cell \"" << cell.name << "\" file not found: "
                               << cell.floorCollisionFilename << "\n";
                }
            }
            result.push_back(std::move(resolvedCell));
        }

        bool anyCellResolved = false;
        for (const auto& resolvedCell : result) {
            if (!resolvedCell.submeshes.empty()) {
                anyCellResolved = true;
                break;
            }
        }
        if (!anyCellResolved) {
            throw std::runtime_error("no renderable cells resolved for " + realIffPath);
        }

        std::cout << "[BUILDING] resolved objectCrc=0x" << std::hex << objectCrc << std::dec << " -> "
                   << realIffPath << " (" << result.size() << " cells)\n";
        return result;
    }

    // Resolves the real per-building terrain-grading `.lay` file for a
    // building's objectCrc via realGradingMap_ - real GROUND TRUTH, not a
    // heuristic (see realGradingMap_'s own comment for how it was
    // extracted and why an earlier heuristic-guess version of this
    // function was wrong: it applied grading to a player guildhall, but
    // Core3's own real source confirms player construction NEVER gets
    // graded - only military/starport/faction-perk-HQ/lair templates do).
    // Returns std::nullopt (grading simply skipped, never a hard failure)
    // if objectCrc isn't a known building template, the template isn't in
    // the real grading map (the real, correct outcome for every player
    // house/guildhall and the vast majority of other templates), or the
    // mapped `.lay` file is missing/fails to parse.
    std::optional<terrain::TerrainGenerator> resolveGradingLay(uint32_t objectCrc) {
        auto candidateIt = crcToTemplatePath_.find(objectCrc);
        if (candidateIt == crcToTemplatePath_.end()) {
            return std::nullopt;
        }
        // realGradingMap_ is keyed by the real `clientTemplateFileName`
        // form (with the "shared_" filename prefix, matching Core3's own
        // Lua source verbatim) - crcToTemplatePath_ stores the RAW
        // (pre-"shared_") form (see toSharedTemplatePath()'s own comment),
        // so convert before looking up.
        std::string sharedTemplatePath = toSharedTemplatePath(candidateIt->second);
        auto mapIt = realGradingMap_.find(sharedTemplatePath);
        if (mapIt == realGradingMap_.end()) {
            return std::nullopt;
        }
        const std::string& layFile = mapIt->second;

        auto bytes = tryExtract(layFile);
        if (!bytes.has_value()) {
            std::cerr << "[GRADING] real candidate " << layFile << " (for " << sharedTemplatePath
                       << ") not found in any open archive\n";
            return std::nullopt;
        }
        try {
            return terrain::TerrainGenerator::parseStandalone(*bytes);
        } catch (const std::exception& e) {
            std::cerr << "[GRADING] real candidate " << layFile << " failed to parse: " << e.what()
                       << "\n";
            return std::nullopt;
        }
    }

private:
    std::optional<std::vector<uint8_t>> tryExtract(const std::string& name) {
        for (const auto* archive : archives_) {
            if (archive->contains(name)) {
                return archive->extract(name);
            }
        }
        return std::nullopt;
    }

    std::vector<const assets::TreArchive*> archives_;
    std::unordered_map<uint32_t, std::string> crcToTemplatePath_;

    // Real terrain-grading map (Phase 22) - see resolveGradingLay()'s own
    // comment. Keyed by the real `clientTemplateFileName` (with "shared_"
    // prefix), valued by the real `.lay` filename - both extracted
    // verbatim from Core3's own `bin/scripts/object/building/**/
    // objects.lua` (496 real non-empty `terrainModificationFileName`
    // entries found, only 7 distinct real `.lay` files among them - see
    // libs/assets/data/real_terrain_grading_map.txt).
    std::unordered_map<std::string, std::string> realGradingMap_;
};

// Real per-body-part CPU-side data needed for animation (Phase 21) - unlike
// RealSkeletalMeshResolver::resolveMeshDataOnly()'s own MeshData (bind-pose
// geometry only, bone weights discarded), this keeps everything a runtime
// skinning pass needs: the real per-shader submeshes (each carrying its own
// `sourceVertexIndices`, see SkeletalMesh.h), this body part's own real
// bone-name list, and its real mesh-level per-vertex bone weights.
struct AnimatedMeshPart {
    std::vector<assets::SkeletalMeshSubmesh> submeshes;
    std::vector<std::string> boneNames;
    std::vector<std::vector<assets::BoneWeight>> vertexWeights;
};

// Real animation resolution for ONE creature template (Phase 21, v1
// deliberately self-only - see runVisualizer()'s own comment on why other
// visible players/creatures still render bind-pose only this phase).
struct SelfAnimationData {
    assets::SkeletonData skeleton;
    std::vector<AnimatedMeshPart> meshParts;
    assets::AnimationStateTableData stateTable; // real states, empty if resolution failed/no LATX
    // Real gender, derived directly from self's own resolved template path
    // (e.g. "object/creature/player/shared_zabrak_male.iff" - every real
    // player template path checked this session carries a literal
    // "_male"/"_female" suffix) - "male"/"female", or empty if neither
    // marker is present (falls back to the real animation state table's
    // own first gender branch, see AnimationStateTable.h's own comment on
    // AnimationSelectionContext).
    std::string gender;
};

// Resolves a real objectCrc to real SKELETAL mesh geometry (Phase 15) -
// walks the real chain this phase's research discovered and verified
// against real data: .iff (SharedObjectTemplate) -> .sat
// (SkeletalAppearance) -> one or more .lmg (SkeletalMeshLod, per real
// body-part) -> .mgn (SkeletalMesh, may itself have zero or more real
// per-shader submeshes). Unlike RealMeshResolver's candidate list (a
// bundled file mechanically extracted from a Core3 source clone this
// machine doesn't have), this resolver builds its own candidate list by
// enumerating REAL creature/mobile template paths directly out of the
// client archives themselves (TreArchive::listFiles()) - the archives are
// the one source of truth this doesn't depend on an external clone for,
// and every real filename found this way is already in the exact
// "shared_"-prefixed form objectCrc is hashed from (see
// toSharedTemplatePath()'s own comment for why that prefix matters),
// unlike the Lua-derived list which needs that transform applied first.
class RealSkeletalMeshResolver {
public:
    explicit RealSkeletalMeshResolver(const std::string& clientPath) {
        static const char* kArchiveNames[] = {
            "data_other_00.tre",
            "data_animation_00.tre", // real .lat/.ans files (Phase 21, animation)
            "data_skeletal_mesh_00.tre",
            "data_skeletal_mesh_01.tre",
            "data_sku1_00.tre",
            "data_sku1_01.tre",
            "data_sku1_02.tre",
            "data_sku1_03.tre",
            "data_sku1_04.tre",
            "data_sku1_05.tre",
            "data_sku1_06.tre",
            "data_sku1_07.tre",
        };
        for (const char* name : kArchiveNames) {
            std::string path = clientPath + "\\" + name;
            try {
                archives_.push_back({name, assets::TreArchive(path)});
                std::cout << "[SKELMESH] opened " << path << " (" << archives_.back().archive.fileCount()
                           << " files)\n";
            } catch (const std::exception& e) {
                std::cerr << "[SKELMESH] failed to open " << path << ": " << e.what() << "\n";
            }
        }

        for (const auto& named : archives_) {
            for (const auto& filePath : named.archive.listFiles()) {
                bool isCreatureTemplate =
                    (filePath.rfind("object/mobile/", 0) == 0 ||
                     filePath.rfind("object/creature/", 0) == 0) &&
                    filePath.size() >= 4 && filePath.compare(filePath.size() - 4, 4, ".iff") == 0;
                if (!isCreatureTemplate) {
                    continue;
                }
                uint32_t crc = soe::MessageHash::compute(filePath);
                crcToTemplatePath_[crc] = filePath;
            }
        }
        std::cout << "[SKELMESH] indexed " << crcToTemplatePath_.size()
                   << " real creature/mobile template paths\n";
    }

    // Pure-CPU resolution (mirrors RealMeshResolver::resolveMeshDataOnly()'s
    // own shape/threading contract exactly - callable from the asset worker
    // thread, throws on any resolution failure for the caller to catch).
    // A real creature can have multiple body-part meshes (appearance.
    // meshFilenames), each with zero or more real per-shader submeshes
    // (SkeletalMesh::parse()'s own confirmed real anomaly - see
    // SkeletalMesh.h) - a body part that fails to resolve or has zero
    // submeshes is simply skipped, not a hard failure, UNLESS every body
    // part fails, in which case there is nothing renderable and this
    // throws so the caller falls back to the placeholder box.
    std::vector<assets::MeshData> resolveMeshDataOnly(uint32_t objectCrc) {
        auto candidateIt = crcToTemplatePath_.find(objectCrc);
        if (candidateIt == crcToTemplatePath_.end()) {
            throw std::runtime_error("objectCrc not in the known creature/mobile candidate list");
        }
        const std::string& realIffPath = candidateIt->second;

        auto iffBytes = tryExtract(realIffPath);
        if (!iffBytes.has_value()) {
            throw std::runtime_error("template file not found: " + realIffPath);
        }
        assets::SharedObjectTemplateData sharedTemplate;
        try {
            sharedTemplate = assets::SharedObjectTemplate::parse(*iffBytes);
        } catch (const std::exception& e) {
            throw std::runtime_error("template parse failed for " + realIffPath + ": " + e.what());
        }

        auto satBytes = tryExtract(sharedTemplate.appearanceFilename);
        if (!satBytes.has_value()) {
            throw std::runtime_error("skeletal appearance file not found: " +
                                      sharedTemplate.appearanceFilename);
        }
        auto appearance = assets::SkeletalAppearance::parse(*satBytes);

        std::vector<assets::MeshData> submeshMeshes;
        for (const auto& lmgPath : appearance.meshFilenames) {
            auto lmgBytes = tryExtract(lmgPath);
            if (!lmgBytes.has_value()) {
                std::cerr << "[SKELMESH] body-part .lmg not found, skipping: " << lmgPath << "\n";
                continue;
            }
            assets::SkeletalMeshLodData lod;
            try {
                lod = assets::SkeletalMeshLod::parse(*lmgBytes);
            } catch (const std::exception& e) {
                std::cerr << "[SKELMESH] .lmg parse failed, skipping " << lmgPath << ": " << e.what()
                           << "\n";
                continue;
            }
            auto mgnBytes = tryExtract(lod.highestDetailMeshFilename);
            if (!mgnBytes.has_value()) {
                std::cerr << "[SKELMESH] .mgn not found, skipping: " << lod.highestDetailMeshFilename
                           << "\n";
                continue;
            }
            assets::SkeletalMeshData skelMesh;
            try {
                skelMesh = assets::SkeletalMesh::parse(*mgnBytes);
            } catch (const std::exception& e) {
                std::cerr << "[SKELMESH] .mgn parse failed, skipping " << lod.highestDetailMeshFilename
                           << ": " << e.what() << "\n";
                continue;
            }
            for (auto& submesh : skelMesh.submeshes) {
                assets::MeshData meshData;
                meshData.positions = std::move(submesh.positions);
                meshData.normals = std::move(submesh.normals);
                meshData.uv0 = std::move(submesh.uv0);
                meshData.indices = std::move(submesh.indices);
                submeshMeshes.push_back(std::move(meshData));
            }
        }

        if (submeshMeshes.empty()) {
            throw std::runtime_error("no renderable submeshes resolved for " + realIffPath);
        }

        std::cout << "[SKELMESH] resolved objectCrc=0x" << std::hex << objectCrc << std::dec << " -> "
                   << realIffPath << " (" << submeshMeshes.size() << " submeshes)\n";
        return submeshMeshes;
    }

    // Real animation resolution (Phase 21) - same real .iff -> .sat chain as
    // resolveMeshDataOnly() above, but keeps the full per-body-part
    // submesh/bone-weight data AND additionally resolves the real skeleton
    // (.skt) and animation state table (.lat, via the .sat's own LATX
    // chunk). Called once, synchronously (not via AssetWorkerThread - a
    // one-time few-ms cost for a single object is fine, unlike the
    // streaming-many-creatures case the async pipeline exists for). Returns
    // nullopt on any hard failure (no candidate template, .iff/.sat/.skt
    // unreadable) - a missing/unreadable LATX or individual body part is
    // tolerated (empty stateTable / that part just skipped), matching this
    // project's established per-part graceful-fallback convention.
    std::optional<SelfAnimationData> resolveSelfAnimationData(uint32_t objectCrc) {
        auto candidateIt = crcToTemplatePath_.find(objectCrc);
        if (candidateIt == crcToTemplatePath_.end()) {
            return std::nullopt;
        }
        const std::string& realIffPath = candidateIt->second;

        auto iffBytes = tryExtract(realIffPath);
        if (!iffBytes.has_value()) {
            return std::nullopt;
        }
        assets::SharedObjectTemplateData sharedTemplate;
        try {
            sharedTemplate = assets::SharedObjectTemplate::parse(*iffBytes);
        } catch (const std::exception&) {
            return std::nullopt;
        }

        auto satBytes = tryExtract(sharedTemplate.appearanceFilename);
        if (!satBytes.has_value()) {
            return std::nullopt;
        }
        assets::SkeletalAppearanceData appearance;
        try {
            appearance = assets::SkeletalAppearance::parse(*satBytes);
        } catch (const std::exception&) {
            return std::nullopt;
        }

        auto sktBytes = tryExtract(appearance.skeletonFilename);
        if (!sktBytes.has_value()) {
            return std::nullopt;
        }
        SelfAnimationData result;
        if (realIffPath.find("_female") != std::string::npos) {
            result.gender = "female";
        } else if (realIffPath.find("_male") != std::string::npos) {
            result.gender = "male";
        }
        try {
            result.skeleton = assets::Skeleton::parse(*sktBytes);
        } catch (const std::exception& e) {
            std::cerr << "[ANIM] skeleton parse failed for " << appearance.skeletonFilename << ": "
                       << e.what() << "\n";
            return std::nullopt;
        }

        for (const auto& lmgPath : appearance.meshFilenames) {
            auto lmgBytes = tryExtract(lmgPath);
            if (!lmgBytes.has_value()) continue;
            assets::SkeletalMeshLodData lod;
            try {
                lod = assets::SkeletalMeshLod::parse(*lmgBytes);
            } catch (const std::exception&) {
                continue;
            }
            auto mgnBytes = tryExtract(lod.highestDetailMeshFilename);
            if (!mgnBytes.has_value()) continue;
            assets::SkeletalMeshData skelMesh;
            try {
                skelMesh = assets::SkeletalMesh::parse(*mgnBytes);
            } catch (const std::exception&) {
                continue;
            }
            if (skelMesh.submeshes.empty()) continue;

            AnimatedMeshPart part;
            part.submeshes = std::move(skelMesh.submeshes);
            part.boneNames = std::move(skelMesh.boneNames);
            part.vertexWeights = std::move(skelMesh.vertexWeights);
            result.meshParts.push_back(std::move(part));
        }

        if (!appearance.latFilename.empty()) {
            auto latBytes = tryExtract(appearance.latFilename);
            if (latBytes.has_value()) {
                try {
                    result.stateTable = assets::AnimationStateTable::parse(*latBytes);
                } catch (const std::exception& e) {
                    std::cerr << "[ANIM] .lat parse failed for " << appearance.latFilename << ": "
                               << e.what() << "\n";
                }
            }
        }

        std::cout << "[ANIM] resolved self animation data: " << realIffPath << " -> "
                   << result.meshParts.size() << " body parts, " << result.skeleton.bones.size()
                   << " bones, " << result.stateTable.states.size() << " real animation states\n";
        // Phase 21 diagnostic - real skeleton bone names, one-time dump, to
        // directly compare against a real clip's own XFIN bone names rather
        // than assume they match.
        std::cout << "[ANIMDBG] skeleton bone names: ";
        for (size_t i = 0; i < result.skeleton.bones.size(); ++i) {
            std::cout << "[" << i << "]" << result.skeleton.bones[i].name << " ";
        }
        std::cout << "\n";
        // Phase 21 diagnostic - real bone hierarchy (name + parentIndex,
        // resolved to the parent's own name) - added while investigating
        // the finger "sail" artifact, to directly verify the real skeleton
        // parses the deepest chains (thumb/index/ring) with correct parent
        // links, not just that a plausible-looking name list exists.
        std::cout << "[ANIMDBG] skeleton hierarchy:\n";
        for (size_t i = 0; i < result.skeleton.bones.size(); ++i) {
            int32_t parent = result.skeleton.bones[i].parentIndex;
            std::string parentName = "(none)";
            if (parent >= 0 && static_cast<size_t>(parent) < result.skeleton.bones.size()) {
                parentName = result.skeleton.bones[static_cast<size_t>(parent)].name;
            }
            std::cout << "  [" << i << "]" << result.skeleton.bones[i].name << " parentIndex=" << parent
                       << " parentName=" << parentName << "\n";
        }
        // Phase 21 diagnostic - real per-body-part mesh bone names (from
        // each .mgn's own XFNM chunk) - compared against the skeleton
        // bone names above to confirm case/naming actually matches for
        // mesh-to-skeleton binding too, not just clip-to-skeleton.
        for (size_t pi = 0; pi < result.meshParts.size(); ++pi) {
            std::cout << "[ANIMDBG] meshPart[" << pi << "] bone names: ";
            for (size_t bi = 0; bi < result.meshParts[pi].boneNames.size(); ++bi) {
                std::cout << "[" << bi << "]" << result.meshParts[pi].boneNames[bi] << " ";
            }
            std::cout << "\n";
        }
        // Phase 21 diagnostic - flags any real triangle whose longest edge,
        // measured in BIND-POSE (unanimated) local space, is suspiciously
        // large relative to typical mesh-part scale - added while
        // investigating the finger "sail" artifact after ruling out
        // rotation decode, bind pose data, composition order, skinning
        // technique (linear vs dual-quaternion), and bone hierarchy: a
        // mis-triangulated mesh (a real index-parsing bug connecting two
        // vertices that shouldn't be connected, e.g. two different
        // fingertips) would be invisible in a static bind-pose render if
        // the wrongly-connected vertices happen to sit close together
        // there, but would stretch into an obvious flat panel once the
        // bones actually separate during animation - and would look
        // identical under any skinning technique, since both just render
        // whatever triangles exist, wrong ones included.
        for (size_t pi = 0; pi < result.meshParts.size(); ++pi) {
            const auto& part = result.meshParts[pi];
            for (size_t si = 0; si < part.submeshes.size(); ++si) {
                const auto& sm = part.submeshes[si];
                float maxEdge = 0.0f;
                size_t maxEdgeTri = 0;
                for (size_t t = 0; t + 2 < sm.indices.size(); t += 3) {
                    uint32_t ia = sm.indices[t];
                    uint32_t ib = sm.indices[t + 1];
                    uint32_t ic = sm.indices[t + 2];
                    if (ia >= sm.positions.size() || ib >= sm.positions.size() || ic >= sm.positions.size())
                        continue;
                    const auto& pa = sm.positions[ia];
                    const auto& pb = sm.positions[ib];
                    const auto& pc = sm.positions[ic];
                    auto dist = [](const assets::Float3& x, const assets::Float3& y) {
                        float dx = x.x - y.x, dy = x.y - y.y, dz = x.z - y.z;
                        return std::sqrt(dx * dx + dy * dy + dz * dz);
                    };
                    float e = std::max({dist(pa, pb), dist(pb, pc), dist(pc, pa)});
                    if (e > maxEdge) {
                        maxEdge = e;
                        maxEdgeTri = t / 3;
                    }
                }
                std::cout << "[ANIMDBG] meshPart[" << pi << "] submesh[" << si << "] triCount="
                           << (sm.indices.size() / 3) << " vertCount=" << sm.positions.size()
                           << " maxBindEdgeLength=" << maxEdge << " (tri#" << maxEdgeTri << ")\n";
            }
        }
        return result;
    }

    // Real .ans clip resolution, used lazily by runVisualizer()'s own clip
    // cache - any archive this resolver already has open (including
    // data_animation_00.tre, added Phase 21) may hold it.
    std::optional<assets::AnimationClipData> resolveAnimationClip(const std::string& ansPath) {
        auto bytes = tryExtract(ansPath);
        if (!bytes.has_value()) return std::nullopt;
        try {
            return assets::AnimationClip::parse(*bytes);
        } catch (const std::exception& e) {
            std::cerr << "[ANIM] .ans parse failed for " << ansPath << ": " << e.what() << "\n";
            return std::nullopt;
        }
    }

private:
    struct NamedArchive {
        std::string name;
        assets::TreArchive archive;
    };

    std::optional<std::vector<uint8_t>> tryExtract(const std::string& name) {
        // TEMPORARY debug override (Blender reweighting pipeline live test,
        // 2026-07-24) - loads a single reworked .mgn from disk instead of
        // the archived original, to visually verify a vertex-weight fix
        // before deciding whether to ship it. Remove once verified.
        if (name == "appearance/mesh/hum_m_hands_l0.mgn") {
            std::ifstream f(
                "C:\\Users\\lmad2\\AppData\\Local\\Temp\\claude\\c--SWG-Client-new-SWG-Client-New\\"
                "4a3b024f-c467-4d32-9f86-ecf0bf94804d\\scratchpad\\mgn_work\\"
                "hum_m_hands_l0_reweighted_v2.mgn",
                std::ios::binary);
            if (f) {
                std::cout << "[DEBUG-OVERRIDE] loading reweighted " << name << " from disk\n";
                return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)),
                                             std::istreambuf_iterator<char>());
            }
        }
        for (auto& named : archives_) {
            if (named.archive.contains(name)) {
                return named.archive.extract(name);
            }
        }
        return std::nullopt;
    }

    std::vector<NamedArchive> archives_;
    std::unordered_map<uint32_t, std::string> crcToTemplatePath_;
};

// Phase 21 (animation) - real posture/locomotion/mood/weapon -> `.lat`
// state name mapping now lives in creatureanim::stateNameFor (see
// libs/creatureanim/include/creatureanim/AnimationStateSelection.h),
// generalized from this file's own original self-only version.

// Looks up `stateName`'s real selected clip path in `table`, walking the
// state's own real selection tree (gender- and mood-branched - see
// AnimationStateTable.h's own comment) rather than blindly taking
// whichever real clip a naive flatten happened to find first. Empty
// string if the state isn't found.
std::string selfClipPathForState(const assets::AnimationStateTableData& table, const char* stateName,
                                  const assets::AnimationSelectionContext& selectionContext,
                                  bool preferLocomotion) {
    for (const auto& state : table.states) {
        if (state.name == stateName) {
            return assets::selectAnimationClip(state.root, selectionContext, preferLocomotion);
        }
    }
    return {};
}

// Resolves a real terrainName (from CmdStartScene, e.g. "terrain/naboo.trn")
// into a real terrain::TerrainSource - real classic-planet .trn files live
// directly in data_other_00.tre (the same archive RealMeshResolver already
// opens for real mesh templates, and also where this exact terrain file's
// own color-ramp images were confirmed to live this session), not a
// separate texture archive. Takes that already-open archive rather than
// opening its own second copy. Any failure (empty terrainName, archive
// missing, parse throws) returns nullptr - the caller falls back to the
// existing flat reference grid, never blocks rendering, same
// graceful-degradation shape as RealMeshResolver's own resolve().
class TerrainResolver {
public:
    explicit TerrainResolver(const assets::TreArchive* dataOtherArchive)
        : archive_(dataOtherArchive) {}

    std::shared_ptr<terrain::TerrainSource> resolve(const std::string& terrainName) {
        if (terrainName.empty() || !archive_) {
            return nullptr;
        }
        try {
            if (!archive_->contains(terrainName)) {
                std::cerr << "[TERRAIN] " << terrainName << " not found in " << "data_other_00.tre"
                           << "\n";
                return nullptr;
            }
            auto bytes = archive_->extract(terrainName);
            auto source = std::make_shared<terrain::ProceduralTerrainSource>(
                terrain::ProceduralTerrainSource::parse(bytes, archive_));
            std::cout << "[TERRAIN] resolved " << terrainName << "\n";
            return source;
        } catch (const std::exception& e) {
            std::cerr << "[TERRAIN] failed to parse " << terrainName << ": " << e.what() << "\n";
            return nullptr;
        }
    }

private:
    const assets::TreArchive* archive_;
};

// Phase 14 (engine loop redesign) - the asset worker thread. Owns a
// TerrainChunkManager and does BOTH real terrain-chunk generation
// (TerrainSource::generateChunk() - the actual measured ~126ms/chunk cost
// from earlier this session, not just mesh triangulation) and real-mesh
// CPU-side resolution (RealMeshResolver::resolveMeshDataOnly()) off the
// render thread. Deliberately touches NEITHER worldmodel::ObjectStore NOR
// SoeSession/networking - its only inputs are immutable archive/terrain-
// source data (safe for one dedicated thread to query repeatedly with no
// locking, since nothing mutates it after construction) and pure
// computation, so its synchronization surface is limited to the four
// queues below. GPU upload itself stays on the render thread (Vulkan
// command buffer recording isn't set up for multi-threaded use here) -
// this thread only ever produces CPU-side data for the render thread to
// upload.
class AssetWorkerThread {
public:
    // A real ordinary object's mesh almost always has more than one real
    // shader-group submesh (see StaticMeshSubmesh's own comment - confirmed
    // live, e.g. a real elevator platform has 5) - same "carries a whole
    // list" shape as SkeletalMeshReady below, for the identical reason. Each
    // ResolvedSubmesh (Phase 19) carries its own independently-resolved
    // real texture alongside its geometry.
    struct MeshReady {
        uint32_t objectCrc;
        std::optional<std::vector<ResolvedSubmesh>> meshData; // nullopt = resolution failed
    };
    // Phase 15 - a real creature can resolve to more than one submesh
    // (multiple body parts, each with its own real per-shader submeshes -
    // see RealSkeletalMeshResolver's own comment), so unlike MeshReady this
    // carries a whole list; nullopt still means "resolution failed
    // entirely" (falls back to the placeholder box), matching MeshReady's
    // own convention exactly. Character/creature texturing is a deliberate
    // Phase 19 scope cut (see RealSkeletalMeshResolver's own comment) -
    // still plain MeshData, not ResolvedSubmesh.
    struct SkeletalMeshReady {
        uint32_t objectCrc;
        std::optional<std::vector<assets::MeshData>> submeshData;
    };
    // Phase 16 - a real building resolves to one entry per cell (Phase 19:
    // each a ResolvedCell - that cell's own real submeshes, kept separate so
    // each gets its own correctly-matched real texture, plus its own real
    // bounds for local self-position-based room detection - see
    // ResolvedCell/CellBounds' own comments).
    struct BuildingReady {
        uint32_t objectCrc;
        std::optional<std::vector<ResolvedCell>> cellMeshData;
    };
    struct TerrainReady {
        terrain::ChunkCoord coord;
        terrain::TerrainMeshData meshData;
    };
    // A building instance's real terrain-grading request - see
    // requestGrading()'s own comment for why this is keyed by objectId, not
    // objectCrc. No "Ready" queue counterpart: grading has no GPU upload
    // step, it's a pure background side effect directly into the shared
    // ProceduralTerrainSource, so nothing needs to report back to the
    // render thread.
    struct GradingRequest {
        uint64_t objectId;
        uint32_t objectCrc;
        float worldX;
        float worldZ;
    };

    AssetWorkerThread(RealMeshResolver& meshResolver, RealSkeletalMeshResolver& skeletalMeshResolver,
                       RealBuildingResolver& buildingResolver,
                       std::shared_ptr<terrain::TerrainSource> terrainSource, int radiusInChunks)
        : meshResolver_(meshResolver), skeletalMeshResolver_(skeletalMeshResolver),
          buildingResolver_(buildingResolver) {
        if (terrainSource) {
            terrainChunkManager_ =
                std::make_unique<terrain::TerrainChunkManager>(terrainSource, radiusInChunks);
            // Terrain grading (structure footprint flattening, see
            // requestGrading()) is specific to the procedural terrain
            // system - not part of the abstract TerrainSource interface,
            // same as Core3's own ProceduralTerrainAppearance::
            // addTerrainModification() isn't a generic TerrainManager
            // concept either. A null result here (a future, different
            // TerrainSource implementation) just means grading requests
            // silently no-op - same graceful-degrade posture as everything
            // else in this class.
            proceduralTerrainSource_ =
                std::dynamic_pointer_cast<terrain::ProceduralTerrainSource>(terrainSource);
        }
        thread_ = std::thread([this] { run(); });
    }

    ~AssetWorkerThread() {
        stopRequested_.store(true);
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    AssetWorkerThread(const AssetWorkerThread&) = delete;
    AssetWorkerThread& operator=(const AssetWorkerThread&) = delete;

    // Called from the render thread when a newly-seen objectCrc isn't
    // cached and isn't already pending (see MeshHandleCache below).
    void requestMeshResolve(uint32_t objectCrc) { meshJobs_.push(objectCrc); }

    // Same contract as requestMeshResolve(), for creature/player objects
    // (see SkeletalMeshHandleCache below).
    void requestSkeletalMeshResolve(uint32_t objectCrc) { skeletalMeshJobs_.push(objectCrc); }

    // Same contract again, for building objects (see BuildingHandleCache
    // below).
    void requestBuildingResolve(uint32_t objectCrc) { buildingJobs_.push(objectCrc); }

    // Registers real terrain grading (footprint flattening, see
    // RealBuildingResolver::resolveGradingLay()'s own comment) for a
    // building instance - called once per real objectId the first time it's
    // seen with a successfully-resolved buildingCells, matching
    // requestBuildingResolve()'s own "first-seen" caller discipline. Unlike
    // the resolve-by-objectCrc jobs above, grading is keyed by objectId
    // (per-instance world position), not objectCrc (per-template geometry)
    // - the same building template placed at two different positions needs
    // two independent grading registrations.
    void requestGrading(uint64_t objectId, uint32_t objectCrc, float worldX, float worldZ) {
        gradingJobs_.push(GradingRequest{objectId, objectCrc, worldX, worldZ});
    }

    // Un-registers a building's grading (real structure destroyed) - called
    // once the render loop no longer sees a previously-graded objectId.
    void requestUngrading(uint64_t objectId) { ungradingJobs_.push(objectId); }

    // Called from the render thread once per frame - cheap (just an atomic
    // store), read by this thread's own loop to know where to stream
    // terrain around.
    void updateSelfPosition(float x, float z) {
        selfX_.store(x, std::memory_order_relaxed);
        selfZ_.store(z, std::memory_order_relaxed);
    }

    soe::ThreadSafeQueue<MeshReady> meshReady;
    soe::ThreadSafeQueue<SkeletalMeshReady> skeletalMeshReady;
    soe::ThreadSafeQueue<BuildingReady> buildingReady;
    soe::ThreadSafeQueue<TerrainReady> terrainReady;
    soe::ThreadSafeQueue<terrain::ChunkCoord> terrainEvicted;

private:
    void run() {
        while (!stopRequested_.load()) {
            bool didWork = false;

            if (auto objectCrc = meshJobs_.tryPop()) {
                didWork = true;
                std::optional<std::vector<ResolvedSubmesh>> result;
                try {
                    result = meshResolver_.resolveMeshDataOnly(*objectCrc);
                } catch (const std::exception& e) {
                    std::cout << "[MESH] resolution failed for objectCrc=0x" << std::hex
                               << *objectCrc << std::dec << ": " << e.what() << "\n";
                }
                meshReady.push(MeshReady{*objectCrc, std::move(result)});
            }

            if (auto objectCrc = skeletalMeshJobs_.tryPop()) {
                didWork = true;
                std::optional<std::vector<assets::MeshData>> result;
                try {
                    result = skeletalMeshResolver_.resolveMeshDataOnly(*objectCrc);
                } catch (const std::exception& e) {
                    std::cout << "[SKELMESH] resolution failed for objectCrc=0x" << std::hex
                               << *objectCrc << std::dec << ": " << e.what() << "\n";
                }
                skeletalMeshReady.push(SkeletalMeshReady{*objectCrc, std::move(result)});
            }

            if (auto objectCrc = buildingJobs_.tryPop()) {
                didWork = true;
                std::optional<std::vector<ResolvedCell>> result;
                try {
                    result = buildingResolver_.resolveMeshDataOnly(*objectCrc);
                } catch (const std::exception& e) {
                    std::cout << "[BUILDING] resolution failed for objectCrc=0x" << std::hex
                               << *objectCrc << std::dec << ": " << e.what() << "\n";
                }
                buildingReady.push(BuildingReady{*objectCrc, std::move(result)});
            }

            if (auto job = gradingJobs_.tryPop()) {
                didWork = true;
                if (proceduralTerrainSource_) {
                    // Template-scoped: the expensive part (extracting +
                    // parsing the real .lay file) only happens once per
                    // distinct objectCrc, cached forever - every instance of
                    // the same building template reuses the parsed,
                    // still-untranslated generator.
                    auto cacheIt = gradingTemplateCache_.find(job->objectCrc);
                    if (cacheIt == gradingTemplateCache_.end()) {
                        cacheIt = gradingTemplateCache_
                                      .emplace(job->objectCrc,
                                               buildingResolver_.resolveGradingLay(job->objectCrc))
                                      .first;
                    }
                    if (cacheIt->second.has_value()) {
                        // Per-instance: copy the template generator, then
                        // translate its boundaries + bake its flatten
                        // height to THIS instance's real world position -
                        // mirrors Core3's ProceduralTerrainAppearance::
                        // addTerrainModification() exactly (translateBoundary
                        // + setHeight using the PRE-modification height
                        // sampled at (x,z)).
                        terrain::TerrainGenerator instanceGenerator = *cacheIt->second;
                        float currentHeight =
                            proceduralTerrainSource_->queryHeight(job->worldX, job->worldZ);
                        for (auto& layer : instanceGenerator.topLevelLayers) {
                            terrain::translateLayerBoundaries(layer, job->worldX, job->worldZ);
                            terrain::bakeLayerHeight(layer, currentHeight);
                        }
                        proceduralTerrainSource_->addTerrainModification(job->objectId,
                                                                          std::move(instanceGenerator));
                        gradedPositions_[job->objectId] = {job->worldX, job->worldZ};
                        if (terrainChunkManager_) {
                            terrainChunkManager_->invalidateChunksOverlapping(
                                job->worldX, job->worldZ, kGradingInvalidationRadiusMeters);
                        }
                        std::cout << "[GRADING] applied objectId=" << job->objectId << " objectCrc=0x"
                                   << std::hex << job->objectCrc << std::dec << " at (" << job->worldX
                                   << ", " << job->worldZ << ")\n";
                    }
                }
            }

            if (auto objectId = ungradingJobs_.tryPop()) {
                didWork = true;
                if (proceduralTerrainSource_) {
                    proceduralTerrainSource_->removeTerrainModification(*objectId);
                    auto posIt = gradedPositions_.find(*objectId);
                    if (posIt != gradedPositions_.end()) {
                        if (terrainChunkManager_) {
                            terrainChunkManager_->invalidateChunksOverlapping(
                                posIt->second.first, posIt->second.second,
                                kGradingInvalidationRadiusMeters);
                        }
                        gradedPositions_.erase(posIt);
                    }
                }
            }

            if (terrainChunkManager_) {
                float x = selfX_.load(std::memory_order_relaxed);
                float z = selfZ_.load(std::memory_order_relaxed);
                // Budgeted (Phase 15): at most one new chunk generated per
                // loop iteration, not the whole streaming radius in one
                // blocking call - see TerrainChunkManager::
                // loadOneMissingChunk()'s own comment for why (a live test
                // showed the old update()-every-iteration call starving
                // this thread's mesh/skeletal-mesh job queues for the
                // whole initial terrain burst, worse in a Debug build).
                if (terrainChunkManager_->loadOneMissingChunk(x, z)) {
                    didWork = true;
                }
                const auto& loaded = terrainChunkManager_->loadedChunks();

                // New chunks: generateChunk() + buildChunkMesh() both
                // already ran (the former inside update() above, the
                // real expensive step) - just diff against what we've
                // already told the render thread about.
                for (const auto& [coord, chunkData] : loaded) {
                    if (knownTerrainCoords_.insert(coord).second) {
                        didWork = true;
                        terrainReady.push(TerrainReady{coord, terrain::buildChunkMesh(chunkData)});
                    }
                }

                // Evicted chunks: previously known, no longer in the
                // manager's own loaded set (out of streaming radius now).
                for (auto it = knownTerrainCoords_.begin(); it != knownTerrainCoords_.end();) {
                    if (loaded.find(*it) == loaded.end()) {
                        didWork = true;
                        terrainEvicted.push(*it);
                        it = knownTerrainCoords_.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            if (!didWork) {
                // Nothing to do this cycle - avoid a hot spin loop.
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }
    }

    // Real per-building footprint radius isn't known client-side (see
    // RealBuildingResolver::resolveGradingLay()'s own comment on why the
    // real filename mapping is a heuristic) - a generous fixed radius that
    // comfortably covers any real player-house/guildhall `.lay` file's own
    // boundary + feather, confirmed by eye against this project's real
    // player-house footprints, used only to decide which already-generated
    // terrain chunks need to be thrown away and regenerated after a grading
    // change (an overly generous radius just means a few extra harmless
    // chunk regenerations, never a correctness problem).
    static constexpr float kGradingInvalidationRadiusMeters = 64.0f;

    RealMeshResolver& meshResolver_;
    RealSkeletalMeshResolver& skeletalMeshResolver_;
    RealBuildingResolver& buildingResolver_;
    std::unique_ptr<terrain::TerrainChunkManager> terrainChunkManager_;
    std::shared_ptr<terrain::ProceduralTerrainSource> proceduralTerrainSource_;
    std::unordered_set<terrain::ChunkCoord, terrain::ChunkCoordHash> knownTerrainCoords_;
    std::atomic<float> selfX_{0.0f};
    std::atomic<float> selfZ_{0.0f};
    std::thread thread_;
    std::atomic<bool> stopRequested_{false};
    soe::ThreadSafeQueue<uint32_t> meshJobs_;
    soe::ThreadSafeQueue<uint32_t> skeletalMeshJobs_;
    soe::ThreadSafeQueue<uint32_t> buildingJobs_;
    soe::ThreadSafeQueue<GradingRequest> gradingJobs_;
    soe::ThreadSafeQueue<uint64_t> ungradingJobs_;
    // Worker-thread-only state (never touched by the render thread) - the
    // parsed-but-untranslated per-template `.lay` generator (keyed by
    // objectCrc, cached forever like every other resolver cache in this
    // class) and each currently-graded instance's real world position
    // (keyed by objectId, needed to invalidate the right terrain chunks on
    // removal).
    std::unordered_map<uint32_t, std::optional<terrain::TerrainGenerator>> gradingTemplateCache_;
    std::unordered_map<uint64_t, std::pair<float, float>> gradedPositions_;
};

// Render-thread-side cache of resolved mesh handles (Phase 14) - fed by
// AssetWorkerThread's meshReady queue instead of resolving synchronously
// inline in the draw loop. pending_ avoids re-queuing the same objectCrc
// every frame while its resolution is still in flight on the worker
// thread. Mirrors RealMeshResolver's old meshCache_ ownership/lifetime
// exactly (cache-forever, bounded by the finite candidate-template set),
// just relocated here since the render thread - not the CPU-parsing
// resolver - is what owns real GPU handles now.
// Uploads one ResolvedSubmesh's geometry, then (if it resolved a real
// texture) its texture too, wiring the resulting TextureHandle's descriptor
// set directly into the MeshHandle's own textureDescriptorSet field - the
// shared upload step both MeshHandleCache and BuildingHandleCache use below,
// rather than duplicating the two-call sequence in each.
renderer::MeshHandle uploadResolvedSubmesh(renderer::VulkanRenderer& gfx,
                                            const ResolvedSubmesh& resolved) {
    renderer::MeshHandle handle = gfx.loadStaticMesh(resolved.mesh);
    if (resolved.texture.has_value()) {
        renderer::TextureHandle tex = gfx.loadTexture(*resolved.texture);
        handle.textureDescriptorSet = tex.descriptorSet;
    }
    return handle;
}

class MeshHandleCache {
public:
    const std::vector<renderer::MeshHandle>* get(uint32_t objectCrc, AssetWorkerThread& worker) {
        if (objectCrc == 0) {
            return nullptr;
        }
        auto cached = cache_.find(objectCrc);
        if (cached != cache_.end()) {
            return (cached->second.has_value() && !cached->second->empty()) ? &*cached->second
                                                                              : nullptr;
        }
        if (pending_.insert(objectCrc).second) {
            worker.requestMeshResolve(objectCrc);
        }
        return nullptr; // not resolved yet (or just now requested)
    }

    // Drained once per frame from the render loop, budgeted like terrain
    // uploads (see kMaxAssetUploadsPerFrame).
    void drainReady(AssetWorkerThread& worker, renderer::VulkanRenderer& gfx, int maxPerFrame) {
        for (int i = 0; i < maxPerFrame; ++i) {
            auto ready = worker.meshReady.tryPop();
            if (!ready.has_value()) {
                break;
            }
            pending_.erase(ready->objectCrc);
            std::optional<std::vector<renderer::MeshHandle>> handles;
            if (ready->meshData.has_value()) {
                std::vector<renderer::MeshHandle> uploaded;
                uploaded.reserve(ready->meshData->size());
                for (const auto& resolved : *ready->meshData) {
                    uploaded.push_back(uploadResolvedSubmesh(gfx, resolved));
                }
                handles = std::move(uploaded);
            }
            cache_[ready->objectCrc] = std::move(handles);
        }
    }

private:
    std::unordered_map<uint32_t, std::optional<std::vector<renderer::MeshHandle>>> cache_;
    std::unordered_set<uint32_t> pending_;
};

// Phase 15 - same shape/contract as MeshHandleCache above, except a cached
// entry is a whole LIST of mesh handles (one per real submesh a creature's
// body-part meshes resolved to) rather than a single one, since a real
// skeletal appearance can have multiple body parts each with multiple
// per-shader submeshes. An empty (but present, i.e. `has_value()`) vector
// is a real, valid outcome (see RealSkeletalMeshResolver's own comment on
// zero-submesh meshes) and get() below returns nullptr for it exactly like
// a hard resolution failure - both mean "nothing to draw here," so the
// caller's fallback-to-wireframe-box logic doesn't need to distinguish them.
class SkeletalMeshHandleCache {
public:
    const std::vector<renderer::MeshHandle>* get(uint32_t objectCrc, AssetWorkerThread& worker) {
        if (objectCrc == 0) {
            return nullptr;
        }
        auto cached = cache_.find(objectCrc);
        if (cached != cache_.end()) {
            return (cached->second.has_value() && !cached->second->empty()) ? &*cached->second
                                                                              : nullptr;
        }
        if (pending_.insert(objectCrc).second) {
            worker.requestSkeletalMeshResolve(objectCrc);
        }
        return nullptr; // not resolved yet (or just now requested)
    }

    void drainReady(AssetWorkerThread& worker, renderer::VulkanRenderer& gfx, int maxPerFrame) {
        for (int i = 0; i < maxPerFrame; ++i) {
            auto ready = worker.skeletalMeshReady.tryPop();
            if (!ready.has_value()) {
                break;
            }
            pending_.erase(ready->objectCrc);
            std::optional<std::vector<renderer::MeshHandle>> handles;
            if (ready->submeshData.has_value()) {
                std::vector<renderer::MeshHandle> uploaded;
                uploaded.reserve(ready->submeshData->size());
                for (const auto& submeshData : *ready->submeshData) {
                    uploaded.push_back(gfx.loadStaticMesh(submeshData));
                }
                handles = std::move(uploaded);
            }
            cache_[ready->objectCrc] = std::move(handles);
        }
    }

private:
    std::unordered_map<uint32_t, std::optional<std::vector<renderer::MeshHandle>>> cache_;
    std::unordered_set<uint32_t> pending_;
};

// One real cell's uploaded submesh handles plus its own real bounds -
// carried all the way to the render thread (Phase 19) so the draw loop can
// do its own local self-position-based room detection (see CellBounds' own
// comment for why the server-reported cellNumber alone isn't reliable).
struct CellHandles {
    std::vector<renderer::MeshHandle> submeshes;
    std::optional<CellBounds> bounds;
    // Real collision-only geometry (Phase 20) - CPU-side only, never
    // uploaded to the GPU (unlike `submeshes`); may be empty if the real
    // cell had no CMSH data, in which case collision queries for this cell
    // must fall back gracefully (see ResolvedCell::collisionMesh's comment).
    assets::MeshData collisionMesh;
    // Real portal data (Phase 20b) - see ResolvedCell::portals' own
    // comment.
    std::vector<assets::CellPortal> portals;
    std::vector<assets::PortalShape> portalShapes;
    // Real dedicated floor-collision navmesh (Phase 20c) - see
    // ResolvedCell::floorMesh's own comment.
    assets::FloorCollisionMesh floorMesh;
};

// A real cell's own mesh geometry doesn't extend out into the shared
// doorway/portal threshold it connects through (real interior meshes stop
// at their own walls) - a zero-margin containment test only registers a
// room change once self has walked all the way PAST that threshold,
// producing a real, live-caught "doesn't draw the room until you're
// already standing in it" lag right at every doorway. Padding each cell's
// own real bounds outward by this much before testing containment closes
// that gap - self is treated as "in" the next room while still mid-doorway,
// not only once fully inside it. SWG world units are ~meters (this
// project's own established convention), so this is roughly a doorway's
// own real width.
constexpr float kCellBoundsMarginMeters = 1.0f;

// Real, live-caught gap (2026-07-28): a real entrance ramp exists
// specifically to let a structure sit on varying/sloped terrain (the
// building adapts to the ground, not vice versa) - it extends outward from
// the building's own compact footprint well past kCellBoundsMarginMeters,
// to actually reach natural grade (real measured data: self's own local Z
// reached 43.98 against a bare cell-0 max.z of 32.5 - the real ramp
// extends >11m past the compact box). A first fix tried widening
// kCellBoundsMarginMeters itself, which is WRONG - that same constant also
// gates wall-blocking/persistent-cell/portal-crossing logic, and widening
// it uniformly let real stair-riser geometry register as wall hits well
// outside the building's real interior, breaking movement that worked
// before (a real, live-caught regression, reverted). This SEPARATE,
// floor-height-fallback-only margin (see its real single call site in the
// main per-frame movement/collision block) is sized from the real
// measurement above with headroom, not guessed.
constexpr float kFloorFallbackMarginMeters = 20.0f;

// Which real cell (by index) `localPos` (already in this building's own
// local mesh space - see worldToBuildingLocal()) falls inside (padded by
// kCellBoundsMarginMeters - see its own comment), or nullopt if none. Index
// 0 (the exterior shell) is deliberately skipped - its own bounds encompass
// the whole building, so it would always "win" otherwise. When more than
// one interior cell's padded bounds contain the point (expected right at a
// shared doorway, by design now), the smallest-volume match wins, on the
// theory that a smaller room's own box is a tighter, more specific fit than
// a large room's box that happens to also extend into the same area.
std::optional<size_t> findContainingCellIndex(const std::vector<CellHandles>& cells,
                                               const DirectX::XMFLOAT3& localPos) {
    std::optional<size_t> best;
    float bestVolume = 0.0f;
    for (size_t i = 1; i < cells.size(); ++i) {
        if (!cells[i].bounds.has_value()) {
            continue;
        }
        const auto& b = *cells[i].bounds;
        if (localPos.x < b.min.x - kCellBoundsMarginMeters ||
            localPos.x > b.max.x + kCellBoundsMarginMeters ||
            localPos.y < b.min.y - kCellBoundsMarginMeters ||
            localPos.y > b.max.y + kCellBoundsMarginMeters ||
            localPos.z < b.min.z - kCellBoundsMarginMeters ||
            localPos.z > b.max.z + kCellBoundsMarginMeters) {
            continue;
        }
        float volume = (b.max.x - b.min.x) * (b.max.y - b.min.y) * (b.max.z - b.min.z);
        if (!best.has_value() || volume < bestVolume) {
            best = i;
            bestVolume = volume;
        }
    }
    return best;
}

// Phase 20b (portal-based cell transitions) - true if the real segment from
// `from` to `to` (both already in the shared building-local space every
// cell's own data lives in) crosses `shape`'s own real portal plane close
// enough to the polygon's own real extent to count as passing through the
// actual opening. Uses a bounding-sphere-around-the-centroid approximation
// of the true polygon rather than a full point-in-polygon test - simpler,
// and good enough for real door/archway-sized SWG portals (small, roughly
// convex real openings, never oddly star-shaped or huge). A degenerate
// shape (fewer than 3 real vertices, or a zero-area first triangle) never
// blocks a transition - callers must treat that as "this specific portal
// can't be used to detect a crossing," never a hard failure.
bool segmentCrossesPortal(const assets::PortalShape& shape, const DirectX::XMFLOAT3& from,
                           const DirectX::XMFLOAT3& to) {
    if (shape.vertices.size() < 3) {
        return false;
    }
    DirectX::XMFLOAT3 centroid{0.0f, 0.0f, 0.0f};
    for (const auto& v : shape.vertices) {
        centroid.x += v.x;
        centroid.y += v.y;
        centroid.z += v.z;
    }
    float count = static_cast<float>(shape.vertices.size());
    centroid.x /= count;
    centroid.y /= count;
    centroid.z /= count;

    const auto& v0 = shape.vertices[0];
    const auto& v1 = shape.vertices[1];
    const auto& v2 = shape.vertices[2];
    DirectX::XMFLOAT3 edge1{v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
    DirectX::XMFLOAT3 edge2{v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
    DirectX::XMFLOAT3 normal{edge1.y * edge2.z - edge1.z * edge2.y,
                              edge1.z * edge2.x - edge1.x * edge2.z,
                              edge1.x * edge2.y - edge1.y * edge2.x};
    float normalLen = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (normalLen < 1e-6f) {
        return false; // degenerate portal shape
    }
    normal.x /= normalLen;
    normal.y /= normalLen;
    normal.z /= normalLen;

    auto sideOf = [&](const DirectX::XMFLOAT3& p) {
        return (p.x - centroid.x) * normal.x + (p.y - centroid.y) * normal.y +
               (p.z - centroid.z) * normal.z;
    };
    float d0 = sideOf(from);
    float d1 = sideOf(to);
    if ((d0 > 0.0f) == (d1 > 0.0f)) {
        return false; // segment doesn't cross this portal's own plane at all
    }

    float t = d0 / (d0 - d1);
    DirectX::XMFLOAT3 crossPoint{from.x + t * (to.x - from.x), from.y + t * (to.y - from.y),
                                  from.z + t * (to.z - from.z)};

    float maxRadius = 0.0f;
    for (const auto& v : shape.vertices) {
        float dx = v.x - centroid.x;
        float dy = v.y - centroid.y;
        float dz = v.z - centroid.z;
        maxRadius = std::max(maxRadius, std::sqrt(dx * dx + dy * dy + dz * dz));
    }
    float crossDx = crossPoint.x - centroid.x;
    float crossDy = crossPoint.y - centroid.y;
    float crossDz = crossPoint.z - centroid.z;
    float crossDist = std::sqrt(crossDx * crossDx + crossDy * crossDy + crossDz * crossDz);
    return crossDist <= maxRadius;
}

// Phase 20 (collision) - standard Moller-Trumbore ray/triangle intersection.
// `dir` must be a unit vector; on a hit, `outT` is the real distance from
// `origin` along `dir` (since `dir` is unit-length, t IS the distance, not
// just a ray parameter needing further scaling). Used for both the
// straight-down floor-height query and the horizontal wall-blocking sweep
// below - same math, different ray direction/purpose.
bool rayTriangleIntersect(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir,
                           const assets::Float3& v0, const assets::Float3& v1,
                           const assets::Float3& v2, float& outT) {
    constexpr float kEpsilon = 1e-6f;
    DirectX::XMFLOAT3 edge1{v1.x - v0.x, v1.y - v0.y, v1.z - v0.z};
    DirectX::XMFLOAT3 edge2{v2.x - v0.x, v2.y - v0.y, v2.z - v0.z};
    DirectX::XMFLOAT3 h{dir.y * edge2.z - dir.z * edge2.y, dir.z * edge2.x - dir.x * edge2.z,
                         dir.x * edge2.y - dir.y * edge2.x};
    float a = edge1.x * h.x + edge1.y * h.y + edge1.z * h.z;
    if (a > -kEpsilon && a < kEpsilon) {
        return false; // ray parallel to this triangle
    }
    float f = 1.0f / a;
    DirectX::XMFLOAT3 s{origin.x - v0.x, origin.y - v0.y, origin.z - v0.z};
    float u = f * (s.x * h.x + s.y * h.y + s.z * h.z);
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    DirectX::XMFLOAT3 q{s.y * edge1.z - s.z * edge1.y, s.z * edge1.x - s.x * edge1.z,
                         s.x * edge1.y - s.y * edge1.x};
    float v = f * (dir.x * q.x + dir.y * q.y + dir.z * q.z);
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    float t = f * (edge2.x * q.x + edge2.y * q.y + edge2.z * q.z);
    if (t > kEpsilon) {
        outT = t;
        return true;
    }
    return false;
}

// Real floor height (Phase 20c/d) - a proper 2D (X/Z-projected)
// point-in-triangle query against a cell's own dedicated floor-collision
// navmesh (see assets::FloorCollisionMesh's own comment). Preferred over
// the CMSH raycast below wherever real .flr data is available.
//
// KNOWN LIMITATION, found live: a real switchback staircase's LOWER flight
// can share the exact same X/Z footprint as its own UPPER entrance, viewed
// from directly above - confirmed live, self crossing into a stairwell
// cell for the first time got matched to the bottom-of-stairs triangle
// (Y=2.75) instead of the top (Y~9.5), because both real triangles'
// footprints genuinely overlap in 2D at that specific point. A single
// "does any triangle contain this point" query can't disambiguate that -
// it needs real triangle ADJACENCY (assets::FloorCollisionMesh::
// triangleNeighbors) instead: once on a known-good triangle, only that
// triangle and its real neighbors are ever considered for the NEXT frame's
// position, which is a continuity constraint (self can only ever move to a
// triangle it's truly connected to), not a height heuristic that two
// unrelated overlapping surfaces could fool.
struct FloorHit2D {
    float y = 0.0f;
    size_t triangleIndex = 0;
};

// Tests ONE specific real triangle for 2D (X/Z) containment of localPos,
// returning its real barycentric-interpolated Y if it contains the point.
std::optional<float> testFloorTriangle2D(const assets::FloorCollisionMesh& mesh,
                                          size_t triangleIndex,
                                          const DirectX::XMFLOAT3& localPos) {
    size_t i = triangleIndex * 3;
    if (i + 2 >= mesh.triangleVertexIndices.size()) {
        return std::nullopt;
    }
    const auto& v0 = mesh.positions[mesh.triangleVertexIndices[i]];
    const auto& v1 = mesh.positions[mesh.triangleVertexIndices[i + 1]];
    const auto& v2 = mesh.positions[mesh.triangleVertexIndices[i + 2]];
    // Standard 2D barycentric point-in-triangle test.
    float denom = (v1.z - v2.z) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.z - v2.z);
    if (std::fabs(denom) < 1e-8f) {
        return std::nullopt; // degenerate in this X/Z projection
    }
    float a = ((v1.z - v2.z) * (localPos.x - v2.x) + (v2.x - v1.x) * (localPos.z - v2.z)) / denom;
    float b = ((v2.z - v0.z) * (localPos.x - v2.x) + (v0.x - v2.x) * (localPos.z - v2.z)) / denom;
    float c = 1.0f - a - b;
    constexpr float kEdgeTolerance = -0.01f; // small slack for real edge-adjacent positions
    if (a >= kEdgeTolerance && b >= kEdgeTolerance && c >= kEdgeTolerance) {
        return a * v0.y + b * v1.y + c * v2.y;
    }
    return std::nullopt;
}

// Adjacency-restricted query (the normal, everyday path): tests ONLY
// `triangleIndex` and its up-to-3 real neighbors - the ambiguity-free way
// to continue tracking height across ordinary continuous movement. Returns
// nullopt if self has moved off this local neighborhood entirely (a real
// room transition or a big jump) - callers fall back to the full-scan
// version below in that case.
std::optional<FloorHit2D> queryFloorHeight2DAdjacent(const assets::FloorCollisionMesh& mesh,
                                                      size_t triangleIndex,
                                                      const DirectX::XMFLOAT3& localPos) {
    if (auto y = testFloorTriangle2D(mesh, triangleIndex, localPos)) {
        return FloorHit2D{*y, triangleIndex};
    }
    size_t neighborBase = triangleIndex * 3;
    if (neighborBase + 2 < mesh.triangleNeighbors.size()) {
        for (int k = 0; k < 3; ++k) {
            int32_t neighbor = mesh.triangleNeighbors[neighborBase + k];
            if (neighbor < 0) {
                continue;
            }
            size_t neighborIdx = static_cast<size_t>(neighbor);
            if (auto y = testFloorTriangle2D(mesh, neighborIdx, localPos)) {
                return FloorHit2D{*y, neighborIdx};
            }
        }
    }
    return std::nullopt;
}

// Full-mesh scan (only for a fresh cell entry or a big jump, where no
// current-triangle context exists yet): among ALL real triangles whose 2D
// footprint contains localPos.x/z, picks whichever's real Y is CLOSEST to
// `referenceY` - the caller's own last known-good height (e.g. self's Y in
// the PREVIOUS cell, right at the moment of a portal crossing), which
// correctly disambiguates the real switchback case (the upper entrance,
// near referenceY, over the unrelated lower flight far below it).
std::optional<FloorHit2D> queryFloorHeight2DFullScan(const assets::FloorCollisionMesh& mesh,
                                                      const DirectX::XMFLOAT3& localPos,
                                                      float referenceY) {
    std::optional<FloorHit2D> best;
    float bestDelta = 0.0f;
    size_t triCount = mesh.triangleVertexIndices.size() / 3;
    for (size_t t = 0; t < triCount; ++t) {
        auto y = testFloorTriangle2D(mesh, t, localPos);
        if (!y.has_value()) {
            continue;
        }
        float delta = std::fabs(*y - referenceY);
        if (!best.has_value() || delta < bestDelta) {
            best = FloorHit2D{*y, t};
            bestDelta = delta;
        }
    }
    return best;
}

// Real 3D nearest-point floor query (2026-07-29, ramp/porch fix attempt 3 -
// replaces two earlier live-failed attempts, still not fully working - see
// this function's call site). Standard closest-point-on-triangle-in-3D
// algorithm (Ericson, "Real-Time Collision Detection" 5.1.5), ranked by
// real 3D distance across every triangle in the mesh, rather than a 2D
// (X/Z-projected) containment test with a separate height check. Two
// earlier attempts both failed live because they used a 2D (X/Z-only)
// proxy instead of real 3D distance: the first used pure X/Z
// nearest-point-on-edge (no height check at all), the second added a
// height-proximity tiebreak keyed to a reference Y. Both were heuristics
// grafted onto 2D containment; this version ranks purely by real 3D
// distance, which should naturally reject a nearby foundation/skirt edge
// far below self in Y in favor of a legitimate nearby floor point - but
// still doesn't fully resolve the live defect (see the call site's own
// comment) for a reason not yet understood.
DirectX::XMFLOAT3 closestPointOnTriangle3D(const DirectX::XMFLOAT3& p, const assets::Float3& a,
                                            const assets::Float3& b, const assets::Float3& c) {
    float abx = b.x - a.x, aby = b.y - a.y, abz = b.z - a.z;
    float acx = c.x - a.x, acy = c.y - a.y, acz = c.z - a.z;
    float apx = p.x - a.x, apy = p.y - a.y, apz = p.z - a.z;
    float d1 = abx * apx + aby * apy + abz * apz;
    float d2 = acx * apx + acy * apy + acz * apz;
    if (d1 <= 0.0f && d2 <= 0.0f) {
        return {a.x, a.y, a.z};
    }
    float bpx = p.x - b.x, bpy = p.y - b.y, bpz = p.z - b.z;
    float d3 = abx * bpx + aby * bpy + abz * bpz;
    float d4 = acx * bpx + acy * bpy + acz * bpz;
    if (d3 >= 0.0f && d4 <= d3) {
        return {b.x, b.y, b.z};
    }
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return {a.x + abx * v, a.y + aby * v, a.z + abz * v};
    }
    float cpx = p.x - c.x, cpy = p.y - c.y, cpz = p.z - c.z;
    float d5 = abx * cpx + aby * cpy + abz * cpz;
    float d6 = acx * cpx + acy * cpy + acz * cpz;
    if (d6 >= 0.0f && d5 <= d6) {
        return {c.x, c.y, c.z};
    }
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return {a.x + acx * w, a.y + acy * w, a.z + acz * w};
    }
    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return {b.x + (c.x - b.x) * w, b.y + (c.y - b.y) * w, b.z + (c.z - b.z) * w};
    }
    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    return {a.x + abx * v + acx * w, a.y + aby * v + acy * w, a.z + abz * v + acz * w};
}

std::optional<float> queryFloorHeightNearestPoint3D(const assets::FloorCollisionMesh& mesh,
                                                      const DirectX::XMFLOAT3& localPos,
                                                      float maxDistance) {
    if (mesh.positions.empty() || mesh.triangleVertexIndices.empty()) {
        return std::nullopt;
    }
    float bestDistSq = maxDistance * maxDistance;
    std::optional<float> bestY;
    size_t triCount = mesh.triangleVertexIndices.size() / 3;
    for (size_t t = 0; t < triCount; ++t) {
        size_t i = t * 3;
        if (i + 2 >= mesh.triangleVertexIndices.size()) {
            continue;
        }
        const auto& v0 = mesh.positions[mesh.triangleVertexIndices[i]];
        const auto& v1 = mesh.positions[mesh.triangleVertexIndices[i + 1]];
        const auto& v2 = mesh.positions[mesh.triangleVertexIndices[i + 2]];
        DirectX::XMFLOAT3 closest = closestPointOnTriangle3D(localPos, v0, v1, v2);
        float dx = localPos.x - closest.x;
        float dy = localPos.y - closest.y;
        float dz = localPos.z - closest.z;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestY = closest.y;
        }
    }
    return bestY;
}

// Real floor height (Phase 20) - casts a ray straight down through the real
// collision mesh (see BuildingCell::collisionMesh's own comment) from well
// above `localPos`, returning the highest real surface hit below it, or
// nullopt if the mesh is empty/nothing is hit (a real gap between cell
// vertical bounds and where self happens to be, or a cell with no real
// collision data at all) - callers must fall back gracefully in that case,
// never treat it as an error.
//
// KNOWN LIMITATION, found live (Phase 20b): a real switchback staircase
// (two flights stacked over one another within the same cell) breaks
// "highest hit wins" - once self is on the LOWER flight, this ray still
// passes through the UPPER flight/landing directly overhead first, so
// height snaps back to the upper landing instead of continuing down. A
// "closest to self's current height" alternative was tried live and made
// things WORSE (got stuck re-selecting the same wrong surface every frame
// instead of ever finding the true descending path) - reverted. Real fix
// needs something smarter than a single vertical ray (e.g. also checking
// horizontal proximity/reachability from self's last known-good position,
// not just nearest-by-Y) - not yet implemented, tracked as a known gap.
std::optional<float> queryFloorHeight(const assets::MeshData& collisionMesh,
                                       const DirectX::XMFLOAT3& localPos) {
    if (collisionMesh.positions.empty()) {
        return std::nullopt;
    }
    constexpr float kRayStartAbove = 50.0f; // world units - comfortably above any real room height
    DirectX::XMFLOAT3 origin{localPos.x, localPos.y + kRayStartAbove, localPos.z};
    DirectX::XMFLOAT3 down{0.0f, -1.0f, 0.0f};
    std::optional<float> bestT;
    for (size_t i = 0; i + 2 < collisionMesh.indices.size(); i += 3) {
        const auto& v0 = collisionMesh.positions[collisionMesh.indices[i]];
        const auto& v1 = collisionMesh.positions[collisionMesh.indices[i + 1]];
        const auto& v2 = collisionMesh.positions[collisionMesh.indices[i + 2]];
        float t = 0.0f;
        if (rayTriangleIntersect(origin, down, v0, v1, v2, t)) {
            if (!bestT.has_value() || t < *bestT) {
                bestT = t;
            }
        }
    }
    if (!bestT.has_value()) {
        return std::nullopt;
    }
    return origin.y - *bestT;
}

// Real horizontal wall blocking (Phase 20) - true if the straight segment
// from `from` to `to` (both in the same cell's own local space) crosses any
// real triangle of `collisionMesh`. Callers should test at a height above
// the real floor (see kWallTestHeightMeters below) rather than at foot
// level, so a real floor triangle - which the segment would otherwise
// nearly graze along, numerically unstable - is never mistaken for a wall.
bool segmentBlockedByCollisionMesh(const assets::MeshData& collisionMesh,
                                    const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to) {
    if (collisionMesh.positions.empty()) {
        return false;
    }
    DirectX::XMFLOAT3 delta{to.x - from.x, to.y - from.y, to.z - from.z};
    float length = std::sqrt(delta.x * delta.x + delta.y * delta.y + delta.z * delta.z);
    if (length < 1e-5f) {
        return false;
    }
    DirectX::XMFLOAT3 dir{delta.x / length, delta.y / length, delta.z / length};
    for (size_t i = 0; i + 2 < collisionMesh.indices.size(); i += 3) {
        const auto& v0 = collisionMesh.positions[collisionMesh.indices[i]];
        const auto& v1 = collisionMesh.positions[collisionMesh.indices[i + 1]];
        const auto& v2 = collisionMesh.positions[collisionMesh.indices[i + 2]];
        float t = 0.0f;
        if (rayTriangleIntersect(from, dir, v0, v1, v2, t) && t < length) {
            return true;
        }
    }
    return false;
}

// Waist-to-chest height above the real floor, used to test horizontal
// movement against real wall geometry without the test segment nearly
// grazing along the floor mesh itself (see segmentBlockedByCollisionMesh's
// own comment).
constexpr float kWallTestHeightMeters = 1.0f;

// Phase 20, Step 4 - a small, explicitly hand-maintained list of real
// individually-collidable objects that are NOT structural (no BuildingLayout
// cell/CMSH data applies to them - they're ordinary tangible objects) but
// were confirmed live on Finalizer to block movement anyway. Starts with
// exactly the bazaar and bank terminals, both confirmed live. Deliberately
// NOT derived from Core3's own per-template collision flags (confirmed via
// direct byte dump this session: unset/zero for both of these exact
// objects) or from Core3's exact hardcoded per-class server mechanism
// (CloseObjectsVector::COLLIDABLETYPE, not fully traced) - see the plan's
// own Context for the full reasoning. Extend this list opportunistically as
// more real examples turn up through play-testing; no re-derivation needed.
const std::unordered_set<uint32_t>& curatedCollidableObjectCrcs() {
    static const std::unordered_set<uint32_t> crcs = {
        soe::MessageHash::compute("object/tangible/terminal/shared_terminal_bazaar.iff"),
        soe::MessageHash::compute("object/tangible/terminal/shared_terminal_bank.iff"),
    };
    return crcs;
}

// Curated objects have no real per-cell collision mesh to raycast against
// (they're not building geometry) - approximated instead as a simple
// horizontal blocking circle around the object's own real world position (a
// plain point-to-segment distance test in the X/Z plane, ignoring height,
// since these are all fixed floor-standing objects). Same "coarse but
// graceful" spirit as the rest of this pass, not a precise hull.
constexpr float kCuratedObjectBlockRadiusMeters = 0.75f;

bool segmentBlockedByCuratedObject(const DirectX::XMFLOAT3& objPos, const DirectX::XMFLOAT3& from,
                                    const DirectX::XMFLOAT3& to) {
    float dx = to.x - from.x;
    float dz = to.z - from.z;
    float lengthSq = dx * dx + dz * dz;
    float t = 0.0f;
    if (lengthSq > 1e-8f) {
        t = ((objPos.x - from.x) * dx + (objPos.z - from.z) * dz) / lengthSq;
        t = std::clamp(t, 0.0f, 1.0f);
    }
    float closestX = from.x + dx * t;
    float closestZ = from.z + dz * t;
    float distX = objPos.x - closestX;
    float distZ = objPos.z - closestZ;
    return (distX * distX + distZ * distZ) <=
           (kCuratedObjectBlockRadiusMeters * kCuratedObjectBlockRadiusMeters);
}

// Phase 16 - same shape/contract as SkeletalMeshHandleCache above, for real
// building geometry, one level deeper (Phase 19): each cached entry is a
// list of CellHandles, one per real cell, index-aligned with the .pob's own
// cell order exactly as before.
class BuildingHandleCache {
public:
    const std::vector<CellHandles>* get(uint32_t objectCrc, AssetWorkerThread& worker) {
        if (objectCrc == 0) {
            return nullptr;
        }
        auto cached = cache_.find(objectCrc);
        if (cached != cache_.end()) {
            return (cached->second.has_value() && !cached->second->empty()) ? &*cached->second
                                                                              : nullptr;
        }
        if (pending_.insert(objectCrc).second) {
            worker.requestBuildingResolve(objectCrc);
        }
        return nullptr; // not resolved yet (or just now requested)
    }

    void drainReady(AssetWorkerThread& worker, renderer::VulkanRenderer& gfx, int maxPerFrame) {
        for (int i = 0; i < maxPerFrame; ++i) {
            auto ready = worker.buildingReady.tryPop();
            if (!ready.has_value()) {
                break;
            }
            pending_.erase(ready->objectCrc);
            std::optional<std::vector<CellHandles>> handles;
            if (ready->cellMeshData.has_value()) {
                std::vector<CellHandles> uploadedCells;
                uploadedCells.reserve(ready->cellMeshData->size());
                for (const auto& resolvedCell : *ready->cellMeshData) {
                    CellHandles cellHandles;
                    cellHandles.bounds = resolvedCell.bounds;
                    cellHandles.collisionMesh = resolvedCell.collisionMesh;
                    cellHandles.portals = resolvedCell.portals;
                    cellHandles.portalShapes = resolvedCell.portalShapes;
                    cellHandles.floorMesh = resolvedCell.floorMesh;
                    cellHandles.submeshes.reserve(resolvedCell.submeshes.size());
                    for (const auto& resolved : resolvedCell.submeshes) {
                        cellHandles.submeshes.push_back(uploadResolvedSubmesh(gfx, resolved));
                    }
                    uploadedCells.push_back(std::move(cellHandles));
                }
                handles = std::move(uploadedCells);
            }
            cache_[ready->objectCrc] = std::move(handles);
        }
    }

private:
    std::unordered_map<uint32_t, std::optional<std::vector<CellHandles>>> cache_;
    std::unordered_set<uint32_t> pending_;
};

// Phase 14 (engine loop redesign) - owns SoeSession/MessageDispatcher on a
// dedicated thread, confining ALL socket access (both receive AND send) to
// one thread. This sidesteps needing to reason about standalone Asio's
// thread-safety guarantees for concurrent send/receive from different
// threads entirely - the simplest correct option, not a compromise (see
// GAP_ANALYSIS.md section 0.4 and the Phase 14 plan). The render thread
// never touches `session`/`dispatcher` directly anymore: inbound state
// reaches it exclusively through worldmodel::ObjectStore (thread-safe as of
// Phase 14 step 3), and outbound messages are queued via enqueueSend()
// rather than sent directly.
class NetworkThread {
public:
    NetworkThread(soe::SoeSession& session, soe::MessageDispatcher& dispatcher)
        : session_(session), dispatcher_(dispatcher) {
        thread_ = std::thread([this] { run(); });
    }

    ~NetworkThread() {
        stopRequested_.store(true);
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    NetworkThread(const NetworkThread&) = delete;
    NetworkThread& operator=(const NetworkThread&) = delete;

    // Queues bytes for this thread to actually send on its next loop
    // iteration - called from the render thread (e.g. outbound movement),
    // never sends directly.
    void enqueueSend(std::vector<uint8_t> payload) { outbound_.push(std::move(payload)); }

    // Mirrors the `bool& failed` signal every other connection-driving
    // function in this file already watches - set here instead when the
    // connection fails on this thread, polled by the render loop once per
    // frame.
    bool failed() const { return failed_.load(); }

private:
    void run() {
        while (!stopRequested_.load()) {
            std::optional<std::vector<uint8_t>> payload;
            while ((payload = outbound_.tryPop()).has_value()) {
                try {
                    session_.sendMessage(*payload);
                } catch (const std::exception& e) {
                    std::cerr << "[NETWORK] send failed: " << e.what() << "\n";
                    failed_.store(true);
                }
            }

            try {
                // A real (not artificially budget-split) timeout now that
                // this loop no longer shares time with rendering - more
                // responsive to a burst of server traffic than the old
                // single-threaded design's 5ms-per-frame drain window
                // could be, not less.
                auto messages = session_.receiveMessages(std::chrono::milliseconds(50));
                for (auto& msg : messages) {
                    dispatcher_.dispatch(msg);
                }
            } catch (const soe::TimeoutError&) {
                // Normal - just means no traffic arrived this cycle.
            } catch (const soe::DisconnectedError&) {
                failed_.store(true);
                return;
            } catch (const std::exception& e) {
                std::cerr << "[NETWORK] receive failed: " << e.what() << "\n";
                failed_.store(true);
                return;
            }
        }
    }

    soe::SoeSession& session_;
    soe::MessageDispatcher& dispatcher_;
    std::thread thread_;
    std::atomic<bool> stopRequested_{false};
    std::atomic<bool> failed_{false};
    soe::ThreadSafeQueue<std::vector<uint8_t>> outbound_;
};

} // namespace

// The crude wireframe visualizer (Windows/Vulkan only - see libs/renderer).
// Proves the whole pipeline end to end: network decode -> ObjectStore ->
// rendering, per the user's explicit request once Movement work landed real
// positions in the Object Model.
//
// Phase 14 (engine loop redesign): networking now runs on its own thread
// (NetworkThread, above) rather than being polled inline in this loop - the
// original single-threaded design (a short-timeout poll interleaved with
// rendering) was deliberate and worked, but the user explicitly chose to
// decouple networking proactively rather than wait for it to become a
// measured bottleneck (see GAP_ANALYSIS.md section 0.4). worldmodel::
// ObjectStore is genuinely shared now (the network thread writes, this
// loop reads every frame) and thread-safe as of Phase 14 step 3 - this
// loop never needs its own locking, ObjectStore's internal mutex covers it.
//
// Despawned objects need no special handling here: SceneDestroyObject's
// handler (registered permanently in main(), see the movement-work comment
// above it) already calls objectStore.remove() the moment a real destroy
// message arrives, and this loop re-reads objectStore fresh every single
// frame rather than keeping its own object list - a removed object simply
// stops being drawn on the very next frame.
//
// Only objects with at least one real transform update
// (transformMessagesSeen > 0) are drawn - an object that decoded a baseline
// but never broadcast a position would otherwise render at a misleading
// (0,0,0). UpdateTransformMessage alone left this true for the self
// character and most stationary objects (this headless client sends no
// movement of its own, so nothing ever triggered a real broadcast for them).
// FIXED 2026-07-18: main() now also wires DataTransform/
// DataTransformWithParent (an "idle sync" Core3 reliably sends for the self
// character right at zone-in, confirmed live even on a fully passive
// connection - see SESSION_LOG.md) into the SAME transformMessagesSeen/
// position fields, so self now renders with no changes needed here. Other
// stationary objects (buildings/terminals that never move AND never
// received their own idle-sync) may still be invisible - not yet confirmed
// either way, a real "what to add next" candidate rather than solved here.
void runVisualizer(soe::SoeSession& zoneSession, soe::MessageDispatcher& dispatcher, bool& failed,
                    swgproto::ObjControllerDispatcher& objControllerDispatcher,
                    const worldmodel::ObjectStore& objectStore, const std::string& clientPath,
                    const std::string& terrainName, int autoRestPoseTestSecondsPerPhase,
                    int autoRestPoseVariantSweepSecondsPerPhase,
                    int autoRestPoseBindAxisSweepSecondsPerPhase,
                    int autoRestPoseBindOnlySecondsPerPhase) {
    // kObjControllerMessageHash forwarding is no longer registered locally
    // here - main() now owns it permanently (see its 2026-07-18 fix comment).
    renderer::Window window(1920, 1080, L"SWG Client - Crude Visualizer");
    renderer::VulkanRenderer gfx(window.handle(), window.width(), window.height());

    // Real-mesh resolution (2026-07-18) - opens real client archives once
    // at startup; a resolution failure for any given object just falls back
    // to the existing wireframe placeholder box, never blocks rendering.
    // Its CPU-side resolveMeshDataOnly() now runs on the asset worker
    // thread (Phase 14, constructed below) rather than inline in the draw
    // loop - meshResolver itself just needs to outlive that thread.
    RealMeshResolver meshResolver(clientPath);
    MeshHandleCache meshCache;

    // Real skeletal-mesh resolution (Phase 15) - separate resolver/cache
    // from the static-mesh pair above, since RealMeshResolver's own
    // candidate list explicitly excludes creature/mobile templates (see its
    // own comment). Construction opens its own archive handles rather than
    // sharing meshResolver's, matching RealMeshResolver's own "each
    // resolver owns its archives" precedent (findArchiveNamed() exists for
    // the one case - TerrainResolver - that specifically needs to share).
    RealSkeletalMeshResolver skeletalMeshResolver(clientPath);
    SkeletalMeshHandleCache skeletalMeshCache;

    // Real building resolution (Phase 16) - unlike the skeletal-mesh pair
    // above, this DOES share meshResolver's already-open archives
    // (findArchiveNamed(), same as TerrainResolver's own precedent just
    // below) rather than opening a second copy, since buildings live in
    // the exact same archive set ordinary static objects do and are
    // already covered by the same bundled candidate list.
    RealBuildingResolver buildingResolver(meshResolver);
    BuildingHandleCache buildingCache;

    // Real terrain grading (structure footprint flattening around a placed
    // building) - tracks which real objectIds have already had a grading
    // request sent (so it's only ever sent once per instance, not every
    // frame) and which ones are still actually present each frame (a
    // building removed from the world needs its grading un-registered -
    // see the per-frame diff right after the object-store scan below,
    // mirroring AssetWorkerThread's own terrain-chunk eviction diffing).
    std::unordered_set<uint64_t> gradedBuildingIds;

    // Real terrain (Phase 8 of the terrain plan) - resolved once at
    // startup from the real terrainName CmdStartScene reported at zone-in,
    // reusing meshResolver's already-open data_other_00.tre rather than
    // opening a second copy of the same archive. A null terrainSource
    // (empty name, missing archive, parse failure) just means the asset
    // worker thread's own TerrainChunkManager never gets created - no
    // ground surface renders at all in that case (the flat placeholder
    // reference grid was removed 2026-07-20, once real terrain covered
    // every case it used to fall back for).
    TerrainResolver terrainResolver(meshResolver.findArchiveNamed("data_other_00.tre"));
    std::shared_ptr<terrain::TerrainSource> terrainSource = terrainResolver.resolve(terrainName);
    std::unordered_map<terrain::ChunkCoord, renderer::TerrainChunkHandle, terrain::ChunkCoordHash>
        terrainChunkHandles;

    // Phase 14 - real terrain-chunk generation (the actual measured
    // ~126ms/chunk cost, not just mesh triangulation) and real-mesh CPU
    // parsing both now happen on this dedicated thread instead of inline
    // in the render loop. Radius 2 -> a 5x5 chunk window (128m chunks - see
    // ProceduralTerrainSource::kChunkWidthInMeters - so ~640m across),
    // chosen for a reasonable initial-load pause (measured this session:
    // ~25 chunks x ~126ms/chunk in a Release build) rather than the plan's
    // own suggested default of 3 - unchanged from before, just relocated.
    AssetWorkerThread assetWorker(meshResolver, skeletalMeshResolver, buildingResolver, terrainSource,
                                    /*radiusInChunks=*/2);
    constexpr int kMaxAssetUploadsPerFrame = 2; // tunable - see Phase 14 plan's own note

    // Third-person follow camera, locked to self (2026-07-18, replacing the
    // earlier free FlyCamera per user request now that self reliably
    // renders): position is fully re-derived from self's live WorldObject
    // position every frame rather than cached/seeded once, so it tracks self
    // even if self later moves. `lastKnownSelfPos` only matters before
    // self's first seeded position arrives (a brief window right at zone-in)
    // or in the defensive case self is somehow never found - initialized to
    // the origin, same fallback the old camera used before its own
    // self-centering fix.
    renderer::FollowCamera camera;
    // Phase 18 - a second, free noclip camera for full-building inspection
    // (see the 'I' toggle below): lets the user pull back and see a whole
    // building's real geometry at once instead of only the exterior shell
    // or their own current cell, without touching the normal gameplay-
    // accurate FollowCamera/current-cell-filter path used the rest of the
    // time.
    renderer::FlyCamera flyCamera;
    bool inspectionMode = false;
    bool inspectKeyWasDown = false; // edge-detect, same reasoning as leftMouseWasDown below
    DirectX::XMFLOAT3 lastKnownSelfPos{0.0f, 0.0f, 0.0f};
    float lastKnownSelfYawRadians = 0.0f; // for the minimap's facing indicator

    // Animation debug-key bundle (V/C/T/N/Z/R/X/L/F/G/H/U/M/B) and the
    // locked-camera/burst-screenshot pipeline (K/J/P) - see
    // AnimationDebugControls.h/ScreenshotCapture.h for what each owns; both
    // used to live inline here. autoTest drives both automatically when a
    // --rest-pose-* CLI flag was passed (see RestPoseAutoTest.h) - normal
    // interactive runs leave it disabled and it's a no-op.
    dummyclient::AnimationDebugControls animControls;
    dummyclient::ScreenshotCapture screenshotCapture;
    dummyclient::RestPoseAutoTest autoTest(autoRestPoseTestSecondsPerPhase,
                                            autoRestPoseVariantSweepSecondsPerPhase,
                                            autoRestPoseBindAxisSweepSecondsPerPhase,
                                            autoRestPoseBindOnlySecondsPerPhase);

    // Player movement (Phase 13) - a LOCALLY predicted position/facing,
    // driven by WASD input every frame and only ever seeded ONCE from
    // lastKnownSelfPos (the raw, server-reported position) the first frame
    // self is seen. Every visual use of self's position downstream (camera,
    // grid/terrain recentering, self's own drawn box/mesh/label, minimap)
    // reads THIS, not lastKnownSelfPos, so the whole scene stays centered on
    // where WASD has locally moved self to rather than lagging a step behind
    // the last server-confirmed position. Whether the server echoes the
    // sending client's own movement back as an UpdateTransformMessage is
    // unconfirmed (this client has never sent movement before) - if it does,
    // lastKnownSelfPos simply goes unused for rendering (no reconciliation
    // logic added preemptively; see the Phase 13 plan's own note on this).
    DirectX::XMFLOAT3 predictedSelfPos{0.0f, 0.0f, 0.0f};
    float predictedSelfYawRadians = 0.0f;
    bool predictedSelfInitialized = false;
    uint64_t selfObjectId = 0;

    // Phase 21 (animation) - promoted out of the WASD-handling block's own
    // local `movementKeyHeld` (which resets every frame and isn't visible
    // outside that block) into small persistent locomotion state so the
    // draw loop's own animation state-selection (further down, same frame)
    // can read this frame's movement intent.
    bool selfIsMoving = false;

    // Phase 21 (animation) - self-only real animation state (v1 scope, see
    // SelfAnimationData's own comment on why other visible players/
    // creatures still render bind-pose only this phase). Resolved once,
    // synchronously, the first frame self's real objectCrc becomes known
    // (see the draw loop below). `selfDynamicMeshes` holds one
    // renderer::DynamicMeshHandle per real submesh across every resolved
    // body part (flattened, parallel to iterating `animData->meshParts`
    // then each part's own `submeshes`); `selfMeshBoneBindings` is the
    // matching per-body-part bone-name binding (see
    // animation::bindMeshBoneIndices). `selfClipCache` memoizes parsed real
    // `.ans` clips by their real archive path, resolved lazily the first
    // time a given clip is actually needed to animate self (avoids parsing
    // all 664 real states' worth of clips up front for the handful this
    // phase's small state map ever selects).
    std::optional<SelfAnimationData> selfAnimData;
    std::vector<std::vector<int>> selfMeshBoneBindings; // parallel to selfAnimData->meshParts
    std::vector<renderer::DynamicMeshHandle> selfDynamicMeshes;
    std::unordered_map<std::string, std::optional<assets::AnimationClipData>> selfClipCache;
    float selfAnimTimeSeconds = 0.0f;
    bool selfAnimResolveAttempted = false;
    // Phase 21 real fix (2026-07-25) - the currently-active clip's own real
    // CHNK LOCT average translation speed (see assets::AnimationClipData's
    // own comment), 0.0f if the active clip has none (most non-locomotion
    // clips). Used to scale animation playback speed to match self's real
    // movement speed instead of always playing back at a fixed rate - a
    // real, confirmed ~3x mismatch (this project's own walk speed constant
    // is 4.5 units/sec; the real all_b_loc_walk_male.ans clip's own
    // authored average speed is only ~1.55) was found to be the likely
    // cause of a real, direct user report ("legs cross/exaggerated,
    // MD-like" while walking) - the leg keyframe DATA itself was already
    // confirmed individually well-formed and correctly synchronized, so a
    // playback-RATE mismatch (not a decode/composition bug) is the
    // remaining explanation. Updated one frame behind whichever clip is
    // actually resolved for self each frame (see the per-object loop
    // below) - a natural, negligible one-frame lag, not threaded through
    // more tightly since the active clip only changes on real state
    // transitions, not every frame.
    float selfLastActiveClipAverageSpeed = 0.0f;

    // Phase 17 Step 4 - self's currently-tracked containing cell (0 =
    // outdoors/world-relative). Real capture evidence changed the original
    // plan here: rather than deriving floor height from resolved cell mesh
    // geometry (no CPU-side bounding-box data survives past GPU upload
    // today, and would need new plumbing through AssetWorkerThread/
    // BuildingHandleCache to get it), the server ALREADY broadcasts self's
    // own real cell-relative Y via DataTransformWithParent/
    // UpdateTransformWithParentMessage (confirmed live this session, e.g.
    // idle-sync arriving mid-elevator-ride) - simpler, more accurate (it's
    // the server's own authoritative value, not an approximation), and
    // needs no new geometry code at all. So: while indoors, Y is taken
    // directly from the server's own reports (via resolveWorldPosition)
    // instead of a local terrain-height clamp; X/Z stay locally
    // WASD-predicted exactly as outdoors, since floors are flat in
    // practice and real per-frame terrain-style height sampling isn't
    // needed. A cell CHANGE (walking into a different room, riding the
    // elevator) snaps predictedSelfPos straight to the server-resolved
    // world position instead of continuing local prediction through it -
    // correct for a real teleport-like transition (an elevator ride is
    // visually discontinuous anyway), and avoids needing to reconcile
    // stale local X/Z against a coordinate frame that just changed
    // meaning.
    uint64_t predictedSelfParentId = 0;

    // Phase 20b (portal-based cell transitions) - which real cell self is
    // currently believed to be standing in, persisted across frames rather
    // than re-derived from scratch every frame via the old coarse AABB
    // test. Only ever CHANGES when self's own movement segment actually
    // crosses one of the current cell's real portal shapes into the
    // connected adjacent cell (see segmentCrossesPortal()'s own comment) -
    // a real, precise transition signal instead of an approximate
    // bounding-box re-test, which was the root cause of the live-caught
    // boundary flakiness (sinking, "hard edge" snaps, stairs never
    // registering) the AABB-only approach kept hitting right at cell
    // edges. Reset to nullopt (forcing a fresh one-time AABB-based guess)
    // whenever self wasn't inside ANY building footprint last frame, or
    // the building itself changed - see wasInsideBuildingFootprintLastFrame's
    // own comment.
    std::optional<size_t> persistentCellIndex;
    uint64_t persistentCellBuildingId = 0;
    bool wasInsideBuildingFootprintLastFrame = false;

    // Phase 20d - which real triangle of the CURRENT cell's own .flr
    // navmesh self is standing on, persisted across frames for the
    // adjacency-restricted query (see queryFloorHeight2DAdjacent's own
    // comment - this is what actually fixes the real switchback-staircase
    // ambiguity). Keyed by cell index (not building id - persistentCellIndex
    // already handles the building-level reset); reset to nullopt whenever
    // the resolved cell for floor-height purposes changes, forcing a fresh
    // full-mesh scan (see queryFloorHeight2DFullScan) to re-seed it.
    std::optional<size_t> persistentFloorTriangleIndex;
    std::optional<size_t> persistentFloorTriangleCellIndex;

    // Phase 17 Step 5 - edge-detects a fresh left-click (down this frame,
    // wasn't down last frame) rather than firing once per frame the button
    // is held, since Window only exposes level-triggered isMouseButtonDown().
    bool leftMouseWasDown = false;

    // Conservative starting walk speed (world units/second - SWG world units
    // are ~meters, per this project's own established convention) - well
    // under any real character's run speed, tuned against live server
    // behavior rather than a source-confirmed constant (Core3's own
    // checkSpeedHackTests() validates reported speed against actual
    // distance/time with a ~5-10% tolerance, not an exact real value we'd
    // need to match precisely).
    constexpr float kWalkSpeedMetersPerSecond = 4.5f;
    // Real client cadence while moving, confirmed from Core3's own
    // Transform.h (MID_DELTA=400ms) - comfortably above the hard MIN_DELTA=
    // 200ms floor the server enforces (anything faster is rejected outright).
    constexpr uint32_t kMovementSendIntervalMs = 400;

    // Clock isn't declared until just below (the frame-timing block) - using
    // the fully-qualified type here rather than reordering declarations.
    auto movementSendReferenceTime = std::chrono::steady_clock::now(); // timeStamp
                                                                        // epoch - only
                                                                        // relative deltas
                                                                        // between our own
                                                                        // sends matter
    auto lastMovementSendTime = movementSendReferenceTime;
    uint32_t movementMoveCount = 0;

    // Label textures (object name -> rasterized quad), created lazily on
    // first sight of each distinct name and reused every frame after -
    // avoids re-rasterizing the same string through GDI every single frame.
    std::unordered_map<std::string, LabelTexture> labelCache;

    std::cout << "\n[VISUALIZER] window open (1920x1080) - third-person camera locked to self, "
                 "hold the right mouse button to orbit, scroll wheel to zoom. Press 'I' to toggle "
                 "full-building inspection mode (free noclip camera, every cell of a building "
                 "visible at once). Press 'B' to toggle forcing self's animated skinning to the "
                 "real bind pose only, 'V' to cycle the animated-rotation composition variant, "
                 "'C' to cycle a real axis sign/swap correction, 'T' to toggle animated "
                 "translation, 'N' to cycle the real QCHN bit-layout hypothesis, 'M' to cycle "
                 "isolating a single real bone's animation, 'Z' to skip Z-negation for arm-chain "
                 "bones, 'R' to skip Z-negation for the root bone, 'L' to cycle a forced "
                 "dropped-axis override for leg-chain bones, 'F' to cycle the same override for "
                 "the finger+elbow/wrist chain, 'G' to cycle a forced composition-variant override "
                 "for that same chain, or 'H' to cycle a forced axis-fix-variant override for that "
                 "same chain (Phase 21 diagnostics - widened 2026-07-23 to also cover "
                 "forearm/ulna/wrist since they share the same artifact). Press 'K' to lock the "
                 "camera to a fixed angle relative to self's own facing (0/90/180/270deg), 'J' to "
                 "cycle which of those 4 angles is active, and 'P' to start/stop a burst PNG "
                 "screenshot capture (~6/sec) into diagnostic_screenshots/<angle>deg/ - added for "
                 "live walk-cycle visual comparison work. Close the window to exit.\n";

    using Clock = std::chrono::steady_clock;
    auto lastFrameTime = Clock::now();
    const auto targetFrameTime = std::chrono::milliseconds(1000 / 30); // 30fps cap

    // Phase 14 - networking now runs on its own thread for the remainder of
    // this function's lifetime; see NetworkThread's own class comment for
    // why (confining all SoeSession access to one thread). Started here
    // (rather than earlier in this function) only because this is the
    // point every other piece of render-loop state is ready - there's no
    // actual ordering dependency, since ObjectStore's handlers were already
    // registered before runVisualizer() was ever called.
    NetworkThread networkThread(zoneSession, dispatcher);

    while (!failed && !autoTest.complete()) {
        if (!window.pumpMessages()) {
            break;
        }
        if (networkThread.failed()) {
            failed = true;
            break;
        }

        auto now = Clock::now();
        float deltaSeconds = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;

        // Phase 21 (animation) - real .ans clip playback time, treated
        // directly as a frame-number clock (see animation::
        // sampleLocalBoneTransforms's own comment: no real fps scalar has
        // been decoded from a clip's own header yet, so 1 unit == 1 real
        // keyframe "frame" field for now). Free-running/unbounded - each
        // channel wraps independently by its own real keyframe range.
        // Real correction (2026-07-25): an earlier version of this line
        // scaled the PLAYBACK RATE by (real speed / clip's own real LOCT
        // average speed) - live-tested and confirmed WRONG, made the
        // limbs swing much too fast. The real client instead scales the
        // LOCOMOTION TRANSLATION distance to match real movement speed,
        // explicitly WITHOUT touching playback rate - joints always swing
        // at the clip's own native authored cadence. Reverted to the
        // original fixed rate;
        // selfLastActiveClipAverageSpeed/the real LOCT data stays parsed
        // and available for a future real translation-distance-scaling
        // fix, just not used to scale time anymore.
        selfAnimTimeSeconds += deltaSeconds * 30.0f;

        // Phase 18 - one-shot edge-detected toggle (same technique as
        // leftMouseWasDown below), not held-key, so tapping 'I' doesn't
        // flip the mode 30 times/second. Seeds the fly camera's starting
        // position from wherever the follow camera currently is (rather
        // than always resetting to FlyCamera's default {0,15,-15}) so
        // toggling into inspection mode doesn't teleport the view.
        bool inspectKeyDown = renderer::Window::isKeyDown('I');
        if (inspectKeyDown && !inspectKeyWasDown) {
            if (!inspectionMode) {
                flyCamera.position = camera.position;
                flyCamera.yaw = camera.yaw;
                flyCamera.pitch = camera.pitch;
            }
            inspectionMode = !inspectionMode;
            std::cout << "[VISUALIZER] inspection mode " << (inspectionMode ? "ON" : "OFF") << "\n";
        }
        inspectKeyWasDown = inspectKeyDown;

        // Locked-camera/burst-capture keys (K/J/P) - see ScreenshotCapture's
        // own class comment. Not meaningful in inspection mode (that uses
        // the free FlyCamera instead), so this only affects the normal
        // FollowCamera path.
        screenshotCapture.processFrameInput();

        // Animation-composition debug keys (B/V/C/T/N/Z/R/X/L/F/G/H/U/M) -
        // see AnimationDebugControls's own class comment for what each one
        // tests and why they're kept as permanent debug aids.
        animControls.processFrameInput(selfClipCache);
        if (autoTest.enabled()) {
            autoTest.driveFrame(animControls, screenshotCapture);
        }

        // One snapshot per frame, reused by every resolveWorldPosition() call
        // below (self-seeding, draw pass, label pass) - see
        // resolveWorldPosition()'s own comment for why it can't call back
        // into the live ObjectStore from inside a forEach() callback.
        auto worldSnapshot = objectStore.snapshotWorldObjects();

        objectStore.forEach([&](const auto& obj) {
            if (obj.isSelf && obj.transformMessagesSeen > 0) {
                auto resolved = resolveWorldPosition(worldSnapshot, obj);
                lastKnownSelfYawRadians =
                    (static_cast<float>(obj.direction) / 100.0f) * DirectX::XM_2PI;
                selfObjectId = obj.objectId;
                bool parentChanged = obj.parentId != predictedSelfParentId;
                // Real containment status tracked regardless of whether
                // resolveWorldPosition() could fully resolve it this frame
                // - this alone decides indoor-vs-outdoor height handling
                // below, and must stay correct even while unresolved so
                // that branch doesn't wrongly fall back to outdoor terrain
                // height while genuinely indoors.
                predictedSelfParentId = obj.parentId;
                if (resolved.has_value()) {
                    // Only snap/track position from a SUCCESSFULLY resolved
                    // value - see resolveWorldPosition()'s own comment for
                    // why silently accepting an unresolved fallback (raw
                    // local coordinates treated as world-space) was a real,
                    // live-caught bug ("floating, ignoring terrain, near
                    // world origin"). If unresolved, predictedSelfPos and
                    // lastKnownSelfPos both simply hold their last known
                    // good values untouched - a stall, not a wrong jump.
                    lastKnownSelfPos = *resolved;
                    if (!predictedSelfInitialized) {
                        predictedSelfPos = lastKnownSelfPos;
                        predictedSelfYawRadians = lastKnownSelfYawRadians;
                        predictedSelfInitialized = true;
                    } else if (parentChanged) {
                        // A real cell transition (entered/left a building,
                        // rode the elevator, walked into a different room) -
                        // snap rather than keep predicting through it, see
                        // predictedSelfParentId's own comment above.
                        predictedSelfPos = lastKnownSelfPos;
                    }
                }
            }
        });

        // Player movement (Phase 13) - camera-relative WASD: forward/back
        // along the camera's own current look direction (projected onto the
        // XZ plane - vertical look angle shouldn't tilt movement), strafe
        // perpendicular to it. Character always faces its movement
        // direction (confirmed with the user over a classic SWG-style
        // turn-in-place scheme). Only runs once self's real starting
        // position is known - there's nothing to move from before that.
        // Skipped entirely in inspection mode (Phase 18): WASD drives the
        // free fly camera instead (via FlyCamera::update() below), not self
        // - self simply stays put while the camera roams.
        if (predictedSelfInitialized && !inspectionMode) {
            DirectX::XMFLOAT3 lookDir = camera.lookDirection();
            DirectX::XMVECTOR forwardVec = DirectX::XMVector3Normalize(
                DirectX::XMVectorSet(lookDir.x, 0.0f, lookDir.z, 0.0f));
            DirectX::XMVECTOR worldUpVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            // Same right-vector convention already used by the label-
            // billboard basis below (cross(worldUp, forward)).
            DirectX::XMVECTOR rightVec =
                DirectX::XMVector3Normalize(DirectX::XMVector3Cross(worldUpVec, forwardVec));

            DirectX::XMVECTOR moveVec = DirectX::XMVectorZero();
            bool movementKeyHeld = false;
            if (renderer::Window::isKeyDown('W')) {
                moveVec = DirectX::XMVectorAdd(moveVec, forwardVec);
                movementKeyHeld = true;
            }
            if (renderer::Window::isKeyDown('S')) {
                moveVec = DirectX::XMVectorSubtract(moveVec, forwardVec);
                movementKeyHeld = true;
            }
            if (renderer::Window::isKeyDown('D')) {
                moveVec = DirectX::XMVectorAdd(moveVec, rightVec);
                movementKeyHeld = true;
            }
            if (renderer::Window::isKeyDown('A')) {
                moveVec = DirectX::XMVectorSubtract(moveVec, rightVec);
                movementKeyHeld = true;
            }
            selfIsMoving = movementKeyHeld; // Phase 21 - persists past this block for animation state selection

            // Real collision (Phase 20) needs both the pre-move and
            // candidate post-move world position - captured here, before
            // the WASD delta below mutates predictedSelfPos.x/z in place.
            DirectX::XMFLOAT3 preMovePos = predictedSelfPos;

            float currentSpeed = 0.0f;
            if (movementKeyHeld) {
                moveVec = DirectX::XMVector3Normalize(moveVec);
                DirectX::XMFLOAT3 moveDir;
                DirectX::XMStoreFloat3(&moveDir, moveVec);

                predictedSelfPos.x += moveDir.x * kWalkSpeedMetersPerSecond * deltaSeconds;
                predictedSelfPos.z += moveDir.z * kWalkSpeedMetersPerSecond * deltaSeconds;
                // Same sin/cos(yaw) convention as FlyCamera::forward()/
                // FollowCamera::lookDirection() and the minimap facing
                // indicator below - yaw=0 faces +Z, increasing yaw rotates
                // toward +X.
                predictedSelfYawRadians = std::atan2(moveDir.x, moveDir.z);
                currentSpeed = kWalkSpeedMetersPerSecond;
            }

            // Phase 19 fix, extended by Phase 20 (real collision) - "am I
            // inside a building's footprint," "which specific real cell,"
            // real wall blocking, and real floor height are all determined
            // locally now, same reasoning throughout: predictedSelfParentId
            // only updates right at zone-in or after a genuine server-driven
            // relocation (an elevator ride), never during ordinary walking,
            // so anything waiting on a fresh server report goes stale - see
            // project_server_position_echo_gap.md. Deliberately does NOT
            // touch predictedSelfParentId itself, which the outbound-
            // movement/anti-cheat code below still depends on.
            //
            // The specific cell is looked up using the PRE-move position
            // (self was validly there a moment ago, guaranteed), not the
            // just-computed candidate - a real wall-clip attempt can put
            // the candidate position on the far side of a wall, outside
            // every cell's own bounds, which would otherwise make "which
            // cell's collision mesh applies" ambiguous right when it
            // matters most.
            bool selfInsideAnyBuildingFootprint = false;
            bool movementBlockedByWall = false;
            // Real bugfix, found live: queryFloorHeight() returns a height
            // in the BUILDING-LOCAL frame (it raycasts against the
            // collision mesh's own local vertices, same space as oldLocal/
            // newLocal) - it must be converted back to WORLD space (+
            // buildingCandidate.y, captured here as buildingWorldY) before
            // ever being written to predictedSelfPos.y, which is always
            // world space. Missing that conversion was the real root cause
            // of the live-observed stuttering/oscillation and "walks
            // through stairs" behavior: a real local floor hit (e.g. 9.5)
            // was being written directly into predictedSelfPos.y as if it
            // were already a world Y, teleporting self to a wildly wrong
            // world height every time a floor hit was found. That wrong
            // world Y then failed the NEXT frame's containment test
            // (worldToBuildingLocal correctly subtracts building.y, so the
            // now-wrong world Y produced local coordinates outside every
            // real cell's bounds), falling back to the stale spawn-time
            // lastKnownSelfPos.y, re-entering the room, computing the same
            // local floor height again, and repeating forever.
            std::optional<float> newFloorHeight;
            float buildingWorldY = 0.0f;
            objectStore.forEach([&](const auto& buildingCandidate) {
                using T = std::decay_t<decltype(buildingCandidate)>;
                if constexpr (std::is_same_v<T, worldmodel::CellObject> ||
                              std::is_same_v<T, worldmodel::GroupObject>) {
                    return;
                } else {
                    if (selfInsideAnyBuildingFootprint || buildingCandidate.isSelf ||
                        buildingCandidate.typeTag == worldmodel::ObjectTypeTag::Creature ||
                        buildingCandidate.typeTag == worldmodel::ObjectTypeTag::Player) {
                        return;
                    }
                    const auto* cells = buildingCache.get(buildingCandidate.objectCrc, assetWorker);
                    if (cells == nullptr || cells->empty() || !(*cells)[0].bounds.has_value()) {
                        return;
                    }
                    const auto& b = *(*cells)[0].bounds;
                    DirectX::XMFLOAT3 oldLocal = worldToBuildingLocal(preMovePos, buildingCandidate);
                    DirectX::XMFLOAT3 newLocal =
                        worldToBuildingLocal(predictedSelfPos, buildingCandidate);

                    // Real, live-caught gap (2026-07-28): a real entrance
                    // ramp exists specifically so a structure can sit on
                    // varying/sloped terrain (the building adapts to the
                    // ground, not vice versa - confirmed via direct
                    // observation of a real guildhall's own ramp, and via
                    // real measured data: self's own local Z reached 43.98
                    // against a bare max.z of 32.5, i.e. the real ramp
                    // extends >11m past cell 0's own compact bounding box).
                    // A FIRST fix (widening kCellBoundsMarginMeters itself
                    // to 10m) was wrong: that same margin also gates the
                    // wall-blocking/persistent-cell/portal-crossing logic
                    // below, and widening it uniformly let real stair-riser
                    // geometry start registering as wall hits well outside
                    // the building's real interior, breaking movement that
                    // worked before. The two concerns need DIFFERENT
                    // margins: wall-blocking/persistent-cell tracking stays
                    // on the original tight kCellBoundsMarginMeters (real,
                    // working stair/doorway behavior, unchanged), while
                    // ONLY the floor-height fallback (real ramps/porches
                    // are shell-owned geometry - see the existing shell-
                    // fallback comment below) gets the wider
                    // kFloorFallbackMarginMeters, sized from the real
                    // measurement above with headroom, not guessed.
                    bool outsideWideFootprint = oldLocal.x < b.min.x - kFloorFallbackMarginMeters ||
                        oldLocal.x > b.max.x + kFloorFallbackMarginMeters ||
                        oldLocal.y < b.min.y - kFloorFallbackMarginMeters ||
                        oldLocal.y > b.max.y + kFloorFallbackMarginMeters ||
                        oldLocal.z < b.min.z - kFloorFallbackMarginMeters ||
                        oldLocal.z > b.max.z + kFloorFallbackMarginMeters;
                    if (outsideWideFootprint) {
                        return;
                    }
                    bool outsideTightFootprint = oldLocal.x < b.min.x - kCellBoundsMarginMeters ||
                        oldLocal.x > b.max.x + kCellBoundsMarginMeters ||
                        oldLocal.y < b.min.y - kCellBoundsMarginMeters ||
                        oldLocal.y > b.max.y + kCellBoundsMarginMeters ||
                        oldLocal.z < b.min.z - kCellBoundsMarginMeters ||
                        oldLocal.z > b.max.z + kCellBoundsMarginMeters;

                    const CellHandles& shell = (*cells)[0];
                    // Default: the unblocked candidate position - the
                    // ramp-only branch below never computes a `blocked`
                    // flag (no wall test happens out there), so this stays
                    // as-is for that case; the tight-footprint branch
                    // overwrites it once `blocked` is known.
                    DirectX::XMFLOAT3 floorQueryPos = newLocal;
                    // Real fix (Phase 20c/d): the dedicated .flr navmesh's
                    // own 2D point-in-triangle query is tried FIRST - it
                    // can't confuse two vertically-stacked surfaces the way
                    // the CMSH raycast can. Adjacency-restricted
                    // (queryFloorHeight2DAdjacent) whenever a persistent
                    // triangle is already known for THIS source cell -
                    // that's the ambiguity-free everyday path. Only falls
                    // back to a full-mesh scan (queryFloorHeight2DFullScan,
                    // referenced against floorQueryPos.y - self's own
                    // last-known height, which correctly disambiguates a
                    // real switchback's upper entrance from its unrelated
                    // lower flight) on a fresh cell entry, then to the CMSH
                    // raycast if this cell has no real .flr data at all.
                    // allowEdgeExtrapolation (2026-07-29): deliberately
                    // defaulted false and only ever passed true from the
                    // ramp/porch branch below. The interior/tight-footprint
                    // call sites intentionally do NOT get this - the "hold
                    // Y unchanged on a real interior floor-mesh gap" fallback
                    // a few lines further down (see its own "round 2" bugfix
                    // comment) was already tuned live once before and an
                    // over-eager extrapolation there previously caused a
                    // regression; scoping this fix narrowly to the one real,
                    // measured problem (the shell's own ramp mesh ending
                    // ~5m short of where self actually walks) avoids
                    // reopening that.
                    auto tryFloorHeight = [&](const CellHandles& source, size_t sourceCellIndex,
                                               bool allowEdgeExtrapolation = false) {
                        if (!source.floorMesh.positions.empty()) {
                            std::optional<FloorHit2D> hit2D;
                            if (persistentFloorTriangleIndex.has_value() &&
                                persistentFloorTriangleCellIndex.has_value() &&
                                *persistentFloorTriangleCellIndex == sourceCellIndex) {
                                hit2D = queryFloorHeight2DAdjacent(
                                    source.floorMesh, *persistentFloorTriangleIndex, floorQueryPos);
                            }
                            if (!hit2D.has_value()) {
                                hit2D = queryFloorHeight2DFullScan(source.floorMesh, floorQueryPos,
                                                                    floorQueryPos.y);
                            }
                            if (hit2D.has_value()) {
                                newFloorHeight = hit2D->y;
                                persistentFloorTriangleIndex = hit2D->triangleIndex;
                                persistentFloorTriangleCellIndex = sourceCellIndex;
                                return;
                            }
                            if (allowEdgeExtrapolation) {
                                // Real 3D nearest-point search (see this
                                // function's own comment) - localPos already
                                // carries real Y, so no separate reference
                                // height is needed the way the 2D-only
                                // attempts before this one required.
                                auto nearestY = queryFloorHeightNearestPoint3D(
                                    source.floorMesh, floorQueryPos, kFloorFallbackMarginMeters);
                                if (nearestY.has_value()) {
                                    newFloorHeight = nearestY;
                                    return;
                                }
                            }
                        }
                        if (source.collisionMesh.positions.empty()) {
                            return;
                        }
                        auto hit = queryFloorHeight(source.collisionMesh, floorQueryPos);
                        if (!hit.has_value()) {
                            return;
                        }
                        newFloorHeight = hit;
                    };

                    if (outsideTightFootprint) {
                        // Ramp/porch zone: wide margin only. No wall test,
                        // no persistent-cell/portal-crossing tracking -
                        // self isn't meaningfully "in a room" out here,
                        // just walking a real entrance ramp that happens to
                        // extend past the building's compact interior box.
                        // buildingWorldY still needs to be set (newFloorHeight
                        // is in LOCAL space, converted back to world further
                        // below using this) - same real local-to-world
                        // bugfix as the tight-footprint case.
                        buildingWorldY = buildingCandidate.y;
                        tryFloorHeight(shell, 0, /*allowEdgeExtrapolation=*/true);
                        // TEMPORARY diagnostic (2026-07-29, ramp fix retest -
                        // edge-extrapolation fix reported as still not
                        // working live) - throttled real numbers for every
                        // frame self is in this ramp/porch ring, so the next
                        // retest can show exactly what height (if any) got
                        // picked instead of guessing again. Strip once
                        // resolved.
                        {
                            static int rampDiagCounter = 0;
                            if ((rampDiagCounter++ % 10) == 0) {
                                float terrainHeightHere = terrainSource
                                    ? terrainSource->queryHeight(predictedSelfPos.x, predictedSelfPos.z)
                                    : -9999.0f;
                                std::cout << "[RAMP DIAG] local=(" << newLocal.x << "," << newLocal.y
                                           << "," << newLocal.z << ") newFloorHeight="
                                           << (newFloorHeight.has_value()
                                                   ? std::to_string(*newFloorHeight)
                                                   : std::string("NONE"))
                                           << " worldY(if used)="
                                           << (newFloorHeight.has_value()
                                                   ? std::to_string(buildingWorldY + *newFloorHeight)
                                                   : std::string("N/A"))
                                           << " terrainHeightHere=" << terrainHeightHere
                                           << " predictedSelfPos.y=" << predictedSelfPos.y << "\n";
                            }
                        }
                        return;
                    }

                    selfInsideAnyBuildingFootprint = true;
                    buildingWorldY = buildingCandidate.y;

                    DirectX::XMFLOAT3 wallTestFrom{oldLocal.x, oldLocal.y + kWallTestHeightMeters,
                                                    oldLocal.z};
                    DirectX::XMFLOAT3 wallTestTo{newLocal.x, newLocal.y + kWallTestHeightMeters,
                                                  newLocal.z};

                    // Real bugfix, found live (Phase 20b): the old AABB-only
                    // findContainingCellIndex() was re-run from scratch every
                    // single frame, and cell boundaries are exactly where its
                    // padded-bounding-box approximation is weakest (real
                    // rooms are rarely axis-aligned boxes; a stairwell is a
                    // continuous ramp, not a box at all) - confirmed live as
                    // the root cause of recurring boundary flakiness
                    // (sinking on approach, "hard edge" snaps, stairs never
                    // registering height changes). Real per-cell portal data
                    // (see assets::BuildingCell::portals' own comment) gives
                    // an exact, precise transition signal instead: which
                    // cell self is in is now PERSISTENT state
                    // (persistentCellIndex, reset only on a genuine fresh
                    // entry - see its own comment), only ever changing when
                    // self's own movement segment actually crosses one of
                    // the CURRENT cell's real portal shapes into the
                    // connected adjacent cell. The AABB test still runs, but
                    // only once, as the one-time initial guess for a fresh
                    // entry - never as the ongoing per-frame source of
                    // truth.
                    if (!wasInsideBuildingFootprintLastFrame ||
                        persistentCellBuildingId != buildingCandidate.objectId) {
                        persistentCellIndex.reset();
                        persistentCellBuildingId = buildingCandidate.objectId;
                    }
                    if (!persistentCellIndex.has_value()) {
                        persistentCellIndex = findContainingCellIndex(*cells, oldLocal);
                        if (!persistentCellIndex.has_value()) {
                            // Real bugfix, found live: standing on the
                            // building's own ROOF (real geometry that's part
                            // of cell 0/the exterior shell, not any interior
                            // room) has no interior cell whose bounds
                            // contain it - findContainingCellIndex()
                            // deliberately excludes cell 0 itself (correct
                            // for its OTHER caller, room-highlight rendering
                            // - see its own comment), so this always
                            // returned nullopt there. For seeding the
                            // PERSISTENT cell specifically, cell 0 is a
                            // legitimate starting point in its own right -
                            // it has real CMSH floor geometry AND its own
                            // real portal list (confirmed live: without
                            // this fallback, self on the real roof got
                            // seeded into a wrong, unrelated interior room
                            // via the same footprint-AABB match used
                            // elsewhere, then got permanently stuck there
                            // since that wrong room had no real portal path
                            // back to where self actually was).
                            persistentCellIndex = 0;
                        }
                    }
                    if (persistentCellIndex.has_value() && *persistentCellIndex < cells->size()) {
                        const CellHandles& currentCell = (*cells)[*persistentCellIndex];
                        for (const auto& portalRef : currentCell.portals) {
                            if (portalRef.portalShapeIndex >= currentCell.portalShapes.size() ||
                                portalRef.adjacentCellIndex >= cells->size()) {
                                continue;
                            }
                            const auto& shape = currentCell.portalShapes[portalRef.portalShapeIndex];
                            if (segmentCrossesPortal(shape, oldLocal, newLocal)) {
                                persistentCellIndex = portalRef.adjacentCellIndex;
                                break;
                            }
                        }
                    }
                    auto cellIndex = persistentCellIndex;
                    bool blocked = false;
                    if (cellIndex.has_value()) {
                        const CellHandles& cell = (*cells)[*cellIndex];
                        if (!cell.collisionMesh.positions.empty() &&
                            segmentBlockedByCollisionMesh(cell.collisionMesh, wallTestFrom,
                                                           wallTestTo)) {
                            blocked = true;
                        }
                    }

                    // Real bugfix, found live: cell 0 (the exterior shell)
                    // carries the true outer-wall geometry - an interior
                    // room's own CMSH only covers its own internal
                    // partitions, never the true perimeter wall, so the one
                    // frame self's movement actually crossed that wall was
                    // going untested (using only the interior room's own
                    // mesh, which has no triangle there), letting self
                    // tunnel straight through. Checked unconditionally, not
                    // only when no interior cell matches, so the crossing
                    // frame itself is always caught rather than one frame
                    // late. Deliberately NEVER used for floor height below
                    // (see that comment) - blocking doesn't write to Y, so
                    // this can't cause the height-lock feedback bug height
                    // queries did.
                    if (!shell.collisionMesh.positions.empty() &&
                        segmentBlockedByCollisionMesh(shell.collisionMesh, wallTestFrom,
                                                       wallTestTo)) {
                        blocked = true;
                    }

                    if (blocked) {
                        movementBlockedByWall = true;
                    }

                    // Floor height: the matched interior room's own CMSH
                    // takes priority (more precise per-room data), falling
                    // back to cell 0's (the exterior shell's) own CMSH when
                    // no interior room matched or its own mesh gave no hit -
                    // real entry ramps/stairs/porches leading up to the
                    // building's own front door live in the shell's mesh,
                    // not any interior room's. Re-enabled tonight: this was
                    // disabled entirely after last night's "permanent height
                    // lock" bug, but that bug's real root cause (confirmed
                    // and fixed above) was the missing local-to-world Y
                    // conversion, not cell 0's data itself - once a real
                    // floor hit is correctly converted back to world space,
                    // it can no longer feed a wrong value into next frame's
                    // containment test the way the old bug did.
                    floorQueryPos = blocked ? oldLocal : newLocal;
                    if (cellIndex.has_value()) {
                        tryFloorHeight((*cells)[*cellIndex], *cellIndex);
                    }
                    if (!newFloorHeight.has_value()) {
                        tryFloorHeight(shell, 0);
                    }
                }
            });

            // Phase 20b - captures THIS frame's final footprint state for
            // next frame's persistent-cell reset check (see
            // persistentCellIndex's own comment) - must run after the
            // forEach above, once selfInsideAnyBuildingFootprint is settled
            // for the frame.
            wasInsideBuildingFootprintLastFrame = selfInsideAnyBuildingFootprint;

            // Phase 20, Step 4 - the small curated non-structural object
            // list (bazaar/bank terminals). Only checked if the wall pass
            // above didn't already block this move - no need to resolve
            // curated-object positions on an already-rejected move. Uses
            // the same worldSnapshot taken once per frame, per
            // resolveWorldPosition()'s own calling convention.
            if (!movementBlockedByWall) {
                const auto& curatedCrcs = curatedCollidableObjectCrcs();
                objectStore.forEach([&](const auto& candidate) {
                    using T = std::decay_t<decltype(candidate)>;
                    if constexpr (std::is_same_v<T, worldmodel::CellObject> ||
                                  std::is_same_v<T, worldmodel::GroupObject>) {
                        return;
                    } else {
                        if (movementBlockedByWall || candidate.isSelf ||
                            curatedCrcs.find(candidate.objectCrc) == curatedCrcs.end()) {
                            return;
                        }
                        auto objPos = resolveWorldPosition(worldSnapshot, candidate);
                        if (!objPos.has_value()) {
                            return; // graceful fallback - unresolved position, can't test it
                        }
                        if (segmentBlockedByCuratedObject(*objPos, preMovePos, predictedSelfPos)) {
                            movementBlockedByWall = true;
                        }
                    }
                });
            }

            if (movementBlockedByWall) {
                predictedSelfPos.x = preMovePos.x;
                predictedSelfPos.z = preMovePos.z;
            }

            // Real bugfix, found live (round 2): making terrain the
            // universal fallback (see the previous version of this comment)
            // fixed the outdoor-approach clipping, but overcorrected -
            // self standing on an upper floor (e.g. the 3rd story) hits a
            // brief real gap in floor-mesh coverage (crossing between
            // rooms, approaching a stairwell) and got snapped all the way
            // down to GROUND-LEVEL outdoor terrain, confirmed live: "gets
            // to the edge of the cell and then snaps back to the terrain."
            // Terrain is only a sane fallback when genuinely OUTSIDE the
            // building's footprint - indoors, holding the current height
            // unchanged for that one gap frame is far safer than guessing
            // via terrain, which knows nothing about which real floor
            // self is actually standing on. This is now safe to do (it
            // wasn't, the first time this project tried it last night)
            // because two things changed since: the local-to-world Y
            // conversion bug is fixed, and cell 0's own CMSH (real entry
            // ramps/porches) is back in the floor-height search, so the
            // genuinely-outdoors-approaching-the-building case is now
            // usually covered by a real floor hit before ever reaching
            // this fallback at all.
            if (newFloorHeight.has_value()) {
                // Real floor height (Phase 20) - continuously follows the
                // real per-cell collision mesh (real stairs/ramps included),
                // replacing the old frozen-at-last-server-report behavior.
                // newFloorHeight is in the BUILDING-LOCAL frame (see
                // buildingWorldY's own comment above) - convert back to
                // world space before writing to predictedSelfPos.y.
                predictedSelfPos.y = buildingWorldY + *newFloorHeight;
            } else if (terrainSource && !selfInsideAnyBuildingFootprint) {
                predictedSelfPos.y =
                    terrainSource->queryHeight(predictedSelfPos.x, predictedSelfPos.z);
            }
            // else: hold predictedSelfPos.y unchanged - the graceful
            // fallback for a real gap in floor-mesh coverage while
            // genuinely indoors (see comment above).

            // Outbound movement report - real client cadence (MID_DELTA=
            // 400ms, see the constant's own comment), only while actually
            // moving; simply staying quiet while idle is enough (no
            // idle-sync DataTransform variant is sent - that's a separate,
            // 50-move-count-gated real client behavior this pass doesn't
            // reproduce, matching the plan's explicit MVP scope).
            //
            // FIXED (Phase 17): used to always send the plain world-space
            // DataTransform, even while indoors - a real, live-caught bug,
            // not just an authenticity gap: Core3's own server log showed
            // real "Player Speed Abnormality" anti-cheat errors (computed
            // speed 82+ units/sec against a 5.376 max), because the server,
            // having already validated self's containment in a real cell,
            // rejected a subsequent world-space report as physically
            // impossible. Sends the cell-relative buildDataTransformWithParent()
            // instead whenever predictedSelfParentId != 0, converting
            // predictedSelfPos (world-space, WASD-predicted) back into
            // local coordinates via the inverse of resolveWorldPosition()'s
            // own rotation - falls back to the plain world-space message
            // (and just accepts the same real risk this comment used to
            // describe) only if the building's transform genuinely can't be
            // resolved from this frame's snapshot, which should be rare
            // now that Step 2's containment wiring is permanent.
            if (movementKeyHeld) {
                auto elapsedSinceLastSend = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                 now - lastMovementSendTime)
                                                 .count();
                if (elapsedSinceLastSend >= kMovementSendIntervalMs) {
                    uint32_t timeStamp = static_cast<uint32_t>(
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - movementSendReferenceTime)
                            .count());
                    ++movementMoveCount;

                    // Pure Y-axis rotation quaternion from yaw - the direct
                    // inverse of ObjectStore.cpp's own yawByteFromQuaternion()
                    // (confirmed round-trip consistent against that exact
                    // formula before relying on it here).
                    float halfYaw = predictedSelfYawRadians * 0.5f;
                    float quatY = std::sin(halfYaw);
                    float quatW = std::cos(halfYaw);

                    std::optional<DirectX::XMFLOAT3> localPos;
                    if (predictedSelfParentId != 0) {
                        auto cellIt = worldSnapshot.find(predictedSelfParentId);
                        if (cellIt != worldSnapshot.end() &&
                            cellIt->second.containmentMessagesSeen > 0 &&
                            cellIt->second.containerId != 0) {
                            auto buildingIt = worldSnapshot.find(cellIt->second.containerId);
                            if (buildingIt != worldSnapshot.end() &&
                                buildingIt->second.transformMessagesSeen > 0) {
                                const worldmodel::WorldObject& building = buildingIt->second;
                                float buildingYaw =
                                    (static_cast<float>(building.direction) / 100.0f) *
                                    DirectX::XM_2PI;
                                float cosYaw = std::cos(buildingYaw);
                                float sinYaw = std::sin(buildingYaw);
                                float dx = predictedSelfPos.x - building.x;
                                float dz = predictedSelfPos.z - building.z;
                                localPos = DirectX::XMFLOAT3{dx * cosYaw - dz * sinYaw,
                                                              predictedSelfPos.y - building.y,
                                                              dx * sinYaw + dz * cosYaw};
                            }
                        }
                    }

                    // Phase 14 - queued for NetworkThread to actually send,
                    // rather than calling zoneSession.sendMessage()
                    // directly from this (render) thread - see
                    // NetworkThread's own comment for why all SoeSession
                    // access is confined to its one thread.
                    if (localPos.has_value()) {
                        networkThread.enqueueSend(swgproto::buildDataTransformWithParent(
                            selfObjectId, timeStamp, movementMoveCount, predictedSelfParentId,
                            0.0f, quatY, 0.0f, quatW, localPos->x, localPos->y, localPos->z,
                            currentSpeed));
                    } else {
                        networkThread.enqueueSend(swgproto::buildDataTransform(
                            selfObjectId, timeStamp, movementMoveCount, 0.0f, quatY, 0.0f, quatW,
                            predictedSelfPos.x, predictedSelfPos.y, predictedSelfPos.z,
                            currentSpeed));
                    }

                    lastMovementSendTime = now;
                }
            }
        }

        // Phase 18 - inspection mode drives the free FlyCamera instead of
        // the normal self-locked FollowCamera; viewMatrixNow is computed
        // once here and reused by both the main draw pass and the picking
        // ray below, rather than re-branching at each call site.
        if (inspectionMode) {
            flyCamera.update(window, deltaSeconds);
        } else {
            screenshotCapture.applyCameraOverride(camera, predictedSelfYawRadians);
            camera.update(window, predictedSelfPos, deltaSeconds);
        }
        DirectX::XMMATRIX viewMatrixNow =
            inspectionMode ? flyCamera.viewMatrix() : camera.viewMatrix(predictedSelfPos);

        gfx.beginFrame(0.05f, 0.05f, 0.08f);

        float aspect = static_cast<float>(window.width()) / static_cast<float>(window.height());
        DirectX::XMMATRIX projection =
            DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, aspect, 0.1f, 8192.0f);
        gfx.setViewProjection(viewMatrixNow, projection);

        // Real portal-based cell visibility (Phase 22) - one world-space
        // frustum built once per frame, reused for every building's
        // computeVisibleCells() call in the draw pass below (row-vector
        // convention: combined = view * projection, matching how
        // gfx.setViewProjection() itself composes them).
        renderer::Frustum cameraFrustum = renderer::Frustum::fromViewProjection(
            DirectX::XMMatrixMultiply(viewMatrixNow, projection));

        // Real terrain (Phase 8, relocated to a background thread in Phase
        // 14) - tells the asset worker thread where self is (cheap atomic
        // store; the actual generation happens over there, off this
        // thread), then drains whatever it's produced since last frame.
        // Evictions first (unbounded - just a GPU buffer destroy, cheap),
        // then new chunks (budgeted via kMaxAssetUploadsPerFrame, same
        // streaming-budget reasoning as meshCache.drainReady() below).
        // Every currently-tracked chunk is drawn unconditionally each
        // frame regardless of upload status - drawTerrainChunk() already
        // skips one whose GPU upload hasn't completed yet (Phase 14 step
        // 2), so there's no need to track readiness twice.
        assetWorker.updateSelfPosition(predictedSelfPos.x, predictedSelfPos.z);

        while (auto evictedCoord = assetWorker.terrainEvicted.tryPop()) {
            auto it = terrainChunkHandles.find(*evictedCoord);
            if (it != terrainChunkHandles.end()) {
                gfx.unloadTerrainChunk(it->second);
                terrainChunkHandles.erase(it);
            }
        }
        for (int i = 0; i < kMaxAssetUploadsPerFrame; ++i) {
            auto ready = assetWorker.terrainReady.tryPop();
            if (!ready.has_value()) {
                break;
            }
            terrainChunkHandles[ready->coord] = gfx.loadTerrainChunk(ready->meshData);
        }
        for (const auto& [coord, handle] : terrainChunkHandles) {
            gfx.drawTerrainChunk(handle);
        }

        // Real-mesh resolution (Phase 14: CPU parsing happens on
        // assetWorker's thread now - see MeshHandleCache's own comment)
        // drained before this frame's draw pass so a mesh that just became
        // ready can appear the same frame it completes, not one frame late.
        meshCache.drainReady(assetWorker, gfx, kMaxAssetUploadsPerFrame);
        skeletalMeshCache.drainReady(assetWorker, gfx, kMaxAssetUploadsPerFrame);
        buildingCache.drainReady(assetWorker, gfx, kMaxAssetUploadsPerFrame);

        // Phase 17 Step 5 - real cursor-based picking. Computed once per
        // frame (cheap) regardless of whether anything's actually
        // interactable; the draw pass below tests every drawn object's
        // already-computed world position against this same ray, so
        // there's no separate whole-scene scan.
        DirectX::XMFLOAT3 pickRayOrigin{};
        DirectX::XMFLOAT3 pickRayDir{};
        {
            int cursorX = 0;
            int cursorY = 0;
            window.cursorPosition(cursorX, cursorY);
            screenPointToWorldRay(cursorX, cursorY, window.width(), window.height(), viewMatrixNow,
                                   projection, pickRayOrigin, pickRayDir);
        }
        constexpr float kInteractionPickRadius = 1.25f;   // world units, tuned by eye
        constexpr float kMaxInteractionDistance = 12.0f;  // world units, tuned by eye
        uint64_t hoveredObjectId = 0;
        uint32_t hoveredObjectCrc = 0;
        DirectX::XMFLOAT3 hoveredObjectPos{};
        float hoveredDistance = kMaxInteractionDistance;

        // Real terrain grading (see gradedBuildingIds' own comment) - which
        // already-graded buildings are still actually present this frame,
        // used for the post-scan removal diff right after this forEach call.
        std::unordered_set<uint64_t> seenBuildingIdsThisFrame;

        objectStore.forEach([&](const auto& obj) {
            using T = std::decay_t<decltype(obj)>;
            if constexpr (std::is_same_v<T, worldmodel::CellObject> ||
                          std::is_same_v<T, worldmodel::GroupObject>) {
                return; // containers/session objects, not spatial things to draw
            } else {
                if (obj.transformMessagesSeen == 0) {
                    return;
                }
                // A non-self object whose containment chain isn't fully
                // resolved yet is skipped entirely THIS frame rather than
                // drawn at a guessed position - see resolveWorldPosition()'s
                // own comment for the real bug this replaced (silently
                // drawing at a nonsensical near-world-origin fallback).
                DirectX::XMFLOAT3 objPos;
                if (obj.isSelf) {
                    objPos = predictedSelfPos;
                } else {
                    auto resolved = resolveWorldPosition(worldSnapshot, obj);
                    if (!resolved.has_value()) {
                        return;
                    }
                    objPos = *resolved;
                }
                DirectX::XMFLOAT3 halfExtents = visualizerBoxHalfExtentsFor(obj.typeTag);
                DirectX::XMFLOAT4 color = visualizerColorFor(obj.typeTag, obj.isSelf);
                // direction is scaled 0-100 for a full turn, not degrees
                // (see UpdateTransformMessage.h's own field comment). Self
                // is drawn at the LOCALLY predicted position/facing (Phase
                // 13, player movement), not the last server-reported obj.x/
                // y/z/direction, so self's own rendered body stays in sync
                // with the camera/grid/terrain (all of which also follow
                // predictedSelfPos) rather than lagging a step behind.
                float yawRadians = obj.isSelf
                                        ? predictedSelfYawRadians
                                        : (static_cast<float>(obj.direction) / 100.0f) *
                                              DirectX::XM_2PI;

                // Phase 17 Step 5 - real cursor picking, piggybacked on this
                // same per-object pass rather than a second whole-scene
                // scan. Self can't interact with itself. Passing the
                // running `hoveredDistance` in as this call's own
                // maxDistance means each closer hit naturally tightens the
                // search, so the loop converges on the nearest interactable
                // under the cursor without a separate sort/compare step.
                if (!obj.isSelf) {
                    if (auto hitDistance = raySphereIntersect(
                            pickRayOrigin, pickRayDir, objPos, kInteractionPickRadius,
                            hoveredDistance)) {
                        hoveredObjectId = obj.objectId;
                        hoveredObjectCrc = obj.objectCrc;
                        hoveredObjectPos = objPos;
                        hoveredDistance = *hitDistance;
                    }
                }

                // Real mesh geometry (2026-07-18), where resolution
                // succeeds - see RealMeshResolver. A real mesh's own
                // vertices are already positioned relative to the object's
                // natural pivot (unlike the placeholder box primitive,
                // which is symmetric around its center and needs the
                // ground-level adjustment below), so it's drawn directly at
                // objPos with no offset.
                //
                // Real SKELETAL mesh geometry (Phase 15) - creature/player
                // objects only, since RealMeshResolver's own candidate list
                // excludes them entirely (querying it for them would only
                // ever return "not found"). Bind-pose only this phase (no
                // animation/skinning yet - see SkeletalMesh.h), drawn the
                // same way a static mesh is, just once per real submesh.
                //
                // Real BUILDING geometry (Phase 16) - tried for every
                // non-creature/player object before falling back to the
                // ordinary static-mesh path, since real buildings ("Eese's
                // House") decode as plain TangibleObject just like any
                // other item - there's no cheap typeTag signal to tell them
                // apart in advance, only the template data itself
                // (RealBuildingResolver's own empty-appearanceFilename
                // check) reveals it. A non-building object's one-time
                // buildingCache lookup fails and caches "not a building"
                // forever (same cache-forever-even-failures convention
                // every resolver here already uses), so this costs at most
                // one wasted worker-thread resolve attempt per distinct
                // real object ever seen, not a per-frame cost. All cells
                // (exterior shell + every interior room) draw at the
                // building's own resolved position - cell-relative
                // placement isn't implemented yet (Phase 16 explicitly
                // scoped to rendering, not cell-relative movement).
                const std::vector<renderer::MeshHandle>* realMesh = nullptr;
                const std::vector<renderer::MeshHandle>* skeletalSubmeshes = nullptr;
                const std::vector<CellHandles>* buildingCells = nullptr;
                bool drewAnimatedSelf = false;
                if (obj.typeTag == worldmodel::ObjectTypeTag::Creature ||
                    obj.typeTag == worldmodel::ObjectTypeTag::Player) {
                    // Phase 21 (animation), v1 self-only - resolved once,
                    // synchronously, the first time self's real objectCrc is
                    // seen here (see SelfAnimationData's own comment on the
                    // deliberate self-only scope cut - extending this to
                    // other visible creatures/players is a real, natural
                    // follow-up: give them the same treatment
                    // AssetWorkerThread already gives skeletalMeshCache,
                    // just async instead of this one-shot synchronous
                    // call). A resolution failure (no real skeleton/mesh
                    // data for this template) permanently falls back to the
                    // existing bind-pose skeletalMeshCache path for self
                    // too - same graceful degradation as every other
                    // resolver here.
                    if (obj.isSelf && !selfAnimResolveAttempted) {
                        selfAnimResolveAttempted = true;
                        selfAnimData = skeletalMeshResolver.resolveSelfAnimationData(obj.objectCrc);
                        if (selfAnimData.has_value() && !selfAnimData->meshParts.empty()) {
                            selfMeshBoneBindings.clear();
                            selfDynamicMeshes.clear();
                            for (size_t pi = 0; pi < selfAnimData->meshParts.size(); ++pi) {
                                const auto& part = selfAnimData->meshParts[pi];
                                auto binding =
                                    animation::bindMeshBoneIndices(selfAnimData->skeleton, part.boneNames);
                                // One-time diagnostics (fire once per
                                // session) - see AnimationDiagnosticLogging's
                                // own comments.
                                dummyclient::logSkeletonToMeshBoneBinding(selfAnimData->skeleton, binding, pi);
                                selfMeshBoneBindings.push_back(std::move(binding));
                                for (const auto& submesh : part.submeshes) {
                                    selfDynamicMeshes.push_back(gfx.allocateDynamicMesh(
                                        submesh.positions.size(), submesh.indices.size()));
                                }
                                dummyclient::logVertexWeightSample(pi, part.boneNames.size(), part.vertexWeights);
                            }
                        } else {
                            selfAnimData.reset();
                        }
                    }

                    if (obj.isSelf && selfAnimData.has_value() && !selfAnimData->meshParts.empty()) {
                        // Real posture/mood/weapon/combat-state context -
                        // see creatureanim::CreatureAnimationContext's own
                        // comment. Falls back to the struct's own defaults
                        // (posture=0 etc.) for any self-typed object that
                        // isn't a real CreatureObject.
                        creatureanim::CreatureAnimationContext animContext;
                        if constexpr (std::is_same_v<T, worldmodel::CreatureObject>) {
                            animContext = creatureanim::resolveAnimationContext(obj, selfIsMoving);
                        } else {
                            animContext.isMoving = selfIsMoving;
                        }
                        const char* stateName = creatureanim::stateNameFor(animContext);

                        if (!selfAnimData->stateTable.states.empty()) {
                            auto cacheIt = selfClipCache.find(stateName);
                            if (cacheIt == selfClipCache.end()) {
                                assets::AnimationSelectionContext selectionContext =
                                    creatureanim::toSelectionContext(animContext, selfAnimData->gender);
                                std::string clipPath = selfClipPathForState(
                                    selfAnimData->stateTable, stateName, selectionContext, selfIsMoving);
                                std::cout << "[ANIMDBG] resolved clip for state=" << stateName
                                           << " (posture=" << static_cast<int>(animContext.posture)
                                           << " mood=" << animContext.moodString
                                           << " weaponId=" << animContext.weaponId
                                           << " stateBitmask=" << creatureanim::describeStateBitmask(
                                                                       animContext.stateBitmask)
                                           << "): " << clipPath << "\n";
                                std::optional<assets::AnimationClipData> clip;
                                if (!clipPath.empty()) {
                                    clip = skeletalMeshResolver.resolveAnimationClip(clipPath);
                                }
                                cacheIt = selfClipCache.emplace(stateName, std::move(clip)).first;
                            }
                            const assets::AnimationClipData* clip =
                                (cacheIt->second.has_value() && !animControls.forceBindPoseOnly)
                                    ? &cacheIt->second.value()
                                    : nullptr;
                            // Real fix (2026-07-25) - see
                            // selfLastActiveClipAverageSpeed's own comment.
                            // Only real locomotion clips carry a non-zero
                            // real LOCT average speed; leaving this at 0
                            // for any other active clip means playback
                            // falls back to the original fixed rate.
                            selfLastActiveClipAverageSpeed = (clip != nullptr) ? clip->averageTranslationSpeed : 0.0f;

                            std::vector<int> clipBoneIndices;
                            if (clip != nullptr) {
                                clipBoneIndices =
                                    animation::bindClipBoneIndices(selfAnimData->skeleton, *clip);
                            }
                            auto localTransforms = animation::sampleLocalBoneTransforms(
                                selfAnimData->skeleton, clip, clipBoneIndices, selfAnimTimeSeconds,
                                animControls.rotationCompositionVariant, animControls.axisFixVariant,
                                animControls.disableAnimTranslation, &animControls.isolateBoneNames,
                                animControls.fingerCompositionVariant, animControls.fingerAxisFixVariant,
                                animControls.useRealBindPoseFormula,
                                animControls.bindRotationAxisFixVariant);
                            auto worldTransforms =
                                animation::computeWorldBoneTransforms(selfAnimData->skeleton, localTransforms);

                            // Real, numbers-only leg-motion sanity check -
                            // see AnimationDiagnosticLogging's own comment.
                            dummyclient::logLegSanityIfMoving(selfAnimData->skeleton, worldTransforms,
                                                               selfAnimTimeSeconds, selfIsMoving);

                            // Throttled named-bone state dump - see
                            // AnimationDiagnosticLogging's own comment.
                            dummyclient::logNamedBoneDump(selfAnimData->skeleton, worldTransforms,
                                                           localTransforms, clip, clipBoneIndices,
                                                           selfAnimTimeSeconds);

                            // Phase 21 diagnostic - throttle covers the
                            // WHOLE per-part/per-submesh loop below (checked
                            // once per frame here, not once per submesh),
                            // so every body part gets logged together on the
                            // same throttled frame instead of only the
                            // first part encountered.
                            static float lastVertLogTime = -1000.0f;
                            // Threshold is in selfAnimTimeSeconds' own units,
                            // which run at 30x real time (see its own
                            // `+= deltaSeconds * 30.0f` update) - 15.0f here
                            // is 0.5 REAL seconds, not 0.5 of this scaled
                            // clock (an earlier version compared directly
                            // against 0.5f, which is 0.5/30 = ~17ms of real
                            // time - effectively unthrottled, and the real
                            // cause of the console-spam complaint).
                            bool shouldLogVertsThisFrame = selfAnimTimeSeconds - lastVertLogTime > 15.0f;
                            if (shouldLogVertsThisFrame) {
                                lastVertLogTime = selfAnimTimeSeconds;
                            }

                            size_t dynamicMeshIndex = 0;
                            for (size_t partIndex = 0; partIndex < selfAnimData->meshParts.size();
                                 ++partIndex) {
                                const auto& part = selfAnimData->meshParts[partIndex];
                                const auto& meshBoneIndices = selfMeshBoneBindings[partIndex];
                                for (const auto& submesh : part.submeshes) {
                                    if (dynamicMeshIndex >= selfDynamicMeshes.size()) break;
                                    std::vector<assets::Float3> skinnedPositions;
                                    std::vector<assets::Float3> skinnedNormals;
                                    animation::skinSubmeshVertices(
                                        submesh, part.vertexWeights, selfAnimData->skeleton,
                                        meshBoneIndices, part.boneNames, worldTransforms,
                                        skinnedPositions, skinnedNormals,
                                        animControls.useRealBindPoseFormula,
                                        animControls.bindRotationAxisFixVariant);
                                    // Real per-submesh skinned-mesh
                                    // diagnostics - see
                                    // AnimationDiagnosticLogging's own
                                    // comment.
                                    dummyclient::logSkinnedSubmeshDiagnostics(
                                        partIndex, submesh, part.vertexWeights, part.boneNames,
                                        meshBoneIndices, selfAnimData->skeleton, skinnedPositions,
                                        selfAnimTimeSeconds, shouldLogVertsThisFrame);
                                    assets::MeshData meshData;
                                    meshData.positions = std::move(skinnedPositions);
                                    meshData.normals = std::move(skinnedNormals);
                                    meshData.uv0 = submesh.uv0;
                                    meshData.indices = submesh.indices;
                                    // Real GPU-handoff dump (capped
                                    // internally) - see
                                    // AnimationDiagnosticLogging's own
                                    // comment.
                                    dummyclient::logGpuHandoffIfBudgetRemains(
                                        partIndex, dynamicMeshIndex, objPos, yawRadians, meshData,
                                        selfAnimTimeSeconds);
                                    gfx.updateDynamicMesh(selfDynamicMeshes[dynamicMeshIndex], meshData);
                                    gfx.drawDynamicMesh(selfDynamicMeshes[dynamicMeshIndex], objPos,
                                                         yawRadians, color);
                                    ++dynamicMeshIndex;
                                }
                            }
                        }
                        drewAnimatedSelf = true;
                    }

                    if (!drewAnimatedSelf) {
                        skeletalSubmeshes = skeletalMeshCache.get(obj.objectCrc, assetWorker);
                    }
                } else {
                    buildingCells = buildingCache.get(obj.objectCrc, assetWorker);
                    if (buildingCells == nullptr) {
                        realMesh = meshCache.get(obj.objectCrc, assetWorker);
                    } else {
                        // Real terrain grading - register once per real
                        // building instance (objectId), the first frame its
                        // cell layout successfully resolves; already-graded
                        // instances just get marked seen for the removal
                        // diff below.
                        seenBuildingIdsThisFrame.insert(obj.objectId);
                        if (gradedBuildingIds.insert(obj.objectId).second) {
                            assetWorker.requestGrading(obj.objectId, obj.objectCrc, objPos.x, objPos.z);
                        }
                    }
                }

                if (drewAnimatedSelf) {
                    // Already drawn above via the CPU-skinned dynamic-mesh
                    // path - nothing left to do for this object.
                } else if (skeletalSubmeshes != nullptr) {
                    for (const auto& submeshHandle : *skeletalSubmeshes) {
                        gfx.drawMesh(submeshHandle, objPos, yawRadians, color);
                    }
                } else if (buildingCells != nullptr) {
                    // Phase 17 fix: a real, live-caught visual bug - drawing
                    // EVERY one of a building's real cells unconditionally,
                    // every frame, regardless of where self actually is,
                    // meant standing inside any one room showed the
                    // overlapping outlines of all 17 OTHER rooms too (each
                    // individually correctly positioned, per real .pob
                    // format research - no missing per-cell transform - but
                    // never meant to all be visible at once from a single
                    // vantage point, since the real game only shows the
                    // room you're actually in via portal-based visibility).
                    // Cell 0 (the exterior shell) always draws - it's the
                    // only cell meant to be seen from outside the building
                    // at all.
                    //
                    // Phase 19 fix: which OTHER cell counts as "self's
                    // current cell" used to come from the server-reported
                    // cellNumber (via predictedSelfParentId's own CellObject)
                    // - a real, live-caught gap found this session showed
                    // that value only ever updates right at zone-in or after
                    // a genuine server-driven relocation (an elevator ride);
                    // ordinary walking between adjacent rooms produces no
                    // such update at all, so normal mode kept rendering
                    // whatever room self was in at zone-in forever, no
                    // matter where they actually walked. Fixed by computing
                    // it locally instead: self's own already-known world
                    // position, transformed into THIS specific building
                    // instance's local mesh space, tested against each real
                    // cell's own real bounds (see CellBounds/
                    // findContainingCellIndex) - entirely independent of any
                    // server round-trip, so it stays correct through
                    // ordinary walking, not just elevator rides.
                    //
                    // Phase 18: inspection mode deliberately bypasses this
                    // filter - every cell draws at once (a "dollhouse
                    // cutaway" view for visual/scale assessment), matching
                    // the free noclip camera also active in this mode. The
                    // gameplay-accurate filter below is unchanged in spirit
                    // for normal play - just upgraded (Phase 22) from a
                    // binary "exterior + self's current cell only" gate to
                    // real portal-based visibility.
                    std::optional<size_t> selfCellIndexHere;
                    if (!inspectionMode) {
                        DirectX::XMFLOAT3 selfLocalHere = worldToBuildingLocal(predictedSelfPos, obj);
                        selfCellIndexHere = findContainingCellIndex(*buildingCells, selfLocalHere);
                    }

                    // Real portal-based cell visibility (Phase 22) - starts
                    // the portal graph walk from self's own current cell if
                    // self is inside THIS building, or cell 0 (exterior)
                    // otherwise - correctly handles both "a room becomes
                    // visible through an open door from outside" and "the
                    // exterior/another room becomes visible through an open
                    // door from inside" as the same graph. Cell 0 is always
                    // additionally forced visible, matching this project's
                    // prior "exterior always draws" behavior - the
                    // building's own ground-level shape reference shouldn't
                    // disappear just because no portal chain currently
                    // reaches it.
                    std::vector<bool> visibleCells;
                    if (!inspectionMode && !buildingCells->empty()) {
                        std::vector<std::vector<assets::CellPortal>> cellPortals;
                        cellPortals.reserve(buildingCells->size());
                        for (const auto& resolvedCell : *buildingCells) {
                            cellPortals.push_back(resolvedCell.portals);
                        }
                        size_t startCellIndex = selfCellIndexHere.value_or(0);
                        visibleCells = worldmodel::computeVisibleCells(
                            cellPortals, (*buildingCells)[0].portalShapes, startCellIndex,
                            [&](assets::Float3 localCenter, float radius) {
                                DirectX::XMFLOAT3 worldCenter = buildingLocalToWorld(
                                    DirectX::XMFLOAT3{localCenter.x, localCenter.y, localCenter.z}, obj);
                                return cameraFrustum.intersectsSphere(worldCenter, radius);
                            });
                        if (!visibleCells.empty()) {
                            visibleCells[0] = true;
                        }
                    }

                    for (size_t cellIndex = 0; cellIndex < buildingCells->size(); ++cellIndex) {
                        bool isVisible = cellIndex < visibleCells.size() && visibleCells[cellIndex];
                        if (!inspectionMode && !isVisible) {
                            continue;
                        }
                        for (const auto& submeshHandle : (*buildingCells)[cellIndex].submeshes) {
                            gfx.drawMesh(submeshHandle, objPos, yawRadians,
                                         colorForSubmesh(submeshHandle, color));
                        }
                    }
                } else if (realMesh != nullptr) {
                    for (const auto& submeshHandle : *realMesh) {
                        gfx.drawMesh(submeshHandle, objPos, yawRadians,
                                     colorForSubmesh(submeshHandle, color));
                    }
                } else {
                    // FIXED 2026-07-18: objPos is the object's ground/feet
                    // position (confirmed by the ground-grid work above), not
                    // its vertical center - drawWireBox()'s box mesh is
                    // symmetric around `center`, so the box needs to be shifted
                    // up by its own half-height to sit ON TOP of the ground
                    // instead of straddling it.
                    DirectX::XMFLOAT3 center{objPos.x, objPos.y + halfExtents.y, objPos.z};
                    gfx.drawWireBox(center, halfExtents, yawRadians, color);
                }

                // Self's own box (cyan, human-sized) is easy to lose among
                // similarly-colored/sized nearby objects, even with the
                // camera now locked to self and a name label available (see
                // below) - keeping this as a second, always-on cue costs
                // nothing extra to render.
                if (obj.isSelf) {
                    DirectX::XMFLOAT3 beaconCenter{objPos.x, objPos.y + 2.0f * halfExtents.y + 4.0f,
                                                    objPos.z};
                    DirectX::XMFLOAT3 beaconHalfExtents{0.15f, 4.0f, 0.15f};
                    DirectX::XMFLOAT4 beaconColor{1.0f, 0.2f, 1.0f, 1.0f}; // magenta
                    gfx.drawWireBox(beaconCenter, beaconHalfExtents, 0.0f, beaconColor);
                }
            }
        });

        // Real terrain grading removal - a previously-graded building no
        // longer seen this frame (real structure destroyed) gets its
        // grading un-registered. Diffed against gradedBuildingIds rather
        // than hooked directly into SceneDestroyObject's handler (main.cpp,
        // outside this function's scope) - mirrors AssetWorkerThread's own
        // terrain-chunk eviction diffing (knownTerrainCoords_ vs.
        // loadedChunks()) just above.
        for (auto it = gradedBuildingIds.begin(); it != gradedBuildingIds.end();) {
            if (seenBuildingIdsThisFrame.find(*it) == seenBuildingIdsThisFrame.end()) {
                assetWorker.requestUngrading(*it);
                it = gradedBuildingIds.erase(it);
            } else {
                ++it;
            }
        }

        // Name labels - a second pass, after every box this frame, since
        // labels use a different shader/blend/topology than
        // drawWireBox()'s (see D3D11Renderer::beginLabelPass()'s own
        // comment on why it doesn't restore box-drawing state itself;
        // beginFrame() re-establishes it fresh next frame regardless).
        // Billboarded to always face the camera, using the camera's own
        // look direction to derive a world-space right/up basis - the
        // renderer itself has no notion of "camera", only the view/
        // projection matrices already bound above.
        {
            DirectX::XMFLOAT3 camForward = camera.lookDirection();
            DirectX::XMVECTOR forwardVec = DirectX::XMLoadFloat3(&camForward);
            DirectX::XMVECTOR worldUpVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            DirectX::XMVECTOR rightVec = DirectX::XMVector3Normalize(
                DirectX::XMVector3Cross(worldUpVec, forwardVec));
            DirectX::XMVECTOR upVec = DirectX::XMVector3Cross(forwardVec, rightVec);
            DirectX::XMFLOAT3 camRight;
            DirectX::XMFLOAT3 camUp;
            DirectX::XMStoreFloat3(&camRight, rightVec);
            DirectX::XMStoreFloat3(&camUp, upVec);

            gfx.beginLabelPass();
            constexpr float kLabelWorldHeight = 0.4f; // world units - tuned by eye, not measured
            constexpr float kLabelGapAboveBox = 0.3f;
            objectStore.forEach([&](const auto& obj) {
                using T = std::decay_t<decltype(obj)>;
                if constexpr (std::is_same_v<T, worldmodel::CellObject> ||
                              std::is_same_v<T, worldmodel::GroupObject>) {
                    return;
                } else {
                    if (obj.transformMessagesSeen == 0) {
                        return;
                    }
                    // Same "skip rather than guess" rule as the mesh-drawing
                    // pass above - see resolveWorldPosition()'s own comment.
                    std::optional<DirectX::XMFLOAT3> resolvedLabelPos;
                    if (!obj.isSelf) {
                        resolvedLabelPos = resolveWorldPosition(worldSnapshot, obj);
                        if (!resolvedLabelPos.has_value()) {
                            return;
                        }
                    }
                    auto name = labelTextFor(obj);
                    if (!name.has_value()) {
                        return;
                    }
                    std::string key = toUtf8Preview(*name);
                    auto it = labelCache.find(key);
                    if (it == labelCache.end()) {
                        int pixelWidth = 0;
                        int pixelHeight = 0;
                        auto texture = gfx.createTextTexture(toWString(*name), pixelWidth, pixelHeight);
                        float aspectRatio =
                            static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);
                        it = labelCache.emplace(key, LabelTexture{texture, aspectRatio}).first;
                    }

                    DirectX::XMFLOAT3 halfExtents = visualizerBoxHalfExtentsFor(obj.typeTag);
                    // Self's label follows the same locally-predicted
                    // position as its box (Phase 13) - see the box-drawing
                    // pass above for why. objPos.y is ground/feet level and
                    // the box sits ON TOP of it, so the label's true
                    // anchor is objPos.y + 2*halfExtents.y, not
                    // objPos.y + halfExtents.y.
                    DirectX::XMFLOAT3 objPos = obj.isSelf ? predictedSelfPos : *resolvedLabelPos;
                    DirectX::XMFLOAT3 labelCenter{
                        objPos.x, objPos.y + 2.0f * halfExtents.y + kLabelGapAboveBox, objPos.z};
                    float labelHeight = kLabelWorldHeight;
                    float labelWidth = labelHeight * it->second.aspectRatio;
                    gfx.drawLabel(labelCenter, labelWidth, labelHeight, camRight, camUp,
                                  it->second.texture);
                }
            });

            // Phase 17 Step 5 - real elevator interaction. hoveredObjectId
            // was found (if at all) by the ray-cursor pick test folded into
            // the draw pass above; only elevator terminals are wired to a
            // real outbound action today (see elevatorDirectionFor()'s own
            // comment on why the plain, direction-less "terminal_elevator"
            // variant is deliberately left unhandled). A real, live-
            // verified round trip: ObjectMenuSelect -> server moves self
            // via DataTransformWithParent, picked up automatically by
            // ObjectStore/resolveWorldPosition, same as any other real
            // containment update.
            if (hoveredObjectId != 0) {
                ElevatorDirection elevatorDir =
                    elevatorDirectionFor(meshResolver.templatePathFor(hoveredObjectCrc));
                if (elevatorDir != ElevatorDirection::None) {
                    const std::u16string promptText = (elevatorDir == ElevatorDirection::Up)
                                                            ? u"[Click] Take Elevator Up"
                                                            : u"[Click] Take Elevator Down";
                    std::string key = "prompt:" + toUtf8Preview(promptText);
                    auto it = labelCache.find(key);
                    if (it == labelCache.end()) {
                        int pixelWidth = 0;
                        int pixelHeight = 0;
                        auto texture = gfx.createTextTexture(toWString(promptText), pixelWidth,
                                                               pixelHeight);
                        float aspectRatio =
                            static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);
                        it = labelCache.emplace(key, LabelTexture{texture, aspectRatio}).first;
                    }
                    DirectX::XMFLOAT3 promptCenter{hoveredObjectPos.x, hoveredObjectPos.y + 1.0f,
                                                    hoveredObjectPos.z};
                    float promptHeight = kLabelWorldHeight;
                    float promptWidth = promptHeight * it->second.aspectRatio;
                    gfx.drawLabel(promptCenter, promptWidth, promptHeight, camRight, camUp,
                                  it->second.texture);

                    bool leftMouseDown = renderer::Window::isMouseButtonDown(0);
                    if (leftMouseDown && !leftMouseWasDown) {
                        uint8_t radialId = (elevatorDir == ElevatorDirection::Up) ? 198 : 199;
                        networkThread.enqueueSend(
                            swgproto::buildObjectMenuSelect(hoveredObjectId, radialId));
                        std::cout << "[INTERACT] Sent ObjectMenuSelect objectId=" << hoveredObjectId
                                   << " radialId=" << static_cast<int>(radialId) << " (elevator "
                                   << (elevatorDir == ElevatorDirection::Up ? "up" : "down")
                                   << ")\n";
                    }
                }
            }
            leftMouseWasDown = renderer::Window::isMouseButtonDown(0);
        }

        // Top-down minimap, user-requested 2026-07-18: the main perspective
        // view alone left the user unable to tell "is that object above me,
        // or just far away" - an inherent ambiguity of any perspective
        // projection on a flat screen, not something the ground grid/walls
        // could fully resolve. A straight-down orthographic view sidesteps
        // that entirely (no depth axis to misread), at the cost of losing
        // vertical information entirely - a deliberate, explicit tradeoff,
        // not a replacement for the main view. Fixed-size corner overlay,
        // covering a 100x100 world-unit (50m radius) area centered on self,
        // recentered every frame like the ground grid/walls above.
        {
            constexpr int kMinimapSize = 480;
            constexpr int kMinimapMargin = 20;
            int minimapX = window.width() - kMinimapSize - kMinimapMargin;
            int minimapY = kMinimapMargin;
            gfx.beginMinimapPass(minimapX, minimapY, kMinimapSize, kMinimapSize);

            constexpr float kMinimapRadius = 50.0f; // world units - matches the ortho extent below
            DirectX::XMVECTOR selfVec = DirectX::XMLoadFloat3(&predictedSelfPos);
            DirectX::XMVECTOR eyeVec = DirectX::XMVectorAdd(
                selfVec, DirectX::XMVectorSet(0.0f, 100.0f, 0.0f, 0.0f));
            // Straight down, "up" = world +Z (matches the main view's own
            // yaw=0 -> +Z convention, so the minimap's orientation lines up
            // with the main scene's rather than introducing a second,
            // independent "north").
            DirectX::XMMATRIX minimapView = DirectX::XMMatrixLookAtLH(
                eyeVec, selfVec, DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
            DirectX::XMMATRIX minimapProjection = DirectX::XMMatrixOrthographicLH(
                kMinimapRadius * 2.0f, kMinimapRadius * 2.0f, 0.1f, 200.0f);
            gfx.setViewProjection(minimapView, minimapProjection);

            DirectX::XMFLOAT4 ringColor{0.4f, 0.4f, 0.45f, 1.0f};
            gfx.drawCircle(predictedSelfPos, 10.0f, ringColor);
            gfx.drawCircle(predictedSelfPos, 30.0f, ringColor);

            constexpr float kMinimapDotRadius = 1.5f;
            objectStore.forEach([&](const auto& obj) {
                using T = std::decay_t<decltype(obj)>;
                if constexpr (std::is_same_v<T, worldmodel::CellObject> ||
                              std::is_same_v<T, worldmodel::GroupObject>) {
                    return;
                } else {
                    if (obj.transformMessagesSeen == 0 || obj.isSelf) {
                        return; // self gets its own distinct marker below
                    }
                    DirectX::XMFLOAT3 dotCenter{obj.x, predictedSelfPos.y, obj.z};
                    DirectX::XMFLOAT4 color = visualizerColorFor(obj.typeTag, false);
                    gfx.drawCircle(dotCenter, kMinimapDotRadius, color);
                }
            });

            // Self: a distinct marker (matches the beacon's magenta) plus a
            // short facing tick so the minimap shows which way self is
            // looking, not just where self is.
            DirectX::XMFLOAT4 selfColor{1.0f, 0.2f, 1.0f, 1.0f};
            gfx.drawCircle(predictedSelfPos, kMinimapDotRadius * 1.5f, selfColor);

            DirectX::XMFLOAT3 facingDir{sinf(predictedSelfYawRadians), 0.0f,
                                         cosf(predictedSelfYawRadians)};
            DirectX::XMFLOAT3 facingCenter{
                predictedSelfPos.x + facingDir.x * 5.0f, predictedSelfPos.y,
                predictedSelfPos.z + facingDir.z * 5.0f};
            DirectX::XMFLOAT3 facingHalfExtents{0.4f, 0.1f, 3.0f};
            gfx.drawWireBox(facingCenter, facingHalfExtents, predictedSelfYawRadians, selfColor);
        }

        gfx.endFrame();

        // Burst screenshot capture - see ScreenshotCapture::captureIfActive's
        // own comment. Must run right after endFrame() and before the next
        // beginFrame() - see captureFrameRGBA8's own comment on why.
        screenshotCapture.captureIfActive(gfx);

        auto frameElapsed = Clock::now() - now;
        if (frameElapsed < targetFrameTime) {
            std::this_thread::sleep_for(targetFrameTime - frameElapsed);
        }
    }

    std::cout << "[VISUALIZER] window closed.\n";
}

#endif
