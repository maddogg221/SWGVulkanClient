#include "animation/SkeletalPose.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <unordered_map>

using namespace DirectX;
using namespace assets;

namespace animation {

namespace {

// Real bone names don't reliably share the same case between a `.skt`
// skeleton and a `.ans` clip - confirmed live this session: a real
// skeleton names its bones in camelCase ("lThigh", "rThigh", "Head",
// "lForeArm"), while a real clip's own XFIN bone names are all-lowercase
// ("lthigh", "rthigh", "head", "lforearm"). Bones that happen to already
// be lowercase in both (root, spine1/2/3, neck) matched fine under a
// naive exact-string compare, masking the bug until legs/arms/head -
// which never animated at all, silently falling back to bind pose every
// frame - made it visually obvious live. Case-insensitive ASCII compare
// throughout this file's own name-matching fixes it without needing to
// know which convention any particular real file happens to use.
bool equalsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

XMVECTOR toXMQuat(const Quaternion& q) {
    return XMVectorSet(q.x, q.y, q.z, q.w);
}

// SOE's own real quaternion Hamilton product (lhs*rhs), read directly from
// the leaked original Quaternion.cpp (Quaternion::operator*) - reimplemented
// by hand rather than using DirectXMath's XMQuaternionMultiply, which
// documents computing Q2*Q1 (REVERSED argument order, for compatibility
// with its own row-vector matrix convention) - using it directly here would
// silently apply the real formula's three terms in the wrong order. This
// keeps `useRealBindPoseFormula`'s math traceable exactly back to the real
// source instead of depending on getting a DirectXMath convention subtlety
// right by accident.
XMVECTOR realQuaternionMultiply(XMVECTOR lhsV, XMVECTOR rhsV) {
    XMFLOAT4 l, r;
    XMStoreFloat4(&l, lhsV);
    XMStoreFloat4(&r, rhsV);
    // l/r are (x,y,z,w); SOE's formula (Quaternion.cpp) is stated in (w,x,y,z).
    float w = l.w * r.w - (l.x * r.x + l.y * r.y + l.z * r.z);
    float x = l.w * r.x + r.w * l.x + (l.y * r.z - l.z * r.y);
    float y = l.w * r.y + r.w * l.y + (l.z * r.x - l.x * r.z);
    float z = l.w * r.z + r.w * l.z + (l.x * r.y - l.y * r.x);
    return XMVectorSet(x, y, z, w);
}

// Samples one real per-axis scalar curve at `timeSeconds` (treated directly
// as a frame number - real .ans keyframe "frame" fields are the only time
// unit decoded so far, see AnimationClip.h's own note on CHNK INFO's
// unread header fields, which likely carry a frame rate) via linear
// interpolation between the two bracketing real keyframes, looping by
// wrapping past the channel's own last real frame.
float sampleScalarChannel(const std::vector<ScalarKeyframe>& channel, float timeSeconds,
                           float fallback) {
    if (channel.empty()) return fallback;
    if (channel.size() == 1) return channel[0].value;
    float lastFrame = static_cast<float>(channel.back().frame);
    float period = lastFrame + 1.0f;
    float t = period > 0.0f ? std::fmod(timeSeconds, period) : 0.0f;
    if (t < 0.0f) t += period;
    for (size_t i = 0; i + 1 < channel.size(); ++i) {
        float f0 = static_cast<float>(channel[i].frame);
        float f1 = static_cast<float>(channel[i + 1].frame);
        if (t >= f0 && t <= f1) {
            float alpha = f1 > f0 ? (t - f0) / (f1 - f0) : 0.0f;
            return channel[i].value + (channel[i + 1].value - channel[i].value) * alpha;
        }
    }
    return channel.back().value;
}

// Same idea as sampleScalarChannel, SLERPing between bracketing real
// keyframe quaternions instead of lerping a scalar.
XMVECTOR sampleRotationChannel(const std::vector<QuaternionKeyframe>& channel, float timeSeconds,
                                XMVECTOR fallback) {
    if (channel.empty()) return fallback;
    if (channel.size() == 1) return toXMQuat(channel[0].rotation);
    float lastFrame = static_cast<float>(channel.back().frame);
    float period = lastFrame + 1.0f;
    float t = period > 0.0f ? std::fmod(timeSeconds, period) : 0.0f;
    if (t < 0.0f) t += period;
    for (size_t i = 0; i + 1 < channel.size(); ++i) {
        float f0 = static_cast<float>(channel[i].frame);
        float f1 = static_cast<float>(channel[i + 1].frame);
        if (t >= f0 && t <= f1) {
            float alpha = f1 > f0 ? (t - f0) / (f1 - f0) : 0.0f;
            return XMQuaternionSlerp(toXMQuat(channel[i].rotation), toXMQuat(channel[i + 1].rotation),
                                      alpha);
        }
    }
    return toXMQuat(channel.back().rotation);
}

} // namespace

std::vector<int> bindMeshBoneIndices(const SkeletonData& skeleton,
                                      const std::vector<std::string>& meshBoneNames) {
    std::vector<int> result(skeleton.bones.size(), -1);
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        for (size_t j = 0; j < meshBoneNames.size(); ++j) {
            if (equalsIgnoreCase(meshBoneNames[j], skeleton.bones[i].name)) {
                result[i] = static_cast<int>(j);
                break;
            }
        }
    }
    return result;
}

