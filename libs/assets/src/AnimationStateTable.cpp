#include "assets/AnimationStateTable.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "assets/IffReader.h"

namespace assets {

namespace {
constexpr uint32_t kLattTag = 0x4C415454;  // 'LATT'
constexpr uint32_t kAnimTag = 0x414E494D;  // 'ANIM'
constexpr uint32_t kInfoTag = 0x494E464F;  // 'INFO'
constexpr uint32_t kPxatTag = 0x50584154;  // 'PXAT'
constexpr uint32_t kSsatTag = 0x53534154;  // 'SSAT'
constexpr uint32_t kSpatTag = 0x53504154;  // 'SPAT'
constexpr uint32_t kAgatTag = 0x41474154;  // 'AGAT'
constexpr uint32_t kAnmsTag = 0x414E4D53;  // 'ANMS'
constexpr uint32_t kLoopTag = 0x4C4F4F50;  // 'LOOP'
constexpr uint32_t kActnTag = 0x4143544E;  // 'ACTN'
constexpr uint32_t kPunfTag = 0x50554E46;  // 'PUNF'

bool equalsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

// Whether ANY real clip anywhere in this subtree carries a real
// gender-specific filename prefix ("all_f_"/"all_m_") - scanning the WHOLE
// branch rather than just whichever single clip a default selection would
// land on, since the real DEFAULT (untriggered) clip in a real gender
// branch is often a shared, non-gendered file ("all_b_...") - only some
// specific mood variants (e.g. a real female-specific sitting/dance
// animation) actually carry the gendered prefix, so only a full-subtree
// scan reliably tells two branches apart.
bool subtreeHasGenderHint(const AnimationNode& node, bool wantFemale) {
    if (node.kind == AnimationNodeKind::Clip) {
        const char* marker = wantFemale ? "/all_f_" : "/all_m_";
        return node.clipPath.find(marker) != std::string::npos;
    }
    for (const AnimationNode& child : node.children) {
        if (subtreeHasGenderHint(child, wantFemale)) return true;
    }
    return false;
}

// The real inner FORM every SSAT/SPAT/AGAT/PXAT wraps its own real content
// in - always exactly one child, a version-numbered wrapper FORM (every
// real sample checked this session is "0000", but selecting the first
// FORM child generically rather than assuming the exact tag matches this
// project's established convention for other version-wrapped formats,
// e.g. Skeleton.cpp's own FORM SKTM handling).
const IffChunk* innerForm(const IffChunk& node) {
    for (const IffChunk& child : node.children) {
        if (child.id == kFormTag) return &child;
    }
    return nullptr;
}

std::vector<std::string> parseActn(const IffChunk& actnChunk) {
    // Real CHNK ACTN: leading uint16 count, then `count` real
    // [NUL-terminated trigger name][float32 weight] pairs - confirmed by
    // hand-decoding several real samples against their own declared chunk
    // size exactly. The weight isn't consumed yet (no real server-driven
    // mood/trigger state is tracked by this project - see
    // AnimationSelectionContext's own comment).
    soe::PacketBuffer buf = actnChunk.data;
    buf.resetReadCursor();
    uint16_t count = buf.readUint16();
    std::vector<std::string> names;
    names.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        names.push_back(readNulTerminatedString(buf));
        buf.readFloat();  // real weight, not consumed yet
    }
    return names;
}

AnimationNode parseNode(const IffChunk& form);

AnimationNode parseClip(const IffChunk& form) {
    AnimationNode node;
    node.kind = AnimationNodeKind::Clip;
    const IffChunk* inner = innerForm(form);
    if (inner == nullptr) return node;
    if (const IffChunk* info = findFirstChunk(*inner, kInfoTag)) {
        soe::PacketBuffer buf = info->data;
        buf.resetReadCursor();
        node.clipPath = readNulTerminatedString(buf);
    }
    if (const IffChunk* punf = findFirstChunk(*inner, kPunfTag)) {
        soe::PacketBuffer buf = punf->data;
        buf.resetReadCursor();
        node.punfParameterName = readNulTerminatedString(buf);
    }
    return node;
}

AnimationNode parseSwitch(const IffChunk& form) {
    AnimationNode node;
    node.kind = AnimationNodeKind::Switch;
    const IffChunk* inner = innerForm(form);
    if (inner == nullptr) return node;
    if (const IffChunk* info = findFirstChunk(*inner, kInfoTag)) {
        soe::PacketBuffer buf = info->data;
        buf.resetReadCursor();
        node.parameterName = readNulTerminatedString(buf);
    }
    for (const IffChunk& child : inner->children) {
        if (child.id != kFormTag || child.formType != kAnmsTag) continue;
        for (const IffChunk& option : child.children) {
            if (option.id != kFormTag) continue;  // skip ANMS's own leading CHNK INFO count
            node.children.push_back(parseNode(option));
        }
    }
    return node;
}

AnimationNode parseContainer(const IffChunk& form) {
    AnimationNode node;
    node.kind = AnimationNodeKind::Container;
    const IffChunk* inner = innerForm(form);
    if (inner == nullptr) return node;
    for (const IffChunk& child : inner->children) {
        if (child.id != kFormTag) continue;  // skip SPAT's own leading CHNK INFO byte
        node.children.push_back(parseNode(child));
    }
    return node;
}

AnimationNode parseVariant(const IffChunk& form) {
    AnimationNode node;
    node.kind = AnimationNodeKind::Variant;
    const IffChunk* inner = innerForm(form);
    if (inner == nullptr) return node;
    if (const IffChunk* info = findFirstChunk(*inner, kInfoTag)) {
        soe::PacketBuffer buf = info->data;
        buf.resetReadCursor();
        if (info->data.size() >= 8) {
            node.minDurationSeconds = buf.readFloat();
            node.maxDurationSeconds = buf.readFloat();
        }
    }
    if (const IffChunk* actn = findFirstChunk(*inner, kActnTag)) {
        node.triggerNames = parseActn(*actn);
    }
    for (const IffChunk& child : inner->children) {
        if (child.id == kFormTag && child.formType == kLoopTag) {
            const IffChunk* wrapped = innerForm(child);
            if (wrapped != nullptr) {
                node.children.push_back(parseNode(*wrapped));
            }
        }
    }
    return node;
}

// Dispatches purely by real FORM type - see AnimationStateTable.h's own
// comment on each AnimationNodeKind for what real data each maps to.
// Unrecognized real FORM types (e.g. TSCL, not yet needed for the basic
// posture/locomotion set this project uses - see the original plan's own
// note on this) parse as an empty Clip node rather than throwing, matching
// this project's "tolerate unknown real content, don't fail the whole
// parse" convention elsewhere (e.g. AnimationStateTable::parse's own
// per-state tolerance below).
AnimationNode parseNode(const IffChunk& form) {
    if (form.formType == kPxatTag) return parseClip(form);
    if (form.formType == kSsatTag) return parseSwitch(form);
    if (form.formType == kSpatTag) return parseContainer(form);
    if (form.formType == kAgatTag) return parseVariant(form);
    return AnimationNode{};
}

}  // namespace

