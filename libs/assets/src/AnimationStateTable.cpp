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
// Real Switch value->index mapping / default-index chunks (confirmed
// byte-exact 2026-07-25 against the leaked original
// StringSelectorSkeletalAnimationTemplate.cpp source - see
// AnimationNode::switchValueMap's own comment).
constexpr uint32_t kValsTag = 0x56414C53;  // 'VALS'
constexpr uint32_t kDfltTag = 0x44464C54;  // 'DFLT'

bool equalsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    return std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

// Fallback ONLY - the real client's own selection mechanism, confirmed
// byte-exact 2026-07-25 (see AnimationNode::switchValueMap's own comment)
// falls back to child index 0 whenever a Switch's real CHNK VALS doesn't
// contain the runtime value AND it has no real CHNK DFLT either -
// confirmed via reading the real runtime StringSelectorSkeletalAnimation-
// Template::getSelectionIndexForValue(), which does exactly that. Directly
// scanning real game data found this IS the actual situation for every
// real "gender" switch checked in a real live `all_m.lat` (664 states
// scanned, VALS always empty for gender, DFLT present only sometimes) -
// meaning the real, literal on-disk mechanism can't distinguish male from
// female for these states at all. Rather than trust "always pick child 0"
// (unverifiable against the real running client without further live RE,
// and structurally suspicious: real gender-specific content, e.g.
// "all_f_dnc_f_belly_loop_high.ans", does exist in the OTHER branch for at
// least one real case checked), this scans each branch's whole subtree for
// a real gender-specific clip filename - the same heuristic this project
// used before decoding VALS/DFLT, already live-verified correct. Used
// ONLY when the real VALS/DFLT data genuinely has nothing to say.
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
    // Real CHNK VALS/DFLT are direct siblings of FORM ANMS inside this
    // switch's own inner FORM 0000, coming AFTER it in real file order
    // (confirmed against the leaked original write() implementation) - a
    // recursive findFirstChunk() would be wrong here: an ANMS option can
    // itself be a nested Switch with its OWN VALS/DFLT, and a depth-first
    // search would wrongly descend into that nested subtree looking for a
    // match before ever reaching these two chunks at THIS level. Scanned as
    // direct (non-recursive) children only, for exactly that reason.
    for (const IffChunk& child : inner->children) {
        if (child.id == kValsTag) {
            soe::PacketBuffer buf = child.data;
            buf.resetReadCursor();
            uint16_t valueCount = buf.readUint16();
            node.switchValueMap.reserve(valueCount);
            for (uint16_t i = 0; i < valueCount; ++i) {
                std::string value = readNulTerminatedString(buf);
                int16_t templateIndex = static_cast<int16_t>(buf.readUint16());
                node.switchValueMap.emplace_back(std::move(value), static_cast<int>(templateIndex));
            }
        } else if (child.id == kDfltTag) {
            soe::PacketBuffer buf = child.data;
            buf.resetReadCursor();
            node.switchDefaultIndex = static_cast<int16_t>(buf.readUint16());
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

                // The REAL selection mechanism (confirmed byte-exact
                // 2026-07-25 against the leaked original
                // StringSelectorSkeletalAnimationTemplate.cpp source -
                // fetchConstAnimationTemplateForValue()): look up the
                // runtime value for this switch's own parameterName
                // directly in its real switchValueMap; if not found (or no
                // runtime value is known at all - e.g. the real "mood"
                // switch, which this project doesn't track server-driven
                // trigger state for), fall back to switchDefaultIndex - the
                // real, authoritative default, not a guessed one. This
                // replaces an earlier heuristic (scanning each branch's
                // whole subtree for a gender-specific filename) that
                // happened to produce correct results on every case tested
                // but was never the real mechanism.
                int selectedIndex = -1;
                bool isGenderSwitch = equalsIgnoreCase(node->parameterName, "gender") && !context.gender.empty();
                if (isGenderSwitch) {
                    for (const auto& valueEntry : node->switchValueMap) {
                        if (equalsIgnoreCase(valueEntry.first, context.gender)) {
                            selectedIndex = valueEntry.second;
                            break;
                        }
                    }
                }
                // Real "mood" switch - same mechanism as gender above, just
                // a different real parameter name/context value. No
                // subtree-scan fallback for mood (that heuristic is
                // specifically a gender-content-detection trick, see
                // subtreeHasGenderHint's own comment) - an unmatched real
                // mood value just falls through to switchDefaultIndex like
                // any other unresolved switch.
                bool isMoodSwitch =
                    !isGenderSwitch && equalsIgnoreCase(node->parameterName, "mood") && !context.mood.empty();
                if (isMoodSwitch) {
                    for (const auto& valueEntry : node->switchValueMap) {
                        if (equalsIgnoreCase(valueEntry.first, context.mood)) {
                            selectedIndex = valueEntry.second;
                            break;
                        }
                    }
                }
                if (selectedIndex < 0 && node->switchDefaultIndex >= 0) {
                    selectedIndex = node->switchDefaultIndex;
                }
                // Real data has nothing to say (confirmed real, not just
                // theoretical - see subtreeHasGenderHint's own comment):
                // for a gender switch specifically, fall back to the
                // filename-scan heuristic rather than blindly taking child
                // 0, since child 0 is not reliably the correct branch.
                if (selectedIndex < 0 && isGenderSwitch) {
                    bool wantsFemale = equalsIgnoreCase(context.gender, "female");
                    for (size_t i = 0; i < node->children.size(); ++i) {
                        if (subtreeHasGenderHint(node->children[i], wantsFemale)) {
                            selectedIndex = static_cast<int>(i);
                            break;
                        }
                    }
                }
                // Last-resort fallback for malformed/incomplete real data,
                // or a non-gender switch (confirmed real: "mood") this
                // project has no server-driven trigger state to resolve -
                // the state's own first real child in file order.
                if (selectedIndex < 0 || selectedIndex >= static_cast<int>(node->children.size())) {
                    selectedIndex = 0;
                }
                node = &node->children[static_cast<size_t>(selectedIndex)];
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