std::vector<int> bindClipBoneIndices(const SkeletonData& skeleton, const AnimationClipData& clip) {
    std::vector<int> result(skeleton.bones.size(), -1);
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        for (size_t j = 0; j < clip.bones.size(); ++j) {
            if (equalsIgnoreCase(clip.bones[j].boneName, skeleton.bones[i].name)) {
                result[i] = static_cast<int>(j);
                break;
            }
        }
    }
    return result;
}

// Phase 21 live experiment: same finger bone list as
// assets::isFingerChainBone (AnimationClip.cpp) - duplicated here since
// this file doesn't share that anonymous namespace. See
// fingerCompositionVariantOverride's own comment in SkeletalPose.h.
// Widened 2026-07-23: user confirmed live that the elbow-through-wrist
// bones show the exact same collapse artifact as the fingers (not a
// separate bug, just not previously called out on its own) - the LBS
// revert this session ruled the blend ALGORITHM back in as correct, so
// whatever convention difference explains the finger sail is the prime
// suspect for the whole lower-arm chain too. Reusing the SAME override
// mechanism (rather than a separate one) so the existing 'G'/'H' live
// debug keys can A/B-test candidate fixes across the whole broken chain
// at once, not just fingers in isolation.
bool isFingerChainBoneLocal(const std::string& boneName) {
    static const char* kFingerBones[] = {"lthumb01", "lthumb02", "lindex01", "lindex02",
                                          "lring01",  "lring02",  "rthumb01", "rthumb02",
                                          "rindex01", "rindex02", "rring01",  "rring02",
                                          "lforearm", "lulna",    "lwrist",
                                          "rforearm", "rulna",    "rwrist"};
    for (const char* name : kFingerBones) {
        if (equalsIgnoreCase(boneName, name)) return true;
    }
    return false;
}

