#include "runtime/boot/nwii_guest_memory.h"

#include "runtime/boot/wii_memory_layout.h"
#include "runtime/cpu_context.h"
#include "runtime/title_identity.h"

NWiiGuestMemory::NWiiGuestMemory(nwii::runtime::CPUContext& context) : context_(context) {}

void NWiiGuestMemory::write32_be(std::uint32_t address, std::uint32_t value) {
    context_.mmu.write32(address, value);
}

bool install_wpse01_boot_memory(nwii::runtime::CPUContext& context,
                                std::uint32_t arena_lo, std::uint32_t arena_hi,
                                std::string& error) {
    NWiiGuestMemory memory(context);
    if (!install_wii_memory_layout(memory, make_wpse01_layout(arena_lo, arena_hi), error)) {
        return false;
    }
    // WiiWare's boot game code is separate from the optional disc-ID override
    // at 0x80003194. The guest uses its region byte to choose title resources.
    memory.write32_be(0x80003180U, nwii::runtime::pack_game_id(
        nwii::runtime::kPokemonRumbleGameId));
    return true;
}
