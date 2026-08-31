#pragma once

#include <cstdint>
#include <string>

class GuestMemory;

struct WiiMemoryLayout {
    std::uint32_t mem1_usable_start;
    std::uint32_t mem1_usable_end;
    std::uint32_t mem2_usable_start;
    std::uint32_t mem2_usable_end;
    std::uint32_t ipc_buffer_start;
    std::uint32_t ipc_buffer_end;
};

struct ValidationResult {
    bool valid;
    std::string message;
};

WiiMemoryLayout make_wpse01_layout(std::uint32_t arena_lo, std::uint32_t arena_hi);
ValidationResult validate(const WiiMemoryLayout& layout);
bool install_wii_memory_layout(GuestMemory& memory, const WiiMemoryLayout& layout, std::string& error);
