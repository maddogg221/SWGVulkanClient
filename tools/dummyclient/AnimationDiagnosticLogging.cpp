#ifdef _WIN32

#include "AnimationDiagnosticLogging.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>

namespace dummyclient {

void logSkeletonToMeshBoneBinding(const assets::SkeletonData& skeleton, const std::vector<int>& binding,
                                   size_t partIndex) {
    std::cout << "[ANIMDBG] meshPart[" << partIndex << "] skeleton->meshBone: ";
    for (size_t si = 0; si < skeleton.bones.size(); ++si) {
        std::cout << skeleton.bones[si].name << "=" << binding[si] << " ";
    }
    std::cout << "\n";
}

void logVertexWeightSample(size_t partIndex, size_t boneNameCount,
                            const std::vector<std::vector<assets::BoneWeight>>& vertexWeights) {
    std::cout << "[ANIMDBG] meshPart[" << partIndex << "] boneNames.size()=" << boneNameCount
               << " vertexWeights.size()=" << vertexWeights.size() << "\n";
    for (size_t vi = 0; vi < vertexWeights.size() && vi < 15; ++vi) {
        std::cout << "[ANIMDBG]   vtx[" << vi << "] weights: ";
        float sum = 0.0f;
        for (const auto& bw : vertexWeights[vi]) {
            std::cout << "(bone=" << bw.boneIndex << " w=" << bw.weight << ") ";
            sum += bw.weight;
        }
        std::cout << " sum=" << sum << "\n";
    }
}

// Phase 21 diagnostic (2026-07-25) - real, NUMBERS-ONLY leg-motion sanity
// check, added specifically because describing what's wrong in a moving
// walk cycle via screenshots proved very hard to convey precisely. Checks
// directly from real world bone positions, no visual interpretation needed:
// (1) does the knee bulge FORWARD of the straight hip-to-ankle line (a real
// human knee always does), (2) are left/right thighs actually out of phase,
// (3) does either ankle/knee ever cross to the wrong side of the body's own
// centerline, (4) direct leg-to-leg 3D distance (mesh interpenetration can
// look "crossed" even with clean joint angles). Throttled to ~3/sec real
// time so the log stays readable.
void logLegSanityIfMoving(const assets::SkeletonData& skeleton,
                           const std::vector<DirectX::XMMATRIX>& worldTransforms, float selfAnimTimeSeconds,
                           bool isMoving) {
    if (!isMoving) {
        return;
    }
    static auto lastLegDiagTime = std::chrono::steady_clock::time_point{};
    auto nowDiag = std::chrono::steady_clock::now();
    if (nowDiag - lastLegDiagTime <= std::chrono::milliseconds(333)) {
        return;
    }
    lastLegDiagTime = nowDiag;

    auto findIdx = [&](const char* name) -> int {
        for (size_t bi = 0; bi < skeleton.bones.size(); ++bi) {
            if (skeleton.bones[bi].name.size() == std::strlen(name)) {
                bool eq = true;
                for (size_t ci = 0; ci < std::strlen(name); ++ci) {
                    if (std::tolower(static_cast<unsigned char>(skeleton.bones[bi].name[ci])) != name[ci]) {
                        eq = false;
                        break;
                    }
                }
                if (eq) return static_cast<int>(bi);
            }
        }
        return -1;
    };
    int rootIdx = findIdx("root");
    int lThighIdx = findIdx("lthigh");
    int lShinIdx = findIdx("lshin");
    int lAnkleIdx = findIdx("lankle");
    int rThighIdx = findIdx("rthigh");
    int rShinIdx = findIdx("rshin");
    int rAnkleIdx = findIdx("rankle");
    if (rootIdx < 0 || lThighIdx < 0 || lShinIdx < 0 || lAnkleIdx < 0 || rThighIdx < 0 || rShinIdx < 0 ||
        rAnkleIdx < 0) {
        return;
    }
    using namespace DirectX;
    auto worldPos = [&](int idx) {
        XMFLOAT3 p;
        XMStoreFloat3(&p, worldTransforms[static_cast<size_t>(idx)].r[3]);
        return p;
    };
    XMFLOAT3 rootPos = worldPos(rootIdx);
    // Forward direction from root's own world rotation (row 2 = local Z
    // basis vector transformed to world) - same convention as this
    // project's own yaw handling elsewhere (yaw=0 faces +Z).
    XMVECTOR fwdVec = XMVector3Normalize(worldTransforms[static_cast<size_t>(rootIdx)].r[2]);
    XMFLOAT3 fwd;
    XMStoreFloat3(&fwd, fwdVec);

    auto kneeBulge = [&](int hipIdx, int kneeIdx, int ankleIdx) {
        XMFLOAT3 hip = worldPos(hipIdx);
        XMFLOAT3 knee = worldPos(kneeIdx);
        XMFLOAT3 ankle = worldPos(ankleIdx);
        XMFLOAT3 mid{(hip.x + ankle.x) * 0.5f, (hip.y + ankle.y) * 0.5f, (hip.z + ankle.z) * 0.5f};
        XMFLOAT3 offset{knee.x - mid.x, knee.y - mid.y, knee.z - mid.z};
        return offset.x * fwd.x + offset.y * fwd.y + offset.z * fwd.z;
    };
    float lBulge = kneeBulge(lThighIdx, lShinIdx, lAnkleIdx);
    float rBulge = kneeBulge(rThighIdx, rShinIdx, rAnkleIdx);

    // Real forward/back extension of each ANKLE relative to root, along the
    // same forward axis - reveals left/right phase directly (using the
    // thigh bone's own origin barely translates; the actual swing shows up
    // at the far end of the limb).
    XMFLOAT3 lAnklePos = worldPos(lAnkleIdx);
    XMFLOAT3 rAnklePos = worldPos(rAnkleIdx);
    float lExtent = (lAnklePos.x - rootPos.x) * fwd.x + (lAnklePos.y - rootPos.y) * fwd.y +
                    (lAnklePos.z - rootPos.z) * fwd.z;
    float rExtent = (rAnklePos.x - rootPos.x) * fwd.x + (rAnklePos.y - rootPos.y) * fwd.y +
                    (rAnklePos.z - rootPos.z) * fwd.z;

    // Lateral (sideways) check - does each ankle ever cross to the WRONG
    // side of the body's own centerline, measured along the body's own
    // right vector (row 0 of root's world basis, same convention as fwd
    // using row 2).
    XMVECTOR rightVec = XMVector3Normalize(worldTransforms[static_cast<size_t>(rootIdx)].r[0]);
    XMFLOAT3 right;
    XMStoreFloat3(&right, rightVec);
    float lLateral =
        (lAnklePos.x - rootPos.x) * right.x + (lAnklePos.y - rootPos.y) * right.y + (lAnklePos.z - rootPos.z) * right.z;
    float rLateral =
        (rAnklePos.x - rootPos.x) * right.x + (rAnklePos.y - rootPos.y) * right.y + (rAnklePos.z - rootPos.z) * right.z;

    // Direct leg-to-leg 3D distance check - "clean" bone math doesn't rule
    // out the two leg MESHES visually clipping through each other if the
    // real separation is ever smaller than the model's own leg thickness.
    XMFLOAT3 lKneePos = worldPos(lShinIdx);
    XMFLOAT3 rKneePos = worldPos(rShinIdx);
    auto dist3 = [](const XMFLOAT3& a, const XMFLOAT3& b) {
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };
    float ankleDist = dist3(lAnklePos, rAnklePos);
    float kneeDist = dist3(lKneePos, rKneePos);

    // Knee LATERAL check - at peak stride extension, the swinging leg's
    // thigh/knee visibly swings inward across the body's own midline, even
    // though the ANKLE recovers back to the correct side by the time it
    // plants - a real "knock-kneed" pattern the ankle-only check couldn't
    // catch.
    float lKneeLateral =
        (lKneePos.x - rootPos.x) * right.x + (lKneePos.y - rootPos.y) * right.y + (lKneePos.z - rootPos.z) * right.z;
    float rKneeLateral =
        (rKneePos.x - rootPos.x) * right.x + (rKneePos.y - rootPos.y) * right.y + (rKneePos.z - rootPos.z) * right.z;

    std::cout << "[ANIMDBG] LEG SANITY t=" << selfAnimTimeSeconds << " kneeBulge(+fwd/-back) L=" << lBulge
               << " R=" << rBulge << "  thighExtent L=" << lExtent << " R=" << rExtent
               << " (samesign=" << ((lExtent > 0) == (rExtent > 0) ? "IN-PHASE(bad?)" : "opposite(good)")
               << ")  LATERAL(ankle) L=" << lLateral << " R=" << rLateral
               << " (crossed=" << ((lLateral > 0) == (rLateral > 0) ? "SAME-SIDE(BAD)" : "opposite(good)")
               << ")  LATERAL(knee) L=" << lKneeLateral << " R=" << rKneeLateral
               << " (crossed=" << ((lKneeLateral > 0) == (rKneeLateral > 0) ? "SAME-SIDE(BAD)" : "opposite(good)")
               << ")  ankleDist=" << ankleDist << " kneeDist=" << kneeDist << "\n";
}

void logNamedBoneDump(const assets::SkeletonData& skeleton,
                       const std::vector<DirectX::XMMATRIX>& worldTransforms,
                       const std::vector<DirectX::XMMATRIX>& localTransforms,
                       const assets::AnimationClipData* clip, const std::vector<int>& clipBoneIndices,
                       float selfAnimTimeSeconds) {
    static float lastLogTime = -1000.0f;
    if (selfAnimTimeSeconds - lastLogTime <= 60.0f) { // ~2s at 30x scale
        return;
    }
    lastLogTime = selfAnimTimeSeconds;

    auto dumpBone = [&](const char* name) {
        for (size_t bi = 0; bi < skeleton.bones.size(); ++bi) {
            if (skeleton.bones[bi].name != name) continue;
            int clipIdx = (bi < clipBoneIndices.size()) ? clipBoneIndices[bi] : -1;
            size_t kfCount = 0;
            if (clip != nullptr && clipIdx >= 0 && static_cast<size_t>(clipIdx) < clip->bones.size()) {
                kfCount = clip->bones[clipIdx].rotationKeyframes.size();
            }
            DirectX::XMFLOAT4X4 w;
            DirectX::XMStoreFloat4x4(&w, worldTransforms[bi]);
            DirectX::XMVECTOR q = DirectX::XMQuaternionRotationMatrix(localTransforms[bi]);
            DirectX::XMFLOAT4 qf;
            DirectX::XMStoreFloat4(&qf, q);
            std::cout << "[ANIMDBG] bone=" << name << " clipIdx=" << clipIdx << " keyframes=" << kfCount
                       << " localQuat=(" << qf.x << "," << qf.y << "," << qf.z << "," << qf.w
                       << ") worldPos=(" << w._41 << "," << w._42 << "," << w._43 << ")\n";
            // Real preRotation/postRotation/bindTranslation straight from
            // the real .skt skeleton data - added while chasing the finger
            // "sail" down to whether one of these is itself anomalous
            // (non-unit length, unexpectedly large) for finger bones.
            const auto& b = skeleton.bones[bi];
            float preLenSq = b.preRotation.x * b.preRotation.x + b.preRotation.y * b.preRotation.y +
                              b.preRotation.z * b.preRotation.z + b.preRotation.w * b.preRotation.w;
            float postLenSq = b.postRotation.x * b.postRotation.x + b.postRotation.y * b.postRotation.y +
                               b.postRotation.z * b.postRotation.z + b.postRotation.w * b.postRotation.w;
            std::cout << "[ANIMDBG]   preRot=(" << b.preRotation.x << "," << b.preRotation.y << ","
                       << b.preRotation.z << "," << b.preRotation.w << ") len=" << std::sqrt(preLenSq)
                       << " postRot=(" << b.postRotation.x << "," << b.postRotation.y << ","
                       << b.postRotation.z << "," << b.postRotation.w << ") len=" << std::sqrt(postLenSq)
                       << " bindTranslation=(" << b.bindTranslation.x << "," << b.bindTranslation.y << ","
                       << b.bindTranslation.z << ")\n";
            return;
        }
    };
    dumpBone("spine1");
    dumpBone("lThigh");
    dumpBone("rThigh");
    dumpBone("root");
    // Added while chasing the real finger "sail" artifact down to a
    // specific outlier triangle (lWrist/lRing01/lRing02) whose individual
    // decoded rotations all checked out sane offline - dumping their real
    // WORLD positions directly to see exactly which link in that 3-bone
    // chain the real distance jump happens at.
    dumpBone("lWrist");
    dumpBone("lRing01");
    dumpBone("lRing02");
    dumpBone("lUlna");
}

void logSkinnedSubmeshDiagnostics(size_t partIndex, const assets::SkeletalMeshSubmesh& submesh,
                                   const std::vector<std::vector<assets::BoneWeight>>& vertexWeights,
                                   const std::vector<std::string>& meshBoneNames,
                                   const std::vector<int>& meshBoneIndices,
                                   const assets::SkeletonData& skeleton,
                                   const std::vector<assets::Float3>& skinnedPositions,
                                   float selfAnimTimeSeconds, bool shouldLogThisFrame) {
    if (!shouldLogThisFrame) {
        return;
    }

    // Real bone-skinned max triangle edge length - a mis-triangulated mesh
    // (wrong vertices connected) would look fine in bind pose if those
    // vertices happen to sit close together there, but should show up
    // clearly here once bones have actually separated during animation.
    float maxEdge = 0.0f;
    size_t maxEdgeTri = 0;
    for (size_t t = 0; t + 2 < submesh.indices.size(); t += 3) {
        uint32_t ia = submesh.indices[t];
        uint32_t ib = submesh.indices[t + 1];
        uint32_t ic = submesh.indices[t + 2];
        if (ia >= skinnedPositions.size() || ib >= skinnedPositions.size() || ic >= skinnedPositions.size())
            continue;
        const auto& pa = skinnedPositions[ia];
        const auto& pb = skinnedPositions[ib];
        const auto& pc = skinnedPositions[ic];
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
    std::cout << "[ANIMDBG] t=" << selfAnimTimeSeconds << " meshPart[" << partIndex
               << "] SKINNED maxEdgeLength=" << maxEdge << " (tri#" << maxEdgeTri << ", indices="
               << (maxEdgeTri * 3 < submesh.indices.size() ? submesh.indices[maxEdgeTri * 3] : 0) << ","
               << (maxEdgeTri * 3 + 1 < submesh.indices.size() ? submesh.indices[maxEdgeTri * 3 + 1] : 0)
               << ","
               << (maxEdgeTri * 3 + 2 < submesh.indices.size() ? submesh.indices[maxEdgeTri * 3 + 2] : 0)
               << ")\n";
    // Real bone weights for the outlier triangle's 3 vertices, to directly
    // confirm whether they're weighted to clearly different (non-adjacent)
    // real bones - the smoking gun for a real mis-triangulated mesh vs. a
    // transform-only bug.
    if (maxEdge > 0.15f && maxEdgeTri * 3 + 2 < submesh.indices.size()) {
        for (int corner = 0; corner < 3; ++corner) {
            uint32_t vi = submesh.indices[maxEdgeTri * 3 + static_cast<size_t>(corner)];
            uint32_t srcIdx = vi < submesh.sourceVertexIndices.size() ? submesh.sourceVertexIndices[vi] : 0;
            std::cout << "[ANIMDBG]   outlier tri corner " << corner << " vertIdx=" << vi
                       << " sourceIdx=" << srcIdx << " weights=";
            if (srcIdx < vertexWeights.size()) {
                for (const auto& bw : vertexWeights[srcIdx]) {
                    std::string bn = bw.boneIndex < meshBoneNames.size() ? meshBoneNames[bw.boneIndex] : "?";
                    std::cout << "(" << bn << " w=" << bw.weight << ") ";
                }
            }
            std::cout << "\n";
        }
    }

    // Throttled bounding-box dump of the skinned result, to see whether
    // positions are exploding to huge/NaN values (a math bug) or stay in a
    // plausible range (a "wrong but bounded" pose - a different class of
    // bug, e.g. triangle winding/indices).
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f, minZ = 1e9f, maxZ = -1e9f;
    for (const auto& p : skinnedPositions) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
        minZ = std::min(minZ, p.z);
        maxZ = std::max(maxZ, p.z);
    }
    float uMinX = 1e9f, uMaxX = -1e9f, uMinY = 1e9f, uMaxY = -1e9f, uMinZ = 1e9f, uMaxZ = -1e9f;
    for (const auto& p : submesh.positions) {
        uMinX = std::min(uMinX, p.x);
        uMaxX = std::max(uMaxX, p.x);
        uMinY = std::min(uMinY, p.y);
        uMaxY = std::max(uMaxY, p.y);
        uMinZ = std::min(uMinZ, p.z);
        uMaxZ = std::max(uMaxZ, p.z);
    }
    float extentX = maxX - minX, extentY = maxY - minY, extentZ = maxZ - minZ;
    float maxExtent = std::max({extentX, extentY, extentZ});
    bool anomalous = maxExtent > 3.0f; // a real bind-pose body is ~2 units tall
    std::cout << "[ANIMDBG] t=" << selfAnimTimeSeconds << " meshPart[" << partIndex << "]"
               << (anomalous ? " *** ANOMALY ***" : "") << " skinned bbox: x=[" << minX << "," << maxX
               << "] y=[" << minY << "," << maxY << "] z=[" << minZ << "," << maxZ
               << "] vertCount=" << skinnedPositions.size() << " | unskinned(bind) bbox: x=[" << uMinX
               << "," << uMaxX << "] y=[" << uMinY << "," << uMaxY << "] z=[" << uMinZ << "," << uMaxZ
               << "]\n";

    // Groups this submesh's vertices by their DOMINANT real bone (highest
    // weight) and reports each bone's average real displacement (skinned
    // minus bind position) - directly identifies WHICH specific bone is
    // still causing bad geometry, rather than guessing by body region.
    std::unordered_map<int, std::pair<float, int>> dispByMeshBone; // meshBoneIdx -> (sumDisp, count)
    for (size_t vi = 0; vi < submesh.positions.size(); ++vi) {
        uint32_t sourceIdx = vi < submesh.sourceVertexIndices.size() ? submesh.sourceVertexIndices[vi] : 0;
        if (sourceIdx >= vertexWeights.size() || vertexWeights[sourceIdx].empty()) {
            continue;
        }
        const auto& weights = vertexWeights[sourceIdx];
        int dominantMeshBone = static_cast<int>(weights[0].boneIndex);
        float bestWeight = weights[0].weight;
        for (const auto& bw : weights) {
            if (bw.weight > bestWeight) {
                bestWeight = bw.weight;
                dominantMeshBone = static_cast<int>(bw.boneIndex);
            }
        }
        float dx = skinnedPositions[vi].x - submesh.positions[vi].x;
        float dy = skinnedPositions[vi].y - submesh.positions[vi].y;
        float dz = skinnedPositions[vi].z - submesh.positions[vi].z;
        float disp = std::sqrt(dx * dx + dy * dy + dz * dz);
        auto& entry = dispByMeshBone[dominantMeshBone];
        entry.first += disp;
        entry.second += 1;
    }
    std::cout << "[ANIMDBG]   per-bone avg displacement (meshPart[" << partIndex << "]): ";
    for (const auto& [meshBoneIdx, sumCount] : dispByMeshBone) {
        std::string boneName = "?";
        for (size_t si = 0; si < meshBoneIndices.size(); ++si) {
            if (meshBoneIndices[si] == meshBoneIdx) {
                boneName = skeleton.bones[si].name;
                break;
            }
        }
        std::cout << boneName << "=" << (sumCount.first / sumCount.second) << "(n=" << sumCount.second
                   << ") ";
    }
    std::cout << "\n";
}

void logGpuHandoffIfBudgetRemains(size_t partIndex, size_t dynamicMeshIndex,
                                   const DirectX::XMFLOAT3& objPos, float yawRadians,
                                   const assets::MeshData& meshData, float selfAnimTimeSeconds) {
    static int gpuHandoffLogCount = 0;
    if (gpuHandoffLogCount >= 60) {
        return;
    }
    ++gpuHandoffLogCount;
    float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f, minZ = 1e9f, maxZ = -1e9f;
    for (const auto& p : meshData.positions) {
        minX = std::min(minX, p.x);
        maxX = std::max(maxX, p.x);
        minY = std::min(minY, p.y);
        maxY = std::max(maxY, p.y);
        minZ = std::min(minZ, p.z);
        maxZ = std::max(maxZ, p.z);
    }
    std::cout << "[ANIMDBG] GPU-HANDOFF #" << gpuHandoffLogCount << " t=" << selfAnimTimeSeconds
               << " partIndex=" << partIndex << " dynamicMeshIndex=" << dynamicMeshIndex << " objPos=("
               << objPos.x << "," << objPos.y << "," << objPos.z << ") yaw=" << yawRadians
               << " vertCount=" << meshData.positions.size() << " idxCount=" << meshData.indices.size()
               << " bbox x=[" << minX << "," << maxX << "] y=[" << minY << "," << maxY << "] z=[" << minZ
               << "," << maxZ << "]\n";
}

} // namespace dummyclient

#endif
