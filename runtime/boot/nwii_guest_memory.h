#pragma once

#include "runtime/boot/guest_memory.h"

#include <cstdint>
#include <string>

namespace nwii::runtime {
struct CPUContext;
}

class NWiiGuestMemory final : public GuestMemory {
public:
    explicit NWiiGuestMemory(nwii::runtime::CPUContext& context);

    void write32_be(std::uint32_t address, std::uint32_t value) override;

private:
    nwii::runtime::CPUContext& context_;
};

bool install_wpse01_boot_memory(nwii::runtime::CPUContext& context,
                                std::uint32_t arena_lo, std::uint32_t arena_hi,
                                std::string& error);