std::vector<XMMATRIX> sampleLocalBoneTransforms(const SkeletonData& skeleton,
                                                 const AnimationClipData* clip,
                                                 const std::vector<int>& clipBoneIndexForSkeletonBone,
                                                 float timeSeconds, int rotationCompositionVariant,
                                                 int axisFixVariant, bool disableAnimTranslation,
                                                 const std::vector<std::string>* isolateBoneNames,
                                                 int fingerCompositionVariantOverride,
                                                 int fingerAxisFixVariantOverride,
                                                 bool useRealBindPoseFormula) {
    std::vector<XMMATRIX> result(skeleton.bones.size());
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        const SkeletonBone& bone = skeleton.bones[i];
        XMVECTOR preRot = toXMQuat(bone.preRotation);
        XMVECTOR postRot = toXMQuat(bone.postRotation);
        XMVECTOR bindPoseRot = toXMQuat(bone.bindPoseRotation);
        XMVECTOR animRot = XMQuaternionIdentity();
        bool hasAnimRot = false;
        XMVECTOR translation =
            XMVectorSet(bone.bindTranslation.x, bone.bindTranslation.y, bone.bindTranslation.z, 0.0f);

        bool isInIsolationSet = false;
        if (isolateBoneNames != nullptr && !isolateBoneNames->empty()) {
            for (const std::string& name : *isolateBoneNames) {
                if (equalsIgnoreCase(bone.name, name)) {
                    isInIsolationSet = true;
                    break;
                }
            }
        }
        bool isolatedOut = isolateBoneNames != nullptr && !isolateBoneNames->empty() && !isInIsolationSet;
        int clipBoneIdx =
            (clip != nullptr && !isolatedOut && i < clipBoneIndexForSkeletonBone.size())
                ? clipBoneIndexForSkeletonBone[i]
                : -1;
        if (clipBoneIdx >= 0 && static_cast<size_t>(clipBoneIdx) < clip->bones.size()) {
            const AnimationBoneChannel& channel = clip->bones[clipBoneIdx];
            if (!channel.rotationKeyframes.empty()) {
                animRot = sampleRotationChannel(channel.rotationKeyframes, timeSeconds,
                                                 XMQuaternionIdentity());
                hasAnimRot = true;
                // Phase 21 live experiment: fingerAxisFixVariantOverride
                // lets finger-chain bones specifically use a DIFFERENT
                // axisFixVariant than every other bone - live evidence
                // (2026-07-22): the outlier "sail" triangle's bone origins
                // are all individually reasonable, but the mesh vertices
                // offset from those origins end up wildly displaced,
                // consistent with the ROTATION being applied to a vertex's
                // own local offset in a subtly wrong axis/sign for fingers
                // specifically - the same kind of per-bone-group axis
                // convention difference already confirmed real for arms
                // (Z-negation) and tested (inconclusively) for legs.
                int effectiveAxisFixVariant =
                    (fingerAxisFixVariantOverride >= 0 && isFingerChainBoneLocal(bone.name))
                        ? fingerAxisFixVariantOverride
                        : axisFixVariant;
                if (effectiveAxisFixVariant != 0) {
                    XMFLOAT4 v;
                    XMStoreFloat4(&v, animRot);
                    switch (effectiveAxisFixVariant) {
                        case 1: v.x = -v.x; break;
                        case 2: v.y = -v.y; break;
                        case 3: v.z = -v.z; break;
                        case 4: v.x = -v.x; v.y = -v.y; v.z = -v.z; break;
                        case 5: std::swap(v.y, v.z); break;
                        case 6: std::swap(v.y, v.z); v.z = -v.z; break;
                        default: break;
                    }
                    animRot = XMVectorSet(v.x, v.y, v.z, v.w);
                }
            }
            bool hasTranslation = !disableAnimTranslation &&
                                   (!channel.translationAxis[0].empty() ||
                                    !channel.translationAxis[1].empty() ||
                                    !channel.translationAxis[2].empty());
            if (hasTranslation) {
                // Real bug found live (Phase 21): a real translation
                // channel's own values are a small DELTA relative to the
                // bone's real bind translation, not an absolute
                // replacement - confirmed live (a real walk clip's root
                // Y-channel only ranges -0.07..-0.01, tiny, yet replacing
                // root's real bind Y outright with that tiny value
                // collapsed root from real hip height down near the
                // origin, dragging the whole hierarchy with it; adding it
                // as a delta instead keeps root near its real bind height
                // with a small real bob on top, matching a real walk
                // cycle's actual vertical motion).
                float dx = channel.translationAxis[0].empty()
                               ? 0.0f
                               : sampleScalarChannel(channel.translationAxis[0], timeSeconds, 0.0f);
                float dy = channel.translationAxis[1].empty()
                               ? 0.0f
                               : sampleScalarChannel(channel.translationAxis[1], timeSeconds, 0.0f);
                float dz = channel.translationAxis[2].empty()
                               ? 0.0f
                               : sampleScalarChannel(channel.translationAxis[2], timeSeconds, 0.0f);
                translation = XMVectorSet(bone.bindTranslation.x + dx, bone.bindTranslation.y + dy,
                                           bone.bindTranslation.z + dz, 0.0f);
            }
        }

        // Real bug found live (Phase 21): bind pose (no clip) was correct
        // with preRot*postRot, but once real animated rotations were
        // applied, some bones (legs) looked chaotic while others
        // (spine/root) looked correct - the exact composition convention
        // for combining a real `.skt` bone's preRotation/postRotation with
        // a real `.ans` clip's animated rotation was never confirmed
        // against a rendered result. See SkeletalPose.h's own comment on
        // `rotationCompositionVariant` for what each option tests.
        int effectiveCompositionVariant =
            (fingerCompositionVariantOverride >= 0 && isFingerChainBoneLocal(bone.name))
                ? fingerCompositionVariantOverride
                : rotationCompositionVariant;
        XMVECTOR rotation;
        if (useRealBindPoseFormula) {
            // Real-client-confirmed formula (read directly from the leaked
            // original source, Skeleton::calculateJointToRootTransforms /
            // BasicSkeletonTemplate::buildModelToJointTransforms - see
            // SkeletalPose.h's own comment on this parameter):
            //   animatedRotation = animationResolverRotation * bindPoseRotation
            //   localRotation    = postRotation * (animatedRotation * preRotation)
            // animationResolverRotation is identity for bind pose (no clip,
            // or this bone has none), or the clip's own decoded rotation
            // (animRot, already through the existing per-bone-group decode
            // fixes below - those address the SEPARATE, upstream question
            // of decoding a compressed keyframe correctly, not this bind
            // formula) when animated. Bypasses every
            // rotationCompositionVariant/axisFixVariant option above
            // entirely - those were empirically tuned against the OLD,
            // two-term (preRot*postRot) formula.
            XMVECTOR animationResolverRotation = hasAnimRot ? animRot : XMQuaternionIdentity();
            XMVECTOR animatedRotation = realQuaternionMultiply(animationResolverRotation, bindPoseRot);
            rotation = realQuaternionMultiply(postRot, realQuaternionMultiply(animatedRotation, preRot));
        } else if (hasAnimRot && effectiveCompositionVariant == 1) {
            rotation = animRot;
        } else if (hasAnimRot && effectiveCompositionVariant == 2) {
            rotation = XMQuaternionMultiply(XMQuaternionMultiply(postRot, animRot), preRot);
        } else if (hasAnimRot && effectiveCompositionVariant == 3) {
            rotation = XMQuaternionMultiply(XMQuaternionMultiply(animRot, preRot), postRot);
        } else if (hasAnimRot && effectiveCompositionVariant == 4) {
            // Phase 21 live experiment: the real translation-channel bug
            // (fixed earlier this session) turned out to be a DELTA on top
            // of a bone's real bind translation, not a replacement -
            // rotation composition has only ever been tested with animRot
            // INSERTED between preRot/postRot (variant 0) or as an outright
            // replacement (variants 1-3), never as a delta multiplied onto
            // the FULL real bind rotation, the same shape as the confirmed
            // translation bug. Testing that here.
            XMVECTOR bindRotation = XMQuaternionMultiply(preRot, postRot);
            rotation = XMQuaternionMultiply(bindRotation, animRot);
        } else if (hasAnimRot && effectiveCompositionVariant == 5) {
            XMVECTOR bindRotation = XMQuaternionMultiply(preRot, postRot);
            rotation = XMQuaternionMultiply(animRot, bindRotation);
        } else {
            rotation = XMQuaternionMultiply(XMQuaternionMultiply(preRot, animRot), postRot);
        }
        // Phase 21 live diagnostic: print the real ANGLE (degrees) of the
        // decoded animRot alone for arm-chain bones, to directly check
        // against a real live visual reference (the official client) for
        // whether this idle clip's real motion should be subtle or large -
        // throttled to once per ~2 REAL seconds to stay readable. `timeSeconds`
        // runs at 30x real time (see Visualizer.cpp's own
        // `selfAnimTimeSeconds += deltaSeconds * 30.0f`), so the threshold
        // here is 60.0f (2*30), not 2.0f - comparing directly against 2.0f
        // (an earlier version of this) meant ~2/30 = 67ms of real time,
        // effectively unthrottled and the real cause of a past
        // console-spam complaint.
        if (hasAnimRot) {
            static const char* kArmBonesForAngleLog[] = {"larm",     "lforearm", "lulna",   "lwrist",
                                                           "rarm",     "rforearm", "rulna",   "rwrist"};
            bool isArmBone = false;
            for (const char* n : kArmBonesForAngleLog) {
                if (equalsIgnoreCase(bone.name, n)) { isArmBone = true; break; }
            }
            if (isArmBone) {
                // Bug found during 2026-07-24 code audit: this was a single
                // SHARED static timer across all 8 arm-chain bones, not one
                // per bone - since bones are processed in skeleton order
                // every frame and lArm comes first, it always claimed the
                // throttle slot first and permanently starved every other
                // arm-chain bone from ever printing. Diagnostic-only bug
                // (never affected rendering), but it's why lforearm/lulna/
                // lwrist's real angles were never actually seen all session
                // despite the log line existing since early Phase 21.
                static std::unordered_map<std::string, float> lastAngleLogTimePerBone;
                auto logIt = lastAngleLogTimePerBone.find(bone.name);
                float lastAngleLogTime = (logIt != lastAngleLogTimePerBone.end()) ? logIt->second : -1000.0f;
                if (timeSeconds - lastAngleLogTime > 60.0f) {
                    lastAngleLogTimePerBone[bone.name] = timeSeconds;
                    XMFLOAT4 aq;
                    XMStoreFloat4(&aq, animRot);
                    float angleDeg = 2.0f * std::acos(std::min(1.0f, std::fabs(aq.w))) * (180.0f / 3.14159265f);
                    std::printf("[ANIMDBG] animRot angle: bone=%s t=%.2f angle=%.1fdeg\n", bone.name.c_str(),
                                timeSeconds, angleDeg);
                }
            }
        }
        // Phase 21 diagnostic (2026-07-23, check 1 of the wrist->finger
        // "avg displacement grows with hierarchy depth" finding) - detects
        // a hemisphere flip in the FULLY COMPOSED local rotation (after
        // preRot*animRot*postRot), not just the raw decoded animRot
        // channel alone (already checked earlier and found clean - this is
        // a genuinely different thing: postRot near an exact 180 degrees
        // can flip the COMPOSED result's hemisphere even when animRot
        // itself never does). A flip here would show up as a sudden
        // snap/tear between adjacent frames, and would compound down a
        // parent-child chain exactly as observed live.
        {
            static const char* kWatchChain[] = {"lwrist", "lring01", "lring02", "rwrist", "rring01", "rring02",
                                                 "lthigh", "lshin",  "lankle",  "ltoe",   "rthigh",  "rshin",
                                                 "rankle", "rtoe",   "root"};
            bool isWatchedChainBone = false;
            for (const char* n : kWatchChain) {
                if (equalsIgnoreCase(bone.name, n)) { isWatchedChainBone = true; break; }
            }
            if (isWatchedChainBone) {
                static std::unordered_map<std::string, XMVECTOR> lastComposedRotation;
                auto it = lastComposedRotation.find(bone.name);
                if (it != lastComposedRotation.end()) {
                    float dot = XMVectorGetX(XMQuaternionDot(rotation, it->second));
                    if (dot < 0.0f) {
                        std::printf(
                            "[ANIMDBG] HEMISPHERE FLIP composed-rotation bone=%s dot=%.4f t=%.2f\n",
                            bone.name.c_str(), dot, timeSeconds);
                    }
                }
                lastComposedRotation[bone.name] = rotation;
            }
        }
        result[i] = XMMatrixRotationQuaternion(rotation) * XMMatrixTranslationFromVector(translation);
    }
    return result;
}

