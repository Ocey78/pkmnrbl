#include "runtime/boot/wii_memory_layout.h"

#include "runtime/boot/guest_memory.h"

#include <array>

namespace {

constexpr std::uint32_t kMem1Start = 0x80000000U;
constexpr std::uint32_t kMem1End = 0x81800000U;
constexpr std::uint32_t kMem2Start = 0x90000000U;
constexpr std::uint32_t kMem2End = 0x94000000U;
constexpr std::uint32_t kAlignment = 0x20U;
constexpr std::uint32_t kMinimumIpcSize = 0x1DE0U;

constexpr std::uint32_t kBootMagic = 0x0D15EA5EU;
constexpr std::uint32_t kApploaderVersion = 0x00000001U;
constexpr std::uint32_t kMem1Size = 0x01800000U;
constexpr std::uint32_t kConsoleType = 0x00000021U;
constexpr std::uint32_t kBusClock = 0x0E7BE2C0U;
constexpr std::uint32_t kCpuClock = 0x2B73A840U;
constexpr std::uint32_t kMem2Size = 0x04000000U;
constexpr std::uint32_t kHollywoodRevision = 0x00000011U;
constexpr std::uint32_t kGddrVendor = 0x00000023U;

struct LowMemoryWrite {
    std::uint32_t address;
    std::uint32_t value;
};

[[nodiscard]] bool is_aligned(std::uint32_t address) {
    return (address & (kAlignment - 1U)) == 0U;
}

[[nodiscard]] bool is_ordered(std::uint32_t start, std::uint32_t end) {
    return start < end;
}

[[nodiscard]] bool is_within(std::uint32_t start, std::uint32_t end,
                             std::uint32_t mapped_start, std::uint32_t mapped_end) {
    return start >= mapped_start && end <= mapped_end;
}

[[nodiscard]] ValidationResult invalid(const char* message) {
    return {false, message};
}

}  // namespace

WiiMemoryLayout make_wpse01_layout(std::uint32_t arena_lo, std::uint32_t arena_hi) {
    return {
        arena_lo,
        arena_hi,
        0x90000800U,
        0x93E00000U,
        0x93E00000U,
        0x94000000U,
    };
}

ValidationResult validate(const WiiMemoryLayout& layout) {
    if (!is_aligned(layout.mem1_usable_start) || !is_aligned(layout.mem1_usable_end)) {
        return invalid("MEM1 bounds must be 32-byte aligned");
    }
    if (!is_ordered(layout.mem1_usable_start, layout.mem1_usable_end)) {
        return invalid("MEM1 range must have a positive size");
    }
    if (!is_within(layout.mem1_usable_start, layout.mem1_usable_end, kMem1Start, kMem1End)) {
        return invalid("MEM1 range must be inside mapped MEM1");
    }

    if (!is_aligned(layout.mem2_usable_start) || !is_aligned(layout.mem2_usable_end)) {
        return invalid("MEM2 bounds must be 32-byte aligned");
    }
    if (!is_ordered(layout.mem2_usable_start, layout.mem2_usable_end)) {
        return invalid("MEM2 range must have a positive size");
    }
    if (!is_within(layout.mem2_usable_start, layout.mem2_usable_end, kMem2Start, kMem2End)) {
        return invalid("MEM2 range must be inside mapped MEM2");
    }

    if (!is_aligned(layout.ipc_buffer_start) || !is_aligned(layout.ipc_buffer_end)) {
        return invalid("IPC bounds must be 32-byte aligned");
    }
    if (!is_ordered(layout.ipc_buffer_start, layout.ipc_buffer_end)) {
        return invalid("IPC range must have a positive size");
    }
    if (!is_within(layout.ipc_buffer_start, layout.ipc_buffer_end, kMem2Start, kMem2End)) {
        return invalid("IPC range must be inside mapped MEM2");
    }
    if (layout.mem2_usable_end > layout.ipc_buffer_start) {
        return invalid("usable MEM2 must not overlap the IPC buffer");
    }
    if (layout.ipc_buffer_end - layout.ipc_buffer_start < kMinimumIpcSize) {
        return invalid("IPC buffer is too small for SDK boot allocations");
    }

    return {true, {}};
}

bool install_wii_memory_layout(GuestMemory& memory, const WiiMemoryLayout& layout, std::string& error) {
    const auto validation = validate(layout);
    if (!validation.valid) {
        error = validation.message;
        return false;
    }

    const std::array writes{
        LowMemoryWrite{0x80000020U, kBootMagic},
        LowMemoryWrite{0x80000024U, kApploaderVersion},
        LowMemoryWrite{0x80000028U, kMem1Size},
        LowMemoryWrite{0x8000002CU, kConsoleType},
        LowMemoryWrite{0x80000030U, layout.mem1_usable_start},
        LowMemoryWrite{0x80000034U, layout.mem1_usable_end},
        LowMemoryWrite{0x800000F0U, kMem1Size},
        LowMemoryWrite{0x800000F8U, kBusClock},
        LowMemoryWrite{0x800000FCU, kCpuClock},
        LowMemoryWrite{0x80003100U, kMem1Size},
        LowMemoryWrite{0x80003104U, kMem1Size},
        LowMemoryWrite{0x8000310CU, layout.mem1_usable_start},
        LowMemoryWrite{0x80003110U, layout.mem1_usable_end},
        LowMemoryWrite{0x80003118U, kMem2Size},
        LowMemoryWrite{0x8000311CU, kMem2Size},
        LowMemoryWrite{0x80003120U, kMem2End},
        LowMemoryWrite{0x80003124U, layout.mem2_usable_start},
        LowMemoryWrite{0x80003128U, layout.mem2_usable_end},
        LowMemoryWrite{0x80003130U, layout.ipc_buffer_start},
        LowMemoryWrite{0x80003134U, layout.ipc_buffer_end},
        LowMemoryWrite{0x80003138U, kHollywoodRevision},
        LowMemoryWrite{0x80003158U, kGddrVendor},
    };

    for (const auto& write : writes) {
        memory.write32_be(write.address, write.value);
    }
    error.clear();
    return true;
}
