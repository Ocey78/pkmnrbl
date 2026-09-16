#include "runtime/hw/hw.h"

#include <cstdint>

namespace nwii::runtime {
extern uint64_t get_os_time();
}

namespace nwii::runtime::hw {

namespace {

constexpr uint32_t AI_CONTROL_PSTAT = 0x01;
constexpr uint32_t AI_CONTROL_AIINTMSK = 0x04;
constexpr uint32_t AI_CONTROL_AIINT = 0x08;
constexpr uint32_t AI_CONTROL_SCRESET = 0x20;
constexpr uint32_t AI_CONTROL_RATE = 0x40;

uint32_t ai_cr = 0;
uint32_t ai_vr = 0;
uint32_t ai_scnt = 0;
uint32_t ai_it = 0;
uint64_t ai_start_time = 0;

uint32_t ai_rate_hz() {
    return (ai_cr & AI_CONTROL_RATE) ? 32000u : 48000u;
}

uint32_t ai_current_sample_counter(uint64_t now) {
    if (!(ai_cr & AI_CONTROL_PSTAT))
        return ai_scnt;

    const uint64_t elapsed_us = now - ai_start_time;
    const uint64_t samples = (elapsed_us * ai_rate_hz()) / 1000000ull;
    return ai_scnt + static_cast<uint32_t>(samples);
}

void ai_update_interrupt_state(uint32_t current_counter) {
    if (ai_it > 0 && current_counter >= ai_it) {
        ai_cr |= AI_CONTROL_AIINT;
        if (ai_cr & AI_CONTROL_AIINTMSK)
            trigger_pi_interrupt(0x20);
    }
}

uint32_t ai_read(uint32_t addr) {
    const uint32_t offset = addr & 0xFFu;
    const uint32_t current_counter =
        ai_current_sample_counter(nwii::runtime::get_os_time());

    ai_update_interrupt_state(current_counter);

    switch (offset) {
    case 0x00:
        return ai_cr;
    case 0x04:
        return ai_vr;
    case 0x08:
        return current_counter;
    case 0x0C:
        return ai_it;
    default:
        return 0;
    }
}

void ai_write(uint32_t addr, uint32_t val) {
    const uint32_t offset = addr & 0xFFu;
    const uint64_t now = nwii::runtime::get_os_time();

    switch (offset) {
    case 0x00: {
        const bool was_playing = (ai_cr & AI_CONTROL_PSTAT) != 0;
        const bool will_play = (val & AI_CONTROL_PSTAT) != 0;
        const uint32_t current_counter = ai_current_sample_counter(now);

        uint32_t interrupt_status = ai_cr & AI_CONTROL_AIINT;
        if (val & AI_CONTROL_AIINT) {
            interrupt_status = 0;
            clear_pi_interrupt(0x20);
        }

        if (val & AI_CONTROL_SCRESET) {
            ai_scnt = 0;
            ai_start_time = now;
        } else if (was_playing && !will_play) {
            ai_scnt = current_counter;
            ai_start_time = now;
        } else if (!was_playing && will_play) {
            ai_start_time = now;
        }

        ai_cr = (val & ~AI_CONTROL_AIINT) | interrupt_status;
        break;
    }

    case 0x04:
        ai_vr = val;
        break;

    case 0x08:
        // AI_AISCNT is read-only from Broadway.
        break;

    case 0x0C:
        ai_it = val;
        break;

    default:
        break;
    }
}

void register_ai_region(MMIODispatcher& dispatcher, uint32_t base) {
    dispatcher.register_region(base, base + 0xFFu, ai_read, ai_write);
}

} // namespace

void register_ai(MMIODispatcher& dispatcher) {
    // GameCube/legacy view retained for compatibility.
    register_ai_region(dispatcher, 0xCC006C00u);

    // Wii Broadway view.
    register_ai_region(dispatcher, 0xCD006C00u);

    // CPUContext::MMU masks cached/uncached high address bits before the
    // hardware dispatch, so 0xCD006Cxx arrives here as 0x0D006Cxx too.
    // Register it before the broad IPC alias so AI is not swallowed by IPC.
    register_ai_region(dispatcher, 0x0D006C00u);
}

void ai_update() {
    const uint32_t current_counter =
        ai_current_sample_counter(nwii::runtime::get_os_time());
    ai_update_interrupt_state(current_counter);
}

} // namespace nwii::runtime::hw
