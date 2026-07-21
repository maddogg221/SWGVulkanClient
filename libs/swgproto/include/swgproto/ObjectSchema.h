#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"

namespace swgproto {

// Standard T C::* -> {ClassType, MemberType} extraction.
template <typename>
struct MemberPointerTraits;

template <typename C, typename T>
struct MemberPointerTraits<T C::*> {
    using ClassType = C;
    using MemberType = T;
};

// One field's decode behavior, type-erased down to two plain function
// pointers so a table of these carries zero per-field template/virtual
// overhead at runtime - all the type-specific work already happened at
// compile time when field<>() built this entry.
struct FieldDescriptor {
    uint16_t index = 0;          // meaningless if !deltaAddressable
    bool deltaAddressable = true;
    const char* name = nullptr; // debug/print ONLY - never compared against
    FieldKind kind{};
    // Baseline path: decode into a real struct member at `base + <this
    // field's offset, captured at field<>() call time>`.
    bool (*decodeInto)(void* base, soe::PacketBuffer& buf, std::string& error) = nullptr;
    // Delta path: decode into a local temporary and format it - deltas stay
    // print-only (no persisted object state yet), so this never touches a
    // struct pointer at all.
    bool (*decodeAndFormat)(soe::PacketBuffer& buf, std::string& valueText,
                             std::string& error) = nullptr;
};

template <FieldKind K, auto Member>
bool decodeIntoTrampoline(void* base, soe::PacketBuffer& buf, std::string& error) {
    using StructT = typename MemberPointerTraits<decltype(Member)>::ClassType;
    auto& obj = *static_cast<StructT*>(base);
    return FieldKindTraits<K>::read(obj.*Member, buf, error);
}

template <FieldKind K>
bool decodeAndFormatTrampoline(soe::PacketBuffer& buf, std::string& valueText,
                                std::string& error) {
    typename FieldKindTraits<K>::MemberType tmp{};
    if (!FieldKindTraits<K>::read(tmp, buf, error)) {
        return false;
    }
    valueText = FieldKindTraits<K>::format(tmp);
    return true;
}

// Builds one schema entry, tying a wire field index to a real struct member
// via pointer-to-member. The static_assert is the whole point of this
// design (see PLAN.md/DISCOVERY.txt's architecture discussion): a table
// entry that claims the wrong FieldKind for a member's actual C++ type
// fails to COMPILE rather than silently misdecoding at runtime.
template <FieldKind K, auto Member>
constexpr FieldDescriptor field(uint16_t index, const char* name) {
    using FieldT = typename MemberPointerTraits<decltype(Member)>::MemberType;
    static_assert(std::is_same_v<FieldT, typename FieldKindTraits<K>::MemberType>,
                  "FieldKind's expected C++ type doesn't match this struct member's actual type");
    return FieldDescriptor{index, true, name, K, &decodeIntoTrampoline<K, Member>,
                            &decodeAndFormatTrampoline<K>};
}

// For fields with no confirmed delta wire index (e.g. CreatureObjectBaseline1's
// skillList/baseHam, or PlayerObjectBaseline3's unknownTail) - they still
// participate in sequential BASELINE decode via their position in
// ObjectSchema::ownFields, but are invisible to delta lookup entirely
// (findDeltaField skips deltaAddressable==false entries) rather than being
// given a made-up index that could accidentally collide with a real one.
template <FieldKind K, auto Member>
constexpr FieldDescriptor fieldBaselineOnly(const char* name) {
    using FieldT = typename MemberPointerTraits<decltype(Member)>::MemberType;
    static_assert(std::is_same_v<FieldT, typename FieldKindTraits<K>::MemberType>,
                  "FieldKind's expected C++ type doesn't match this struct member's actual type");
    return FieldDescriptor{0, false, name, K, &decodeIntoTrampoline<K, Member>,
                            &decodeAndFormatTrampoline<K>};
}

// A type's full field schema. `ancestor` lets a schema delegate to another
// type's table without needing chained/nested pointer-to-member support
// (which standard C++ doesn't offer) - this mirrors composition, not
// inheritance: CreatureObjectBaseline3 HAS-A TangibleObjectBaseline3 (see
// its `tangible` member), it doesn't extend it, and `ancestorOffset` is
// exactly that member's offsetof.
struct ObjectSchema {
    const ObjectSchema* ancestor = nullptr;
    size_t ancestorOffset = 0; // offsetof(Derived, ancestorMember) - meaningless if ancestor == nullptr
    // MUST be declared in wire order - baseline decode reads this array
    // sequentially (see SchemaEngine.cpp's decodeBaselineInto). Delta
    // lookup doesn't care about table order, only baseline decode does.
    std::span<const FieldDescriptor> ownFields;
};

} // namespace swgproto
