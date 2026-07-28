#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>

#include "assets/AnimationClip.h"
#include "assets/SkeletalMesh.h"
#include "assets/Skeleton.h"

namespace animation {

// Maps a `SkeletonData`'s own bone-index space onto a specific real `.mgn`
// mesh's own, independently-indexed bone-name space
// (assets::SkeletalMeshData::boneNames, passed directly rather than the
// whole SkeletalMeshData - only the name list is needed) - built once by
// NAME match per loaded skeleton+mesh combination (real per-format bone
// lists are not guaranteed to share order/count - see
// AnimationClip.h/SkeletalMesh.h's own comments). Result[i] == -1 where
// skeleton bone `i` has no matching real mesh bone (a real, expected case -
// not every skeleton bone necessarily carries mesh skin weights, e.g. a
// purely structural bone).
std::vector<int> bindMeshBoneIndices(const assets::SkeletonData& skeleton,
                                      const std::vector<std::string>& meshBoneNames);

// Same idea, for a specific `AnimationClipData`'s own bone list (its `.ans`
// file's own XFIN order). Result[i] == -1 where skeleton bone `i` has no
// channel in this particular clip (real and expected: many clips only
// animate a subset of bones - see AnimationClip.h).
std::vector<int> bindClipBoneIndices(const assets::SkeletonData& skeleton,
                                      const assets::AnimationClipData& clip);

// Computes one real-time-sampled LOCAL transform (relative to the bone's
// own parent) per skeleton bone, from `clip` at `timeSeconds` (looping,
// wrapping each real channel independently by its own real keyframe frame
// range - different bones/axes can have different real keyframe counts
// within the same clip, see AnimationClip.h). A skeleton bone with no
// channel in this clip (index == -1 in `clipBoneIndexForSkeletonBone`, or
// `clip == nullptr`) falls back to its own real bind pose unmodified - the
// same graceful per-part fallback convention used throughout this project.
// `rotationCompositionVariant` - Phase 21 live-debug aid (real bug found
// live: bind pose was correct but animated legs collapsed while animated
// spine/root looked correct - see SkeletalPose.cpp's own comment). Selects
// how a bone's real preRotation/postRotation (from the `.skt`) combine
// with its real animated rotation (from a `.ans` clip), since the correct
// convention hasn't been confirmed against a real rendered result yet:
//   0 = preRot * animRot * postRot (the standard 3D-engine convention, the
//       original guess)
//   1 = animRot alone when a clip provides one (bypasses pre/post entirely
//       - tests whether pre/post is a bind-pose-only reconstruction aid,
//       not something to recompose with real animated data)
//   2 = postRot * animRot * preRot (reversed order)
//   3 = animRot * preRot * postRot
// Defaults to 0. Remove this parameter (hardcode whichever variant proves
// correct) once live-verified.
//
// `axisFixVariant` - a second, independent live-debug aid, tried after all
// 4 `rotationCompositionVariant` options still looked wrong live: a
// real per-axis sign/swap correction applied to the DECODED animated
// quaternion before use, testing whether the real `.ans` quaternion
// data's own axis convention (handedness) differs from DirectXMath's -
// small rotations look nearly identical under a handedness mismatch
// (explaining why bind pose and small-rotation bones looked fine) while
// large rotations (a full leg swing) diverge completely (explaining the
// live symptom). 0 = no change; 1/2/3 = negate x/y/z; 4 = negate x,y,z
// (conjugate); 5 = swap y,z; 6 = swap y,z and negate the result's z.
// Defaults to 0.
// `disableAnimTranslation` - a third live-debug aid: when true, always uses
// a bone's real bind translation, ignoring any real per-axis animated
// translation channel (only `root` has one, in every real sample checked
// this session) - isolates whether the real Y-extent collapse found live
// (a real submesh's bounding box shrinking from ~1.6 units tall in bind
// pose to ~0.3-0.5 while animated) traces to the translation channel
// specifically, separate from rotation.
// `isolateBoneNames` - a fourth live-debug aid, added when the FULL clip
// (every bone animated simultaneously) kept looking badly broken live
// despite the real bind pose (zero animated bones) being perfect: every
// test up to this point exercised many simultaneously-animated bones at
// once, never a small controlled subset. When non-null and non-empty,
// ONLY bones whose name (case-insensitive) appears in this list use their
// real animated rotation/translation - every other bone is forced to its
// real bind pose, regardless of what the clip provides. Lets a real bone
// chain be built up one bone at a time (root; root+spine1; root+spine1+
// spine2; ...) to find exactly where multiple real simultaneously-
// animated bones start compounding into a visibly broken result, rather
// than testing "one bone alone" or "the whole clip" only.
// `nullptr`/empty = normal behavior (every bone with clip data animates).
//
// `fingerCompositionVariantOverride` - a fifth live-debug aid. Live
// evidence (2026-07-22): finger bones (thumb/index/ring) still show a
// flat "sail" artifact even after the real quaternion decode was
// confirmed correct (bind pose is clean, and the raw decoded rotation
// values checked offline are sane, small, smooth angles) - ruling out
// both bind data and per-keyframe decode math as the cause, leaving
// composition as the remaining suspect. Fingers sit at the deepest point
// in the whole skeleton (10 levels from root), the same kind of
// "different bone group needs different handling" pattern root and arms
// already showed for Z-negation. -1 (default) = fingers use the same
// `rotationCompositionVariant` as every other bone; 0-5 = force
// finger-chain bones specifically to use that variant instead, regardless
// of what every other bone uses.
//
// `fingerAxisFixVariantOverride` - a sixth live-debug aid, same idea as
// `fingerCompositionVariantOverride` but for `axisFixVariant` instead:
// live evidence traced the finger "sail" down to a specific outlier
// triangle whose 3 bones' own WORLD ORIGINS are all individually
// reasonable (small, sane distances apart) - the mesh VERTICES weighted
// to them end up wildly displaced instead, consistent with a vertex's own
// local offset from its bone being rotated in a subtly wrong axis/sign
// specifically for fingers, the same class of per-bone-group axis
// convention difference already confirmed real for arms (Z-negation).
// -1 (default) = fingers use the same `axisFixVariant` as every other
// bone; 0-6 = force finger-chain bones specifically to use that variant
// instead.
//
// `useRealBindPoseFormula` - a seventh live-debug aid, added 2026-07-25
// after finding a real, previously-unread `.skt` chunk (`BPRO` -
// `bindPoseRotation`, one quaternion per bone) by reading the real original
// client source (a 2015 leak of Sony Online Entertainment's own code,
// confirmed genuine via literal copyright headers). The real client
// composes a bone's local rotation as
// `postRotation * (animatedRotation * preRotation)` where `animatedRotation
// = animationResolverRotation * bindPoseRotation` (animationResolverRotation
// = identity for bind pose, or the clip's own decoded rotation when
// animated) - a genuinely different, three-term formula from every
// `rotationCompositionVariant` tried above, all of which only ever combined
// preRotation/postRotation (two terms; `bindPoseRotation` was never read at
// all before now). false (default) = every existing variant above behaves
// exactly as before, completely unaffected - this parameter is fully
// opt-in. true = use the real three-term formula for EVERY bone (bind pose
// and animated alike), via SOE's own real quaternion Hamilton product (not
// DirectXMath's XMQuaternionMultiply, which documents a reversed-argument
// convention - see SkeletalPose.cpp's own `realQuaternionMultiply`) -
// bypasses `rotationCompositionVariant`/`axisFixVariant`/the finger
// overrides entirely when true, since those were empirically tuned against
// the OLD two-term formula and are not known to still apply.
// `bindRotationAxisFixVariant` - an eighth live-debug aid, added 2026-07-28
// during the rest-pose investigation. Every prior `axisFixVariant`/Z-negation
// fix (see AnimationClip.cpp's own comment on the real, already-shipped
// Z-negation correction) was found and tested against ANIMATED per-frame
// QCHN channel data specifically - never against the STATIC per-bone
// `preRotation`/`postRotation`/`bindPoseRotation` float quaternions read
// directly in Skeleton.cpp (RPRE/RPST/BPRO), which are used completely raw.
// Real byte ORDER for those was confirmed correct against the leaked
// source's own writer, but byte order and axis CONVENTION are separate
// questions - this tests whether the same class of axis-convention mismatch
// applies here too. Real motivation: legs have exactly-identity
// preRotation/postRotation/bindPoseRotation for every bone checked (so any
// axis-convention bug in this data would be completely invisible for legs,
// explaining why the independent Blender cross-check - which only exercised
// leg/root/spine bones with identity correction data - never caught it),
// while arms have real, large, non-identity postRotation (~180°, confirmed
// legitimate Maya JointOrient data, not a decode bug) that WOULD expose
// such a bug if one exists. 0 = no change (default, current shipped
// behavior). 1-3 = negate x/y/z, 4 = negate x,y,z (conjugate), 5 = swap y,z,
// 6 = swap y,z and negate the result's z - same scheme as `axisFixVariant`,
// applied to `preRotation`/`postRotation`/`bindPoseRotation` themselves
// (every bone, before any formula/composition runs) rather than to the
// decoded animated rotation.
std::vector<DirectX::XMMATRIX> sampleLocalBoneTransforms(
    const assets::SkeletonData& skeleton, const assets::AnimationClipData* clip,
    const std::vector<int>& clipBoneIndexForSkeletonBone, float timeSeconds,
    int rotationCompositionVariant = 0, int axisFixVariant = 0, bool disableAnimTranslation = false,
    const std::vector<std::string>* isolateBoneNames = nullptr, int fingerCompositionVariantOverride = -1,
    int fingerAxisFixVariantOverride = -1, bool useRealBindPoseFormula = false,
    int bindRotationAxisFixVariant = 0);

// Walks the skeleton hierarchy (SkeletonBone::parentIndex - every real
// skeleton checked this project always lists a bone's parent at an earlier
// index) turning `localTransforms` (see sampleLocalBoneTransforms above)
// into skeleton-root-relative WORLD matrices, one per skeleton bone.
std::vector<DirectX::XMMATRIX> computeWorldBoneTransforms(
    const assets::SkeletonData& skeleton, const std::vector<DirectX::XMMATRIX>& localTransforms);

// CPU-skins one real submesh's already-flattened positions/normals (see
// SkeletalMeshSubmesh::sourceVertexIndices, added alongside this file, for
// how each flattened vertex maps back to the mesh-level
// SkeletalMeshData::vertexWeights this needs) using `worldBoneTransforms`
// (see computeWorldBoneTransforms above) combined with each bone's own
// real inverse bind pose, blended per real assets::BoneWeight - the
// standard per-vertex weighted-sum skinning every real-time 3D engine
// uses. `meshBoneIndexForSkeletonBone` (see bindMeshBoneIndices above)
// resolves the mesh's own bone-index space back to the skeleton's.
// `outPositions`/`outNormals` are resized to match `submesh.positions`;
// UV/indices are untouched (pose-independent). A vertex with no real
// weights (or whose only weights reference unmapped bones) falls back to
// its own unskinned bind-pose position/normal.
// `meshBoneNames` is the same real per-part `SkeletalMeshData::boneNames`
// list `meshBoneIndexForSkeletonBone` was built from (parallel arrays) -
// used only to recognize real facial-feature mesh bones (jaw/eyes/lids/
// brows/lips) that have no match in the real movement skeleton at all, so
// their vertices can fall back to moving rigidly with the real `head` bone
// instead of the skeleton-bind identity fallback every OTHER unresolved
// mesh bone (e.g. a real held-item hardpoint) still correctly gets - see
// this function's own definition for why blanket-applying the head
// fallback to every unresolved bone was tried and found to regress other,
// previously-fine hardpoint bones.
// `useRealBindPoseFormula` - must be passed consistently with whatever was
// used to build `worldBoneTransforms` (see sampleLocalBoneTransforms above)
// - this function independently rebuilds its own BIND POSE world matrices
// internally (to compute each bone's inverse bind pose), so the same real
// vs legacy formula choice has to apply on both sides or the resulting
// skinning matrix (invBind(bindPose) * worldBoneTransforms) would compose
// two different conventions together. Defaults to false (legacy
// preRot*postRot bind pose, unaffected).
void skinSubmeshVertices(const assets::SkeletalMeshSubmesh& submesh,
                          const std::vector<std::vector<assets::BoneWeight>>& vertexWeights,
                          const assets::SkeletonData& skeleton,
                          const std::vector<int>& meshBoneIndexForSkeletonBone,
                          const std::vector<std::string>& meshBoneNames,
                          const std::vector<DirectX::XMMATRIX>& worldBoneTransforms,
                          std::vector<assets::Float3>& outPositions,
                          std::vector<assets::Float3>& outNormals,
                          bool useRealBindPoseFormula = false, int bindRotationAxisFixVariant = 0);

} // namespace animation
