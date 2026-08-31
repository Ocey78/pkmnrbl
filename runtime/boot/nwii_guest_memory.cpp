#include "runtime/boot/nwii_guest_memory.h"

#include "runtime/boot/wii_memory_layout.h"
#include "runtime/cpu_context.h"

NWiiGuestMemory::NWiiGuestMemory(nwii::runtime::CPUContext& context) : context_(context) {}

void NWiiGuestMemory::write32_be(std::uint32_t address, std::uint32_t value) {
    context_.mmu.write32(address, value);
}

bool install_wpse01_boot_memory(nwii::runtime::CPUContext& context,
                                std::uint32_t arena_lo, std::uint32_t arena_hi,
                                std::string& error) {
    NWiiGuestMemory memory(context);
    return install_wii_memory_layout(memory, make_wpse01_layout(arena_lo, arena_hi), error);
}