std::vector<XMMATRIX> computeWorldBoneTransforms(const SkeletonData& skeleton,
                                                  const std::vector<XMMATRIX>& localTransforms) {
    std::vector<XMMATRIX> world(skeleton.bones.size());
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        int32_t parent = skeleton.bones[i].parentIndex;
        // Every real skeleton checked this project lists a bone's parent at
        // an earlier index - if that's ever violated, fall back to treating
        // the bone as its own root rather than reading an uncomputed slot.
        if (parent < 0 || static_cast<size_t>(parent) >= i) {
            // Phase 21 live diagnostic: a bone falling into this branch with
            // parent >= 0 (i.e. it HAS a real parent, just not one that's
            // been computed yet) would silently render using only its own
            // local transform - completely detached from the rest of the
            // hierarchy instead of chaining through its real parent's world
            // transform. That would look exactly like the real live "sail"
            // symptom (a wildly displaced, flat chunk of mesh) for whatever
            // real vertices are skinned to it. Printed once per distinct
            // bone (by name) the first time it's ever seen, not every
            // frame.
            if (parent >= 0) {
                static std::vector<std::string> warnedBones;
                bool alreadyWarned = false;
                for (const std::string& name : warnedBones) {
                    if (name == skeleton.bones[i].name) {
                        alreadyWarned = true;
                        break;
                    }
                }
                if (!alreadyWarned) {
                    warnedBones.push_back(skeleton.bones[i].name);
                    std::printf(
                        "[ANIMDBG] WARNING: bone=%s (index=%zu) has parentIndex=%d which is NOT "
                        "earlier in the skeleton's bone list - falling back to a detached local "
                        "transform instead of chaining through its real parent\n",
                        skeleton.bones[i].name.c_str(), i, parent);
                }
            }
            world[i] = localTransforms[i];
        } else {
            world[i] = localTransforms[i] * world[parent];
        }
    }
    return world;
}