AnimationStateTableData AnimationStateTable::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag || topLevel[0].formType != kLattTag) {
        throw std::runtime_error("AnimationStateTable::parse: not a FORM(LATT)-rooted file");
    }
    const IffChunk& latt = topLevel[0];
    if (latt.children.empty() || latt.children[0].id != kFormTag) {
        throw std::runtime_error("AnimationStateTable::parse: missing state-table FORM");
    }
    const IffChunk& tableForm = latt.children[0];

    AnimationStateTableData result;
    for (const IffChunk& child : tableForm.children) {
        if (child.id != kFormTag || child.formType != kAnimTag) {
            continue;
        }
        const IffChunk* nameChunk = findFirstChunk(child, kInfoTag);
        if (nameChunk == nullptr) {
            continue;  // tolerate a malformed individual state, don't fail the whole table
        }
        soe::PacketBuffer nameBuf = nameChunk->data;
        nameBuf.resetReadCursor();

        AnimationState state;
        state.name = readNulTerminatedString(nameBuf);
        // The state's own direct real content FORM is whichever FORM
        // child follows its own leading CHNK INFO (name) - a real PXAT,
        // SSAT, SPAT, or AGAT, per AnimationNode's own comment.
        for (const IffChunk& stateChild : child.children) {
            if (stateChild.id == kFormTag) {
                state.root = parseNode(stateChild);
                break;
            }
        }
        result.states.push_back(std::move(state));
    }
    return result;
}

