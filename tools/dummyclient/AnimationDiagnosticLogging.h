#pragma once

// Windows/Vulkan only - only ever used by Visualizer.cpp's runVisualizer(),
// itself Windows-only (see Visualizer.h's own top-of-file comment). This
// header is safe to include unconditionally.
#ifdef _WIN32

#include <DirectXMath.h>

#include <vector>

#include "assets/AnimationClip.h"
#include "assets/SkeletalMesh.h"
#include "assets/Skeleton.h"
#include "assets/StaticMesh.h"

namespace dummyclient {

// Phase 21 (animation) live numeric diagnostics, moved out of Visualizer.cpp's
// own per-frame self-animation draw path where these used to be inline
// blocks woven directly between the real skinning/upload calls. Every
// function here is print-only (writes to std::cout) - none of them affect
// rendering, and each is self-throttling exactly as it was inline (matching
// throttle windows preserved verbatim, see each .cpp definition's own
// comment for why).

// One-time (fires once per session, the first time self's real animation
// data resolves - not per-frame) dump of which real mesh-bone index each
// skeleton bone resolved to, per body part.
void logSkeletonToMeshBoneBinding(const assets::SkeletonData& skeleton, const std::vector<int>& binding,
                                   size_t partIndex);

// Same one-time timing as above - raw real per-vertex bone-weight sample
// (first 15 vertices) for a body part, to rule out garbage bone indices or
// weights not summing near 1.0.
void logVertexWeightSample(size_t partIndex, size_t boneNameCount,
                            const std::vector<std::vector<assets::BoneWeight>>& vertexWeights);

// Throttled to ~3/sec real time, only while self is moving - real,
// numbers-only leg-motion sanity check (knee bulge direction, left/right
// gait phase, lateral ankle/knee crossing, direct leg-to-leg 3D distance).
void logLegSanityIfMoving(const assets::SkeletonData& skeleton,
                           const std::vector<DirectX::XMMATRIX>& worldTransforms, float selfAnimTimeSeconds,
                           bool isMoving);

// Throttled to ~2 real seconds - dumps a fixed list of diagnostic bones'
// real animated state (resolved clip channel index, keyframe count, decoded
// local quaternion, world position, and raw preRotation/postRotation/
// bindTranslation) for direct comparison between bones that look correct
// and bones that don't.
void logNamedBoneDump(const assets::SkeletonData& skeleton,
                       const std::vector<DirectX::XMMATRIX>& worldTransforms,
                       const std::vector<DirectX::XMMATRIX>& localTransforms,
                       const assets::AnimationClipData* clip, const std::vector<int>& clipBoneIndices,
                       float selfAnimTimeSeconds);

// Call right after animation::skinSubmeshVertices, before uploading to the
// GPU - `shouldLogThisFrame` is the caller's own throttle (shared across the
// whole per-part/per-submesh loop, computed once per frame, not once per
// submesh - see Visualizer.cpp's own call site). Real diagnostics: max
// triangle edge length after skinning (a mis-triangulation/hemisphere-flip
// detector, with a follow-up per-corner bone-weight dump if it's large),
// skinned-vs-bind bounding box comparison (flags real explosions/NaNs vs. a
// "wrong but bounded" pose), and per-dominant-bone average vertex
// displacement (identifies exactly which bone is causing bad geometry).
void logSkinnedSubmeshDiagnostics(size_t partIndex, const assets::SkeletalMeshSubmesh& submesh,
                                   const std::vector<std::vector<assets::BoneWeight>>& vertexWeights,
                                   const std::vector<std::string>& meshBoneNames,
                                   const std::vector<int>& meshBoneIndices,
                                   const assets::SkeletonData& skeleton,
                                   const std::vector<assets::Float3>& skinnedPositions,
                                   float selfAnimTimeSeconds, bool shouldLogThisFrame);

// Unthrottled but capped internally at 60 total real calls across the whole
// process lifetime - dumps exactly what's handed to the GPU for the first
// several real self-skinning calls, to remove any doubt about timing
// relative to the throttled per-bone diagnostics above.
void logGpuHandoffIfBudgetRemains(size_t partIndex, size_t dynamicMeshIndex,
                                   const DirectX::XMFLOAT3& objPos, float yawRadians,
                                   const assets::MeshData& meshData, float selfAnimTimeSeconds);

} // namespace dummyclient

#endif
