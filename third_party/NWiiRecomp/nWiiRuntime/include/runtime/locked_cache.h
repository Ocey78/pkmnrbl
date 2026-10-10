#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace nwii::runtime {

// Native compatibility for the SDK's identity-mapped locked-cache allocation.
// This is NOT a generic tagged L1 cache or a BAT/TLB implementation. See the
// IBM Gekko manual sections 2.1.2.4, 2.1.2.8 and 9.2-9.3. DMA is synchronous:
// copies complete in issue order before the next native instruction executes.
class LockedCache {
public:
    static constexpr uint32_t base = 0xE0000000;
    static constexpr uint32_t size = 0x4000;
    static constexpr uint32_t lce = 0x10000000;

    uint8_t* get_ptr(uint32_t address, size_t bytes = 1) {
        if (!(hid2_ & lce) || address < base || address - base >= size ||
            bytes == 0 || bytes > size - (address - base)) return nullptr;
        const size_t offset = address - base;
        for (size_t line = offset / 32; line <= (offset + bytes - 1) / 32; ++line)
            if (!allocated_[line]) return nullptr;
        return data_.data() + offset;
    }

    uint32_t read_spr(unsigned spr) const {
        switch (spr) {
        case 920: return hid2_; // Queue length is zero after each completed copy.
        case 922: return dmau_;
        case 923: return dmal_;
        default: throw std::runtime_error("unsupported locked-cache SPR read");
        }
    }

    void zero(uint32_t address, uint32_t msr) {
        if (!(hid2_ & lce))
            throw std::runtime_error("dcbz_l with LCE disabled: guest illegal-instruction exception not implemented");
        address &= ~31u;
        if (address < base || address - base >= size)
            throw std::runtime_error("dcbz_l outside supported SDK locked-cache window");
        const size_t line = (address - base) / 32;
        if (allocated_[line]) error(0x00800000, 0x00080000, msr);
        allocated_[line] = true;
        std::memset(data_.data() + line * 32, 0, 32);
    }

    void maintain(uint32_t address, unsigned xo) {
        address &= ~31u;
        if (!get_ptr(address, 32)) return;
        if (xo == 470) allocated_[(address - base) / 32] = false; // dcbi
        else throw std::runtime_error("locked-cache castout to unsupported external bus address");
    }

    // Physical DMA addresses must not be passed through the CPU alias mask.
    static uint8_t* ram_span(uint32_t address, size_t bytes,
                             std::vector<uint8_t>& mem1, std::vector<uint8_t>& mem2) {
        if (mem1.size() >= 8 && address < mem1.size() - 8 &&
            bytes <= mem1.size() - 8 - address) return mem1.data() + address;
        if (mem2.size() >= 8 && address >= 0x10000000 &&
            address - 0x10000000 < mem2.size() - 8 &&
            bytes <= mem2.size() - 8 - (address - 0x10000000))
            return mem2.data() + (address - 0x10000000);
        return nullptr;
    }

    void write_spr(unsigned spr, uint32_t value, uint32_t msr,
                   std::vector<uint8_t>& mem1, std::vector<uint8_t>& mem2) {
        if (spr == 920) {
            // Do not let software fabricate a queue count or reserved bits.
            hid2_ = value & 0xF0FF0000u;
            return;
        }
        if (spr == 922) { dmau_ = value; return; }
        if (spr != 923) throw std::runtime_error("unsupported locked-cache SPR write");
        dmal_ = value & ~3u;
        // Disabled commands are ignored. Flush wins over trigger; no earlier
        // command remains in flight under this synchronous compatibility policy.
        if (!(hid2_ & lce) || (value & 1) || !(value & 2)) return;

        unsigned lines = ((dmau_ & 31) << 2) | ((value >> 2) & 3);
        if (lines == 0) lines = 128;
        const size_t bytes = lines * 32;
        const uint32_t address = value & ~31u;
        if (address < base || address - base >= size || bytes > size - (address - base))
            throw std::runtime_error("DMA outside supported SDK locked-cache window");
        uint8_t* ram = ram_span(dmau_ & ~31u, bytes, mem1, mem2);
        if (!ram) throw std::runtime_error("locked-cache DMA outside physical MEM1/MEM2 RAM");
        const size_t offset = address - base;
        for (size_t i = 0; i < bytes; i += 32) {
            if (!allocated_[(offset + i) / 32]) {
                error(0x00200000, 0x00020000, msr);
                continue; // Hardware skips a missing line, then tries the next.
            }
            if (value & 0x10) std::memcpy(data_.data() + offset + i, ram + i, 32);
            else std::memcpy(ram + i, data_.data() + offset + i, 32);
        }
    }

private:
    void error(uint32_t flag, uint32_t enable, uint32_t msr) {
        const bool newly_set = !(hid2_ & flag);
        hid2_ |= flag;
        if (newly_set && (hid2_ & enable) && (msr & 0x9000) == 0x9000)
            throw std::runtime_error("locked-cache machine-check exception not implemented");
    }

    std::array<uint8_t, size> data_{};
    std::array<bool, size / 32> allocated_{};
    uint32_t hid2_ = 0;
    uint32_t dmau_ = 0;
    uint32_t dmal_ = 0;
};

} // namespace nwii::runtime
