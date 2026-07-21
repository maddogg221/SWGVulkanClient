#pragma once

// Private (not under include/terrain) - IFF tag constants shared across the
// TGEN generator parser (Boundary/Filter/Affector/Layer/Family). Kept in one
// place rather than duplicated per .cpp (unlike TrnFile.cpp's small local
// tag set) since the generator alone needs several dozen distinct 4-char
// tags - see PHASE_11_STATUS.md and this library's own real-bytes version
// scan (check_tgen, since removed) for how these were confirmed.

#include <cstdint>

namespace terrain {

constexpr uint32_t makeTag(char a, char b, char c, char d) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(a)) << 24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 8) |
           static_cast<uint32_t>(static_cast<uint8_t>(d));
}

// Version FORM tags ("0000".."0008") are always ASCII digits - every real
// version number this library needs is a single digit.
constexpr uint32_t versionTag(int n) {
    return makeTag('0', '0', '0', static_cast<char>('0' + n));
}

// -- common --
constexpr uint32_t kTagIHDR = makeTag('I', 'H', 'D', 'R');
constexpr uint32_t kTagDATA = makeTag('D', 'A', 'T', 'A');
constexpr uint32_t kTagPARM = makeTag('P', 'A', 'R', 'M');
constexpr uint32_t kTagADTA = makeTag('A', 'D', 'T', 'A');
constexpr uint32_t kTagLYRS = makeTag('L', 'Y', 'R', 'S');
constexpr uint32_t kTagLAYR = makeTag('L', 'A', 'Y', 'R');
constexpr uint32_t kTagTGEN = makeTag('T', 'G', 'E', 'N');

// -- boundaries --
constexpr uint32_t kTagBALL = makeTag('B', 'A', 'L', 'L'); // dead
constexpr uint32_t kTagBCIR = makeTag('B', 'C', 'I', 'R');
constexpr uint32_t kTagBREC = makeTag('B', 'R', 'E', 'C');
constexpr uint32_t kTagBPOL = makeTag('B', 'P', 'O', 'L');
constexpr uint32_t kTagBSPL = makeTag('B', 'S', 'P', 'L'); // dead
constexpr uint32_t kTagBPLN = makeTag('B', 'P', 'L', 'N');

// -- filters --
constexpr uint32_t kTagFHGT = makeTag('F', 'H', 'G', 'T');
constexpr uint32_t kTagFFRA = makeTag('F', 'F', 'R', 'A');
constexpr uint32_t kTagFBIT = makeTag('F', 'B', 'I', 'T');
constexpr uint32_t kTagFSLP = makeTag('F', 'S', 'L', 'P');
constexpr uint32_t kTagFDIR = makeTag('F', 'D', 'I', 'R');
constexpr uint32_t kTagFSHD = makeTag('F', 'S', 'H', 'D');

// -- affectors: dead (parse-and-discard, no live object) --
constexpr uint32_t kTagAHSM = makeTag('A', 'H', 'S', 'M');
constexpr uint32_t kTagAHBM = makeTag('A', 'H', 'B', 'M');
constexpr uint32_t kTagACBM = makeTag('A', 'C', 'B', 'M');
constexpr uint32_t kTagASBM = makeTag('A', 'S', 'B', 'M');
constexpr uint32_t kTagAFBM = makeTag('A', 'F', 'B', 'M');

// -- affectors: live --
constexpr uint32_t kTagAENV = makeTag('A', 'E', 'N', 'V');
constexpr uint32_t kTagAHTR = makeTag('A', 'H', 'T', 'R');
constexpr uint32_t kTagAHCN = makeTag('A', 'H', 'C', 'N');
constexpr uint32_t kTagAHFR = makeTag('A', 'H', 'F', 'R');
constexpr uint32_t kTagACCN = makeTag('A', 'C', 'C', 'N');
constexpr uint32_t kTagACRH = makeTag('A', 'C', 'R', 'H');
constexpr uint32_t kTagACRF = makeTag('A', 'C', 'R', 'F');
constexpr uint32_t kTagASCN = makeTag('A', 'S', 'C', 'N');
constexpr uint32_t kTagASRP = makeTag('A', 'S', 'R', 'P');
constexpr uint32_t kTagAFCN = makeTag('A', 'F', 'C', 'N'); // alias of AFSC
constexpr uint32_t kTagAFSC = makeTag('A', 'F', 'S', 'C');
constexpr uint32_t kTagAFSN = makeTag('A', 'F', 'S', 'N');
constexpr uint32_t kTagARCN = makeTag('A', 'R', 'C', 'N'); // alias of AFDN
constexpr uint32_t kTagAFDN = makeTag('A', 'F', 'D', 'N');
constexpr uint32_t kTagAFDF = makeTag('A', 'F', 'D', 'F');
constexpr uint32_t kTagARIB = makeTag('A', 'R', 'I', 'B');
constexpr uint32_t kTagAEXC = makeTag('A', 'E', 'X', 'C');
constexpr uint32_t kTagAPAS = makeTag('A', 'P', 'A', 'S');
constexpr uint32_t kTagAROA = makeTag('A', 'R', 'O', 'A');
constexpr uint32_t kTagARIV = makeTag('A', 'R', 'I', 'V');

// -- HeightData (shared by AffectorRoad/AffectorRiver) --
constexpr uint32_t kTagROAD = makeTag('R', 'O', 'A', 'D');
constexpr uint32_t kTagHDTA = makeTag('H', 'D', 'T', 'A');
constexpr uint32_t kTagSGMT = makeTag('S', 'G', 'M', 'T');

// -- family groups --
constexpr uint32_t kTagSGRP = makeTag('S', 'G', 'R', 'P');
constexpr uint32_t kTagFGRP = makeTag('F', 'G', 'R', 'P');
constexpr uint32_t kTagRGRP = makeTag('R', 'G', 'R', 'P');
constexpr uint32_t kTagEGRP = makeTag('E', 'G', 'R', 'P');
constexpr uint32_t kTagMGRP = makeTag('M', 'G', 'R', 'P');
constexpr uint32_t kTagSFAM = makeTag('S', 'F', 'A', 'M');
constexpr uint32_t kTagFFAM = makeTag('F', 'F', 'A', 'M');
constexpr uint32_t kTagRFAM = makeTag('R', 'F', 'A', 'M');
constexpr uint32_t kTagEFAM = makeTag('E', 'F', 'A', 'M');
constexpr uint32_t kTagMFAM = makeTag('M', 'F', 'A', 'M');
constexpr uint32_t kTagMFRC = makeTag('M', 'F', 'R', 'C');

} // namespace terrain
