#pragma once

#include <cstdint>

namespace nwii::runtime::gx {

// Match the runtime MMU aliases, retaining the MEM2 bank bit. Validate the
// entire guest span, not merely its first byte or the MMU's host-side padding.
inline bool ResolveDisplayListRamSpan(uint32_t address, uint32_t size,
                                      uint32_t& physical) {
    physical = address & 0x3FFFFFFFu;
    if (size == 0) return false;
    if (physical < 0x01800000u)
        return size <= 0x01800000u - physical;
    if (physical >= 0x10000000u && physical < 0x14000000u)
        return size <= 0x14000000u - physical;
    return false;
}

} // namespace nwii::runtime::gx