std::string selectAnimationClip(const AnimationNode& root, const AnimationSelectionContext& context,
                                 bool preferLocomotion) {
    const AnimationNode* node = &root;
    while (true) {
        switch (node->kind) {
            case AnimationNodeKind::Clip:
                return node->clipPath;

            case AnimationNodeKind::Variant:
                if (node->children.empty()) return {};
                node = &node->children[0];
                continue;

            case AnimationNodeKind::Switch: {
                if (node->children.empty()) return {};
                if (equalsIgnoreCase(node->parameterName, "gender") && !context.gender.empty()) {
                    // Real branches aren't labelled with their own gender
                    // anywhere directly decoded yet - classified instead
                    // by scanning each branch's WHOLE subtree for any real
                    // gender-specific clip (see subtreeHasGenderHint's own
                    // comment on why a full scan, not just the default
                    // selected clip, is needed).
                    bool wantsFemale = equalsIgnoreCase(context.gender, "female");
                    const AnimationNode* matched = nullptr;
                    for (const AnimationNode& option : node->children) {
                        if (subtreeHasGenderHint(option, wantsFemale)) {
                            matched = &option;
                            break;
                        }
                    }
                    // No branch carries a real gender-specific clip at all
                    // for this particular state (most states are fully
                    // shared, "all_b_") - branches are otherwise
                    // structurally interchangeable for selection purposes
                    // here, so just take the first.
                    node = matched != nullptr ? matched : &node->children[0];
                    continue;
                }
                // Any other real switch (confirmed real: "mood") - no
                // server-driven trigger state is tracked, so this can't
                // pick a real, triggered variant. The naive-looking
                // "prefer a Variant with an empty real ACTN trigger list"
                // heuristic was tried and found WRONG against real data:
                // a real, empty-ACTN entry can just as easily be a
                // special always-technically-untriggered filler (a real
                // hired-entertainer dance/music loop was found this way,
                // tagged `PUNF="zero_speed"` rather than being a normal
                // idle at all) as it can be a genuine default. What DOES
                // hold, confirmed against real data for multiple real
                // states/branches: the state's own first real child in
                // file order is consistently the intended default idle
                // (e.g. a real "breathe calmly" variant, first in both
                // real gender branches of a real checked state) - simpler
                // and, unlike the trigger-based guess, actually correct.
                node = &node->children[0];
                continue;
            }

            case AnimationNodeKind::Container: {
                if (node->children.empty()) return {};
                // Real locomotion-tagged clips sit as siblings of the
                // mood tree, not inside it - explicitly steer toward or
                // away from them per the caller's own real intent (a
                // resting selection skips them; a movement selection
                // prefers them) rather than picking whichever happens to
                // come first in real file order.
                const AnimationNode* locomotion = nullptr;
                const AnimationNode* nonLocomotion = nullptr;
                for (const AnimationNode& child : node->children) {
                    bool isLocomotion =
                        child.kind == AnimationNodeKind::Clip && equalsIgnoreCase(child.punfParameterName, "locomotion");
                    if (isLocomotion && locomotion == nullptr) {
                        locomotion = &child;
                    } else if (!isLocomotion && nonLocomotion == nullptr) {
                        nonLocomotion = &child;
                    }
                }
                if (preferLocomotion && locomotion != nullptr) {
                    node = locomotion;
                } else if (nonLocomotion != nullptr) {
                    node = nonLocomotion;
                } else {
                    node = &node->children[0];
                }
                continue;
            }
        }
        return {};
    }
}

}  // namespace assets
