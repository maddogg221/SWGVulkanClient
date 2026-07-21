#pragma once

#include <cassert>
#include <string>
#include <utility>

namespace swgproto {

// The result of parsing a value T from the wire: either the parsed value,
// or a description of why parsing failed. Reports failure via this
// explicit, checkable result rather than throwing - same reasoning as
// soe::FragmentResult (see its class comment): a parser's only caller is
// typically MessageDispatcher::dispatch() or another parser building on
// this one, both of which wrap handling in a broad try/catch to isolate
// one bad message from the rest of the session, so an exception thrown
// here would be silently swallowed with no log trail rather than
// actionably reported.
//
// Unlike FragmentResult (a plain struct with public fields), this type
// keeps `value`/`error` private behind ok()/value()/error() so a caller
// can't read parsed data without going through ok() first - the data
// struct T itself (e.g. BaselineEnvelope) stays a pure, unconditional
// representation of "these fields were parsed," with no validity state of
// its own to forget to check. value()/error() assert their precondition
// (ok()/!ok() respectively) rather than throwing - a caller ignoring ok()
// is a caller bug to catch during development, not a runtime failure mode
// this type recovers from.
template <typename T>
class ParseResult {
public:
    static ParseResult ok(T value) {
        ParseResult result;
        result.ok_ = true;
        result.value_ = std::move(value);
        return result;
    }

    static ParseResult invalid(std::string error) {
        ParseResult result;
        result.ok_ = false;
        result.error_ = std::move(error);
        return result;
    }

    bool ok() const { return ok_; }

    // Precondition: ok().
    const T& value() const {
        assert(ok_ && "ParseResult::value() called on an Invalid result");
        return value_;
    }

    // Precondition: !ok().
    const std::string& error() const {
        assert(!ok_ && "ParseResult::error() called on an Ok result");
        return error_;
    }

private:
    ParseResult() = default;

    bool ok_ = false;
    T value_{};
    std::string error_;
};

} // namespace swgproto
