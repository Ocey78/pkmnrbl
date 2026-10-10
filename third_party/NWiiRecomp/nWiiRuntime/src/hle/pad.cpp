#include "runtime/cpu_context.h"
#include "input/input_manager.h"

using namespace nwii::runtime;
using namespace nwii::runtime::input;

namespace {
constexpr uint32_t kKpadSampleSize = 0xB0;
uint32_t previous_buttons[4] = {};

bool writable_ram(const CPUContext& ctx, uint32_t address, uint32_t size) {
    if (!address || uint64_t(address) + size > 0x100000000ull) return false;
    const uint32_t physical = address & 0x3FFFFFFFu;
    if (physical < ctx.mmu.mem1.size() - 8)
        return uint64_t(physical) + size <= ctx.mmu.mem1.size() - 8;
    if (physical >= 0x10000000u)
        return uint64_t(physical - 0x10000000u) + size <= ctx.mmu.mem2.size() - 8;
    return false;
}
}

extern "C" {

void PADInit(CPUContext& ctx) {}
void WPADInit(CPUContext& ctx) {}
void KPADInit(CPUContext& ctx) {
    for (auto& buttons : previous_buttons) buttons = 0;
}

// Opt-in basic native-controller seam. This does not simulate Bluetooth or
// alter the guest's internal WPAD connection/extension state.
void WPADProbe(CPUContext& ctx) {
    const int32_t channel = static_cast<int32_t>(ctx.gpr[3]);
    const uint32_t type_ptr = ctx.gpr[4];
    if (channel < 0 || channel >= 4 ||
        (type_ptr && !writable_ram(ctx, type_ptr, 4))) {
        ctx.gpr[3] = uint32_t(-1);
        return;
    }
    const auto state = InputManager::get().get_wiimote_state(channel);
    // Callers commonly skip KPADRead after a failed probe. End that channel's
    // edge sequence here too, so reconnecting a held button reports a press.
    if (state.err != 0) previous_buttons[channel] = 0;
    if (type_ptr) ctx.mmu.write32(type_ptr, state.err == 0 ? 0 : 0xFF);
    ctx.gpr[3] = static_cast<uint32_t>(static_cast<int32_t>(state.err));
}

void PADRead(CPUContext& ctx) {
    uint32_t pad_status_array_addr = ctx.gpr[3];
    GameCubePadState state = InputManager::get().get_gcpad_state(0);

    ctx.mmu.write16(pad_status_array_addr, state.buttons);
    ctx.mmu.write8(pad_status_array_addr + 2, (uint8_t)state.stick_x);
    ctx.mmu.write8(pad_status_array_addr + 3, (uint8_t)state.stick_y);
    ctx.mmu.write8(pad_status_array_addr + 4, (uint8_t)state.substick_x);
    ctx.mmu.write8(pad_status_array_addr + 5, (uint8_t)state.substick_y);
    ctx.mmu.write8(pad_status_array_addr + 6, state.trigger_l);
    ctx.mmu.write8(pad_status_array_addr + 7, state.trigger_r);
    ctx.mmu.write8(pad_status_array_addr + 8, state.buttons & 0x0100 ? 0xFF : 0);
    ctx.mmu.write8(pad_status_array_addr + 9, state.buttons & 0x0200 ? 0xFF : 0);
    ctx.mmu.write8(pad_status_array_addr + 10, state.err);
    ctx.mmu.write8(pad_status_array_addr + 11, 0);

    for (int i = 1; i < 4; i++) {
        uint32_t offset = pad_status_array_addr + (i * 12);
        ctx.mmu.write16(offset, 0);
        ctx.mmu.write8(offset + 10, (uint8_t)-1); 
    }
}

void WPADRead(CPUContext& ctx) {
    int32_t chan = (int32_t)ctx.gpr[3];
    uint32_t data_ptr = ctx.gpr[4];

    if (chan >= 0 && chan < 4 && data_ptr != 0) {
        WiimoteState state = InputManager::get().get_wiimote_state(chan);
        ctx.mmu.write32(data_ptr + 0x00, state.err);
        ctx.mmu.write32(data_ptr + 0x04, state.buttons);
    }
    ctx.gpr[3] = 0; 
}

void KPADRead(CPUContext& ctx) {
    const int32_t channel = static_cast<int32_t>(ctx.gpr[3]);
    const uint32_t dest = ctx.gpr[4];
    const int32_t capacity = static_cast<int32_t>(ctx.gpr[5]);
    ctx.gpr[3] = 0;
    if (channel < 0 || channel >= 4 || capacity <= 0 ||
        !writable_ram(ctx, dest, kKpadSampleSize)) return;

    const auto state = InputManager::get().get_wiimote_state(channel);
    if (state.err != 0) {
        previous_buttons[channel] = 0;
        return;
    }
    const uint32_t hold = state.buttons;
    const uint32_t trigger = hold & ~previous_buttons[channel];
    const uint32_t release = previous_buttons[channel] & ~hold;

    // Verified WPSE ABI: 176 bytes per sample. Base remote, successful status,
    // no pointer tracking, core-buttons format (0), no extension. Unsupported
    // sensor fields are deterministic zero, not uninitialized guest memory.
    for (uint32_t i = 0; i < kKpadSampleSize; ++i) ctx.mmu.write8(dest + i, 0);
    ctx.mmu.write32(dest, hold);
    ctx.mmu.write32(dest + 4, trigger);
    ctx.mmu.write32(dest + 8, release);
    previous_buttons[channel] = hold;
    ctx.gpr[3] = 1;
}

}
