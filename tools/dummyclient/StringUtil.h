#pragma once

#include <string>

// Shared between main.cpp and Visualizer.cpp - was previously duplicated
// inside main.cpp's own anonymous namespace (internal linkage, so it
// couldn't just be forward-declared across translation units once the
// visualizer code moved into its own .cpp).
inline std::string toUtf8Preview(const std::u16string& s) {
    std::string out;
    out.reserve(s.size());
    for (char16_t ch : s) {
        out.push_back(static_cast<char>(ch));
    }
    return out;
}