// Real, confirmed-live facial-feature mesh bone names (jaw/eyes/lids/
// brows/lips) that have no match in the real movement skeleton - see
// skinSubmeshVertices' own comment on why ONLY these specific names get the
// `head`-fallback treatment, not every unresolved mesh bone.
bool isFacialFeatureBone(const std::string& boneName) {
    static const char* kFacialBones[] = {"jaw",     "rlowerlip", "llowerlip", "rlid",
                                          "llid",    "lbrow1",    "lbrow2",    "rbrow1",
                                          "rbrow2",  "rsmile",    "uppercenterlip", "reye",
                                          "leye",    "lsmile"};
    for (const char* name : kFacialBones) {
        if (equalsIgnoreCase(boneName, name)) return true;
    }
    return false;
}

void skinSubmeshVertices(const SkeletalMeshSubmesh& submesh,
                          const std::vector<std::vector<BoneWeight>>& vertexWeights,
                          const SkeletonData& skeleton,
                          const std::vector<int>& meshBoneIndexForSkeletonBone,
                          const std::vector<std::string>& meshBoneNames,
                          const std::vector<XMMATRIX>& worldBoneTransforms,
                          std::vector<Float3>& outPositions, std::vector<Float3>& outNormals,
                          bool useRealBindPoseFormula) {
    outPositions.resize(submesh.positions.size());
    outNormals.resize(submesh.normals.size());

    // Bind pose (clip == nullptr) world matrices, needed to build each
    // mesh bone's inverse bind pose. Must use the same useRealBindPoseFormula
    // choice as whatever built worldBoneTransforms (see this function's own
    // header comment in SkeletalPose.h) - otherwise invBind(bindPose) *
    // worldBoneTransforms would mix two different conventions.
    std::vector<XMMATRIX> bindLocal = sampleLocalBoneTransforms(
        skeleton, nullptr, {}, 0.0f, 0, 0, false, nullptr, -1, -1, useRealBindPoseFormula);
    std::vector<XMMATRIX> bindWorld = computeWorldBoneTransforms(skeleton, bindLocal);

    // Real bug found live (Phase 21): sizing this purely off the highest
    // mesh-bone-index that a SKELETON bone happens to map to (the original
    // version of this code) silently excludes any real mesh bone that has
    // NO skeleton match at all - e.g. every real facial-feature bone, which
    // sorts after the skeleton-matched bones in the real mesh's own bone
    // list (confirmed live: `head` sits at a low real index, `jaw`/`reye`/
    // etc. sit at higher ones). Those vertices' weight lookups later just
    // fell outside `skinningMatrices`' bounds entirely and got silently
    // skipped (leaving them at their raw unskinned position) via a
    // completely different code path than intended - the facial-fallback
    // logic below was being written but never actually reached for them.
    // Sizing off the real full mesh bone NAME list instead covers every
    // real mesh bone index, resolved or not.
    int maxMeshBoneIndex = static_cast<int>(meshBoneNames.size()) - 1;
    for (int idx : meshBoneIndexForSkeletonBone) {
        if (idx > maxMeshBoneIndex) maxMeshBoneIndex = idx;
    }
    std::vector<int> skeletonBoneForMeshBone(static_cast<size_t>(maxMeshBoneIndex + 1), -1);
    for (size_t i = 0; i < meshBoneIndexForSkeletonBone.size(); ++i) {
        int meshIdx = meshBoneIndexForSkeletonBone[i];
        if (meshIdx >= 0) {
            skeletonBoneForMeshBone[static_cast<size_t>(meshIdx)] = static_cast<int>(i);
        }
    }

    // Phase 21 (2026-07-24) - TWO attempts at "derive bind pose from the
    // mesh's own rigidly-bound vertex data" were tried live and BOTH
    // REVERTED, the second WORSE than the first per direct user report
    // ("even worse than when we first started... nothing connected top to
    // bottom... hands are crumpled shards"):
    //   v1: independently overrode each bone's bind-WORLD translation with
    //       no continuity constraint between bones - fragmented the mesh
    //       into disconnected pieces.
    //   v2: fixed v1's specific failure by solving for a hierarchy-
    //       consistent LOCAL translation correction instead (proper
    //       parent-to-child composition, not an independent per-bone
    //       world-space override) - validated offline on BOTH hands, at
    //       multiple frames, WITH real seam-continuity checks between
    //       adjacent bones (all passed, small real gaps, no tears) - and
    //       STILL made the live result worse, not better, when actually
    //       tested against a live private test server.
    // v2's real, most likely bug, understood only after the fact: this
    // correction is computed independently INSIDE each separate call to
    // `skinSubmeshVertices` - one call per body-part submesh (body, arms,
    // hands, head, each a SEPARATE call with only that submesh's own
    // vertex data). Bones shared across multiple body parts (root, spine,
    // lClav, lArm - the upstream chain every part hangs off of) get
    // DIFFERENT, mutually-INCONSISTENT corrections in each call: the body
    // submesh has real measurements for spine3 and corrects it one way;
    // the arms submesh has NO hand/body vertices to measure spine3/lClav
    // from, so those bones silently fall back to the UNCORRECTED
    // preRot*postRot value in that call - meaning the arm's own hierarchy
    // walk starts from a DIFFERENT upstream reference than the torso's
    // own render uses, visibly disconnecting the arm from the body. Never
    // tested for, because every offline validation (both v1 and v2) only
    // ever checked ONE mesh file (hands) in isolation - never cross-
    // submesh consistency for shared bones. A future attempt at this
    // general idea needs to compute ONE correction per bone, globally,
    // ONCE across all body parts together (not independently per
    // `skinSubmeshVertices` call) - and must be validated across
    // multiple body parts together, not one mesh file at a time, before
    // ever going live again.

    // Real bug found live (Phase 21): real facial-feature mesh bones
    // (jaw/eyes/lids/brows/lips - see isFacialFeatureBone above) have no
    // matching entry in the real movement skeleton at all (it only covers
    // ~38 real bones: root, spine, arms, legs, hands) - these are
    // expression/morph-style pseudo-bones, a different real system this
    // project doesn't implement yet. Leaving their skinning matrix at
    // identity (the default below) left their vertices frozen in raw
    // bind-pose LOCAL space while every correctly-resolved neighboring
    // vertex moves into animated WORLD space - confirmed live as a
    // stretched-mesh "spike" artifact between the frozen face and the
    // moving head. Falling back to the real `head` bone's own skinning
    // matrix instead makes these SPECIFIC vertices move RIGIDLY with the
    // head - no independent facial expression animation yet, but no more
    // stretching either.
    //
    // An earlier version of this fix applied the `head` fallback to EVERY
    // unresolved mesh bone, not just these - that regressed other,
    // previously-fine real hardpoint/attachment bones (e.g. held-item
    // points) that legitimately want the identity fallback, producing NEW
    // spike artifacts elsewhere. Confirmed live: restricting the fallback
    // to only the specific real facial-feature names below is required.
    int headSkeletonIdx = -1;
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        if (equalsIgnoreCase(skeleton.bones[i].name, "head")) {
            headSkeletonIdx = static_cast<int>(i);
            break;
        }
    }
    XMMATRIX headFallbackMatrix = XMMatrixIdentity();
    if (headSkeletonIdx >= 0) {
        XMMATRIX invBind = XMMatrixInverse(nullptr, bindWorld[static_cast<size_t>(headSkeletonIdx)]);
        headFallbackMatrix = invBind * worldBoneTransforms[static_cast<size_t>(headSkeletonIdx)];
    }

    std::vector<XMMATRIX> skinningMatrices(skeletonBoneForMeshBone.size(), XMMatrixIdentity());
    for (size_t meshIdx = 0; meshIdx < skeletonBoneForMeshBone.size(); ++meshIdx) {
        int skelIdx = skeletonBoneForMeshBone[meshIdx];
        if (skelIdx < 0) {
            if (headSkeletonIdx >= 0 && meshIdx < meshBoneNames.size() &&
                isFacialFeatureBone(meshBoneNames[meshIdx])) {
                skinningMatrices[meshIdx] = headFallbackMatrix;
            }
            continue;
        }
        XMMATRIX invBind = XMMatrixInverse(nullptr, bindWorld[static_cast<size_t>(skelIdx)]);
        skinningMatrices[meshIdx] = invBind * worldBoneTransforms[static_cast<size_t>(skelIdx)];
    }

    // Phase 21 diagnostic (2026-07-23, check 2 of the wrist->finger
    // "avg displacement grows with hierarchy depth" finding) - one-time
    // dump of the real bind-chain data (parentIndex, preRot/postRot,
    // bindTranslation) from root down to lring02, the deepest bone in the
    // confirmed-worst outlier triangle. Never actually printed end-to-end
    // for this specific chain before - only individual bones' rotation
    // sanity was checked in isolation. Looking for anything structurally
    // wrong: a bad parent link, a translation magnitude that doesn't make
    // sense for a finger-segment-sized bone, or a non-unit quaternion.
    {
        static bool printedBindChainOnce = false;
        if (!printedBindChainOnce) {
            printedBindChainOnce = true;
            int chainIdx = -1;
            for (size_t i = 0; i < skeleton.bones.size(); ++i) {
                if (equalsIgnoreCase(skeleton.bones[i].name, "lring02")) {
                    chainIdx = static_cast<int>(i);
                    break;
                }
            }
            std::printf("[ANIMDBG] === real bind chain root->lring02 ===\n");
            std::vector<int> chain;
            while (chainIdx >= 0) {
                chain.push_back(chainIdx);
                chainIdx = skeleton.bones[static_cast<size_t>(chainIdx)].parentIndex;
            }
            for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
                const SkeletonBone& cb = skeleton.bones[static_cast<size_t>(*it)];
                float preLen = std::sqrt(cb.preRotation.x * cb.preRotation.x +
                                          cb.preRotation.y * cb.preRotation.y +
                                          cb.preRotation.z * cb.preRotation.z +
                                          cb.preRotation.w * cb.preRotation.w);
                float postLen = std::sqrt(cb.postRotation.x * cb.postRotation.x +
                                           cb.postRotation.y * cb.postRotation.y +
                                           cb.postRotation.z * cb.postRotation.z +
                                           cb.postRotation.w * cb.postRotation.w);
                std::printf(
                    "[ANIMDBG]   idx=%d name=%s parent=%d preRot=(%.4f,%.4f,%.4f,%.4f len=%.4f) "
                    "postRot=(%.4f,%.4f,%.4f,%.4f len=%.4f) bindTranslation=(%.5f,%.5f,%.5f)\n",
                    *it, cb.name.c_str(), cb.parentIndex, cb.preRotation.x, cb.preRotation.y,
                    cb.preRotation.z, cb.preRotation.w, preLen, cb.postRotation.x, cb.postRotation.y,
                    cb.postRotation.z, cb.postRotation.w, postLen, cb.bindTranslation.x,
                    cb.bindTranslation.y, cb.bindTranslation.z);
            }
        }
    }

    // Phase 21 diagnostic (2026-07-23, periodic - not one-time like the
    // earlier removed version): decomposes the real skinning matrix
    // (invBind*world) for the widened elbow-through-finger chain
    // CONTINUOUSLY while playing, not just once at whatever moment happens
    // to fire first (which could easily be a near-rest-pose frame with a
    // real mismatch too small to show up). Tests whether real non-unit
    // SCALE/SHEAR appears specifically during LARGE animated rotations (a
    // walk cycle) even though an earlier one-time check found none - live
    // evidence this session showed idle only collapses the hand/wrist
    // joint, while walking additionally stretches the whole arm/leg
    // dramatically, consistent with an error that grows with rotation
    // magnitude rather than a constant offset. If scale visibly departs
    // from ~1.0 specifically while walking, that's the signature of a
    // mismatched inverse-bind-pose for this bone chain.
    {
        static auto lastPrint = std::chrono::steady_clock::time_point{};
        auto now = std::chrono::steady_clock::now();
        if (now - lastPrint > std::chrono::milliseconds(500)) {
            lastPrint = now;
            static const char* kWatchBones[] = {"lforearm", "lulna", "lwrist", "lring01"};
            for (const char* watchName : kWatchBones) {
                for (size_t meshIdx = 0; meshIdx < meshBoneNames.size() && meshIdx < skinningMatrices.size();
                     ++meshIdx) {
                    if (!equalsIgnoreCase(meshBoneNames[meshIdx], watchName)) continue;
                    XMVECTOR scale, rotQuat, trans;
                    XMMatrixDecompose(&scale, &rotQuat, &trans, skinningMatrices[meshIdx]);
                    XMFLOAT3 sf;
                    XMStoreFloat3(&sf, scale);
                    std::printf("[ANIMDBG] skinningMatrix scale meshBone=%s scale=(%.3f,%.3f,%.3f)\n",
                                watchName, sf.x, sf.y, sf.z);
                    break;
                }
            }
        }
    }

    // Real-client-confirmed algorithm (live x32dbg RE, Phase 21): standard
    // linear (matrix) blend skinning - finalPos = sum(weight_i *
    // (skinningMatrix_i * pos)), same for normals via the upper 3x3 only.
    // Traced the official client's own per-vertex blend arithmetic down to
    // this exact shape (matrix-row dot product against a local vertex
    // position, plus translation, times a per-bone weight, summed across
    // influences) - no dual-quaternion machinery anywhere in that path, so
    // this project's earlier DQS experiment (tried to eliminate a "candy
    // wrapper" collapse hypothesis on finger geometry) has been reverted.
    for (size_t i = 0; i < submesh.positions.size(); ++i) {
        uint32_t sourceIdx = i < submesh.sourceVertexIndices.size() ? submesh.sourceVertexIndices[i] : 0;
        XMVECTOR pos =
            XMVectorSet(submesh.positions[i].x, submesh.positions[i].y, submesh.positions[i].z, 1.0f);
        XMVECTOR nrm = XMVectorSet(submesh.normals[i].x, submesh.normals[i].y, submesh.normals[i].z, 0.0f);

        if (sourceIdx < vertexWeights.size() && !vertexWeights[sourceIdx].empty()) {
            XMVECTOR blendedPos = XMVectorZero();
            XMVECTOR blendedNrm = XMVectorZero();
            float totalWeight = 0.0f;
            for (const BoneWeight& bw : vertexWeights[sourceIdx]) {
                if (bw.boneIndex >= skinningMatrices.size()) continue;
                const XMMATRIX& m = skinningMatrices[bw.boneIndex];
                blendedPos = XMVectorAdd(blendedPos, XMVectorScale(XMVector3Transform(pos, m), bw.weight));
                blendedNrm =
                    XMVectorAdd(blendedNrm, XMVectorScale(XMVector3TransformNormal(nrm, m), bw.weight));
                totalWeight += bw.weight;
            }
            if (totalWeight > 0.0001f) {
                pos = XMVectorSetW(XMVectorScale(blendedPos, 1.0f / totalWeight), 1.0f);
                nrm = XMVector3Normalize(blendedNrm);
            }
        }

        XMFLOAT3 outPos;
        XMFLOAT3 outNrm;
        XMStoreFloat3(&outPos, pos);
        XMStoreFloat3(&outNrm, nrm);
        outPositions[i] = Float3{outPos.x, outPos.y, outPos.z};
        outNormals[i] = Float3{outNrm.x, outNrm.y, outNrm.z};
    }
}

} // namespace animation
