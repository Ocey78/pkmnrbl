#include "runtime/guest_timing.h"
#include "runtime/cpu_context.h"
#include "runtime/event_scheduler.h"
#include "runtime/hw/hw.h"
#include <cstdlib>
#include <limits>

namespace nwii::runtime {
namespace {
bool valid_sdk_structure(const CPUContext& ctx, uint32_t addr, uint32_t size) {
    if ((addr & 3) != 0) return false;
    const uint64_t end = uint64_t{addr} + size;
    // SDK pointers use cached guest RAM. Check the entire object, not just
    // its first byte; MMU's trailing eight bytes are access padding.
    if (addr >= 0x80000000 && end <= 0x81800000 && ctx.mmu.mem1.size() >= 8)
        return end - 0x80000000 <= ctx.mmu.mem1.size() - 8;
    if (addr >= 0x90000000 && end <= 0x94000000 && ctx.mmu.mem2.size() >= 8)
        return end - 0x90000000 <= ctx.mmu.mem2.size() - 8;
    return false;
}

bool sdk_scheduler_idle(CPUContext& ctx) {
    if (!(ctx.msr & 0x8000) || ctx.mmu.read32(0x800000E4) != 0) return false;
    const uint32_t context = ctx.mmu.read32(0x800000D4);
    if (!valid_sdk_structure(ctx, context, 0x2C8)) return false;
    uint32_t thread = ctx.mmu.read32(0x800000DC);
    const uint32_t tail = ctx.mmu.read32(0x800000E0);
    if (!valid_sdk_structure(ctx, thread, 0x318) ||
        !valid_sdk_structure(ctx, tail, 0x318)) return false;

    // OS active-thread queue: state+2C8, next+2FC, previous+300. A complete,
    // nonempty all-waiting list distinguishes SDK idle from early startup.
    uint32_t previous = 0;
    for (unsigned count = 0; count < 1024; ++count) {
        if (!valid_sdk_structure(ctx, thread, 0x318) || thread == context ||
            ctx.mmu.read16(thread + 0x2C8) != 4 ||
            ctx.mmu.read32(thread + 0x300) != previous) return false;
        const uint32_t next = ctx.mmu.read32(thread + 0x2FC);
        if (thread == tail) return next == 0;
        if (next == 0) return false;
        previous = thread;
        thread = next;
    }
    return false;
}
}

bool consume_host_vblank_hint(CPUContext& ctx) {
    if (ctx.vblank_pending && !ctx.in_callback && (ctx.msr & 0x8000)) {
        return ctx.vblank_pending.exchange(false);
    }
    return false;
}

void initialize_guest_timers(CPUContext& ctx) {
    auto& scheduler = EventScheduler::get();
    scheduler.reset();
    scheduler.advance(ctx, ctx.read_timebase());
    uint64_t period = ctx.tb_freq / 60;
    if (period == 0) period = 500000;
    scheduler.schedule_recurring(period, [](CPUContext& c, uint64_t) {
        if ((hw::pi_intmr & 0x100) && !c.in_callback && !(hw::pi_intsr & 0x100) &&
            (c.dispatch_saved_ctx == 0 || c.mmu.read32(0x800000E4) == 0)) {
            hw::vi_trigger_interrupt();
        }
    });
}

void service_guest_timers(CPUContext& ctx) {
    auto& scheduler = EventScheduler::get();
    uint64_t now = ctx.read_timebase();
    // Match CPUContext's presence-based NWII_WALLTB policy. Only the virtual
    // clock may skip idle time; advancing inst_count keeps TB and DEC aligned.
    static const bool wall = std::getenv("NWII_WALLTB") != nullptr;
    const bool deliverable_irq = (ctx.msr & 0x8000) &&
        ((hw::pi_intsr & hw::pi_intmr) != 0 || ctx.dec_expired());
    const bool deliverable_callback = (ctx.msr & 0x8000) &&
        ctx.pending_cb_count.load(std::memory_order_relaxed) != 0;
    if (!wall && !ctx.in_callback && ctx.callback_depth == 0 &&
        ctx.backup_stack.empty() && ctx.dispatch_saved_ctx == 0 &&
        !deliverable_callback && !deliverable_irq &&
        (ctx.pc == 0 || sdk_scheduler_idle(ctx))) {
        auto deadline = scheduler.next_deadline();
        if (ctx.dec_irq_pending && ctx.dec_value <= std::numeric_limits<uint64_t>::max() - ctx.dec_written_tb) {
            const uint64_t dec_deadline = ctx.dec_written_tb + ctx.dec_value;
            // An expired masked DEC is already pending, not a future wakeup.
            if (dec_deadline > now && (!deadline || dec_deadline < *deadline))
                deadline = dec_deadline;
        }
        if (deadline && *deadline > now) {
            ctx.inst_count = *deadline;
            now = ctx.read_timebase();
        }
    }
    scheduler.advance(ctx, now);
}
}
