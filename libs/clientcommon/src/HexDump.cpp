#include "clientcommon/HexDump.h"

#include <iostream>

namespace clientcommon {

void printHexBytes(const std::vector<uint8_t>& bytes) {
    for (uint8_t b : bytes) {
        std::cout << std::hex << (b < 16 ? "0" : "") << static_cast<int>(b) << std::dec << " ";
    }
}

} // namespace clientcommon
