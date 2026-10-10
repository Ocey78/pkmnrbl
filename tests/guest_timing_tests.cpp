#include "runtime/cpu_context.h"
#include "runtime/event_scheduler.h"
#include "runtime/guest_timing.h"
#include "runtime/hw/hw.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>

namespace nwii::runtime {
// The VI beam-position clock is unrelated to retrace IRQ timing.
uint64_t get_os_time() { return 0; }
uint16_t HW_Reg_Read16(uint32_t addr) { return MMIODispatcher::get().read16(addr); }
uint32_t HW_Reg_Read32(uint32_t addr) { return MMIODispatcher::get().read32(addr); }
}
namespace nwii::runtime::hw {
std::atomic<uint32_t> pi_intsr{0};
std::atomic<uint32_t> pi_intmr{0};
}

using namespace nwii::runtime;
using namespace nwii::runtime::hw;

static void require(bool ok, const std::string& message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

struct Fixture {
    CPUContext ctx;
    EventScheduler& scheduler = EventScheduler::get();
    MMIODispatcher& mmio = MMIODispatcher::get();
    explicit Fixture(uint64_t frequency = 40500000) {
        scheduler.reset();
        mmio.clear();
        pi_intsr = 0;
        pi_intmr = 0x100;
        register_vi(mmio);
        for (uint32_t addr : {0xCC002030u, 0xCC002034u, 0xCC002038u, 0xCC00203Cu})
            mmio.write32(addr, 0);
        ctx.pc = 0x80004000;
        ctx.msr = 0x8000;
        ctx.tb_freq = frequency;
        initialize_guest_timers(ctx);
    }
    void poll() {
        consume_host_vblank_hint(ctx);
        if (!ctx.in_callback) service_guest_timers(ctx);
    }
    bool acknowledge_vi() {
        if (!(pi_intsr & 0x100)) return false;
        require((mmio.read32(0xCC002030) & 0x8000) != 0,
                "VI interrupt must have latched the real VI MMIO status");
        mmio.write32(0xCC002030, 0);
        mmio.write32(0xCC002034, 0);
        require(!(pi_intsr & 0x100), "acknowledging both VI fields must clear PI VI");
        return true;
    }
};

static void presentations(int pulses, uint64_t frequency = 40500000) {
    Fixture f(frequency);
    int count = 0;
    for (int i = 1; i <= pulses; ++i) {
        f.ctx.inst_count = frequency * i / pulses;
        f.ctx.vblank_pending = true;
        f.poll();
        count += f.acknowledge_vi();
    }
    require(count == 60, std::to_string(pulses) + " presentations over one guest second: expected 60 acknowledged VIs, got " + std::to_string(count));
}

static void idle(uint64_t frequency = 40500000, uint64_t first_vi = 675000) {
    Fixture f(frequency);
    f.ctx.pc = 0;
    f.poll();
    require(f.ctx.read_timebase() == first_vi, "PC0 idle without host hint must advance guest TB to first VI (" + std::to_string(first_vi) + ")");
    require(f.scheduler.now() == first_vi, "scheduler and guest TB must agree during idle");
    require(f.acknowledge_vi(), "PC0 idle must deliver VI without a host hint");
    f.poll();
    require(f.ctx.read_timebase() == 2 * first_vi, "idle must continue to next VI after acknowledgement");
    require(f.acknowledge_vi(), "second idle deadline must deliver another VI");
}

static void deadlines() {
    Fixture f;
    f.ctx.pc = 0;
    f.ctx.write_dec(100);
    bool dsp_fired = false;
    f.scheduler.schedule_after(200, [&](CPUContext&, uint64_t) {
        dsp_fired = true;
        trigger_pi_interrupt(0x40);
    });
    f.poll();
    require(f.ctx.read_timebase() == 100, "earlier armed DEC deadline must win before DSP and VI");
    require(f.ctx.read_dec() == 0 && f.ctx.dec_expired(), "idle advance must expire the real DEC against the same TB");
    require(f.scheduler.now() == 100 && !dsp_fired && !(pi_intsr & 0x100), "DEC must not prematurely service later events");
    f.poll();
    require(f.ctx.read_timebase() == 100, "deliverable expired DEC must prevent fast-forward");
    f.ctx.dec_irq_pending = false;
    f.poll();
    require(f.ctx.read_timebase() == 200 && dsp_fired, "earlier DSP scheduler event must win before VI");
    require(!(pi_intsr & 0x100), "DSP deadline must not raise VI");
}

static void busy() {
    Fixture f;
    f.poll();
    require(f.ctx.inst_count == 0, "active execution must not fast-forward");
    f.ctx.pc = 0;
    f.ctx.in_callback = true;
    service_guest_timers(f.ctx);
    require(f.ctx.inst_count == 0, "callback in flight must not fast-forward");
    f.ctx.in_callback = false;
    f.ctx.dispatch_saved_ctx = 0x80006000;
    f.poll();
    require(f.ctx.inst_count == 0, "interrupt handler in flight must not fast-forward");
    f.ctx.dispatch_saved_ctx = 0;
    pi_intsr = 0x100;
    f.poll();
    require(f.ctx.inst_count == 0, "pending deliverable PI IRQ must not fast-forward");
    pi_intsr = 0;
    f.ctx.queue_callback(0x80008000, 0, 0);
    f.poll();
    require(f.ctx.inst_count == 0, "queued deliverable callback must not fast-forward");
}

static void masked_idle() {
    Fixture f;
    f.ctx.pc = 0;
    f.ctx.msr = 0;
    f.ctx.queue_callback(0x80008000, 0, 0);
    f.ctx.write_dec(100);
    f.poll();
    require(f.ctx.read_timebase() == 100, "EE-masked queued callback must not prevent idle DEC advance");
    require(f.ctx.dec_expired(), "masked DEC must become pending at its own deadline");
    f.poll();
    require(f.ctx.read_timebase() == 675000, "expired masked DEC must not block advancement to VI");
    require((pi_intsr & 0x100) != 0, "EE-masked idle must still latch VI");
    f.poll();
    require(f.ctx.read_timebase() == 1350000, "masked pending PI IRQ must not block next idle deadline");
    require(f.acknowledge_vi(), "masked idle retraces must coalesce into one VI");
}

static void scheduler_deadlines() {
    Fixture f;
    f.ctx.pc = 0;
    bool cancelled_fired = false;
    bool earlier_fired = false;
    const auto cancelled = f.scheduler.schedule_after(25, [&](CPUContext&, uint64_t) { cancelled_fired = true; });
    f.scheduler.schedule_after(50, [&](CPUContext&, uint64_t) { earlier_fired = true; });
    require(f.scheduler.cancel(cancelled), "earliest scheduled event must be cancellable");
    f.poll();
    require(f.ctx.read_timebase() == 50 && earlier_fired && !cancelled_fired,
            "idle must choose earliest surviving event after cancellation");
    f.scheduler.reset();
    f.poll();
    require(f.ctx.read_timebase() == 50, "empty scheduler and unarmed DEC must leave TB unchanged");
}

static void coalescing() {
    Fixture f;
    pi_intmr = 0;
    f.ctx.inst_count = 675000;
    f.poll();
    require(!(pi_intsr & 0x100), "PI-masked VI must preserve existing suppression");
    pi_intmr = 0x100;
    f.poll();
    require(!(pi_intsr & 0x100), "unmasking at same tick must not replay suppressed VI");
    f.ctx.inst_count = 1350000;
    f.ctx.msr = 0;
    f.poll();
    require((pi_intsr & 0x100) != 0, "EE-masked VI must latch for later delivery");
    f.ctx.inst_count = 2025000;
    f.poll();
    require(f.acknowledge_vi(), "pending VIs must coalesce into one acknowledgement");
    f.poll();
    require(!f.acknowledge_vi(), "repeated same-tick servicing must not generate extra VI");
    f.ctx.msr = 0x8000;
    f.ctx.inst_count = 40500000;
    f.poll();
    require(f.acknowledge_vi(), "late polling must deliver one VI");
    f.poll();
    require(!f.acknowledge_vi(), "late polling must not burst-replay missed retraces");
    f.ctx.inst_count += 675000;
    f.poll();
    require(f.acknowledge_vi(), "late polling must schedule the next VI one period later");
}

static void wall() {
    Fixture f;
    f.ctx.pc = 0;
    f.ctx.tb_start = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    f.ctx.inst_count = 17;
    f.ctx.write_dec(405000000);
    f.poll();
    require(f.ctx.inst_count == 17, "NWII_WALLTB idle must not change instruction clock");
    require(f.ctx.read_timebase() >= 40500000 && f.scheduler.now() >= 40500000,
            "NWII_WALLTB timers must use elapsed wall time");
    require(!f.ctx.dec_expired(), "new wall-time DEC must not expire by virtual idle advance");
    require(f.acknowledge_vi(), "wall-time late poll must still deliver scheduled VI");
}

// Original synthetic guest RAM; addresses and PC are deliberately unrelated
// to title data. The runtime reads these through its real CPUContext MMU.
struct SDKIdleFixture : Fixture {
    static constexpr uint32_t first = 0x80010000;
    static constexpr uint32_t second = 0x90010400;
    static constexpr uint32_t context = 0x80010800;
    SDKIdleFixture() : Fixture(60750000) {
        ctx.pc = 0x80012340;
        write32(0x800000D4, context);
        write32(0x800000DC, first);
        write32(0x800000E0, second);
        write32(0x800000E4, 0);
        write16(first + 0x2C8, 4);
        write32(first + 0x2FC, second);
        write32(first + 0x300, 0);
        write16(second + 0x2C8, 4);
        write32(second + 0x2FC, 0);
        write32(second + 0x300, first);
    }
    void write16(uint32_t addr, uint16_t value) {
        auto* bytes = ctx.mmu.get_ptr(addr);
        require(bytes != nullptr, "synthetic SDK fixture write16 must target RAM");
        bytes[0] = static_cast<uint8_t>(value >> 8);
        bytes[1] = static_cast<uint8_t>(value);
    }
    void write32(uint32_t addr, uint32_t value) {
        auto* bytes = ctx.mmu.get_ptr(addr);
        require(bytes != nullptr, "synthetic SDK fixture write32 must target RAM");
        bytes[0] = static_cast<uint8_t>(value >> 24);
        bytes[1] = static_cast<uint8_t>(value >> 16);
        bytes[2] = static_cast<uint8_t>(value >> 8);
        bytes[3] = static_cast<uint8_t>(value);
    }
};

static void sdk_idle() {
    SDKIdleFixture f;
    f.poll();
    require(f.ctx.read_timebase() == 1012500,
            "validated nonzero-PC all-waiting SDK idle must advance to Wii VI");
    require(f.scheduler.now() == 1012500 && f.acknowledge_vi(),
            "SDK idle must advance real guest TB and scheduler, then latch real VI");
    require(f.ctx.pc == 0x80012340, "semantic SDK idle must preserve the guest PC");
}

static void sdk_deadlines() {
    SDKIdleFixture f;
    f.ctx.write_dec(100);
    bool event_fired = false;
    f.scheduler.schedule_after(200, [&](CPUContext&, uint64_t) { event_fired = true; });
    f.poll();
    require(f.ctx.read_timebase() == 100 && f.ctx.dec_expired() && !event_fired,
            "validated SDK idle must select earlier DEC before event and VI");
    f.poll();
    require(f.ctx.read_timebase() == 100, "deliverable SDK idle DEC must prevent further advance");
    f.ctx.dec_irq_pending = false;
    f.poll();
    require(f.ctx.read_timebase() == 200 && event_fired && !(pi_intsr & 0x100),
            "validated SDK idle must select earlier scheduled event before VI");
}

static void sdk_invalid() {
    // Each fixture starts from a valid complete two-thread waiting list.
    for (int scenario = 0; scenario < 28; ++scenario) {
        SDKIdleFixture f;
        switch (scenario) {
        case 0: f.write32(0x800000E4, SDKIdleFixture::first); break; // current thread
        case 1: f.write32(0x800000D4, 0); break; // uninitialized context
        case 2: f.write32(0x800000D4, 0x80010801); break; // unaligned context
        case 3: f.write32(0x800000D4, 0x817FFF00); break; // truncated context
        case 4: f.write32(0x800000D4, 0xCC002000); break; // MMIO context
        case 5: f.write32(0x800000DC, 0); f.write32(0x800000E0, 0); break; // startup
        case 6: f.write32(0x800000DC, 0); break; // missing head
        case 7: f.write32(0x800000E0, 0); break; // missing tail
        case 8: f.write32(0x800000E0, SDKIdleFixture::first); break; // early tail
        case 9: f.write32(0x800000DC, 0x80010001); break; // unaligned head
        case 10: f.write32(0x800000DC, 0x817FFF00); break; // truncated thread
        case 11: f.write32(SDKIdleFixture::first + 0x300, SDKIdleFixture::second); break; // head prev
        case 12: f.write32(SDKIdleFixture::second + 0x300, 0); break; // broken backlink
        case 13: f.write32(SDKIdleFixture::first + 0x2FC, 0); break; // incomplete list
        case 14: f.write32(SDKIdleFixture::second + 0x2FC, SDKIdleFixture::first); break; // cycle
        case 15: f.write32(SDKIdleFixture::first + 0x2FC, SDKIdleFixture::first); break; // self-cycle
        case 16: f.write32(SDKIdleFixture::first + 0x2FC, 0x94000000); break; // invalid next
        case 17: f.write16(SDKIdleFixture::second + 0x2C8, 1); break; // ready
        case 18: f.write16(SDKIdleFixture::second + 0x2C8, 2); break; // running
        case 19: f.write16(SDKIdleFixture::second + 0x2C8, 0); break; // uninitialized state
        case 20: f.write16(SDKIdleFixture::second + 0x2C8, 8); break; // unsupported state
        case 21: f.ctx.msr = 0; break; // SDK startup with EE off
        case 22: f.write32(0x800000D4, 0x93FFFF00); break; // truncated MEM2 context
        case 23: f.write32(SDKIdleFixture::first + 0x2FC, 0x93FFFF00); break; // truncated MEM2 thread
        case 24: f.write16(SDKIdleFixture::second + 0x2C8, 0xFFFF); break; // unknown state
        case 25: f.write32(0x800000D4, SDKIdleFixture::first); break; // live thread context
        case 26: f.write32(0x800000E0, 0x90010401); break; // unaligned tail
        case 27: { // Excessive traversal must remain bounded even with valid links.
            constexpr uint32_t head = 0x80020000;
            constexpr uint32_t count = 1100;
            f.write32(0x800000DC, head);
            f.write32(0x800000E0, head + (count - 1) * 0x400);
            for (uint32_t i = 0; i < count; ++i) {
                const uint32_t thread = head + i * 0x400;
                f.write16(thread + 0x2C8, 4);
                f.write32(thread + 0x300, i == 0 ? 0 : thread - 0x400);
                f.write32(thread + 0x2FC, i + 1 == count ? 0 : thread + 0x400);
            }
            break;
        }
        }
        f.poll();
        require(f.ctx.inst_count == 0 && f.scheduler.now() == 0 && !(pi_intsr & 0x100),
                "invalid/active SDK state must refuse fast-forward: scenario " + std::to_string(scenario));
    }
}

static void sdk_guards() {
    for (int scenario = 0; scenario < 6; ++scenario) {
        SDKIdleFixture f;
        switch (scenario) {
        case 0: f.ctx.in_callback = true; break;
        case 1: f.ctx.callback_depth = 1; break;
        case 2: f.ctx.backup_stack.push(CPUContext::BackupState{}); break;
        case 3: f.ctx.dispatch_saved_ctx = SDKIdleFixture::context; break;
        case 4: pi_intsr = 0x100; break;
        case 5: f.ctx.queue_callback(0x80014000, 0, 0); break;
        }
        service_guest_timers(f.ctx);
        require(f.ctx.inst_count == 0 && f.scheduler.now() == 0,
                "in-flight/deliverable SDK work must refuse fast-forward: scenario " + std::to_string(scenario));
    }
}

static void reset() {
    Fixture f;
    bool stale_fired = false;
    f.scheduler.schedule_after(10, [&](CPUContext&, uint64_t) { stale_fired = true; });
    f.ctx.inst_count = 100;
    initialize_guest_timers(f.ctx);
    require(f.scheduler.now() == 100, "timer reinitialization must reset and synchronize scheduler to guest TB");
    f.poll();
    require(!stale_fired, "timer reinitialization must discard events from the previous runtime");
    f.ctx.inst_count = 675000;
    f.poll();
    require(!f.acknowledge_vi(), "timer reinitialization must remove the old VI deadline");
    f.ctx.inst_count = 675100;
    f.poll();
    require(f.acknowledge_vi(), "reinitialized runtime must register VI relative to its current TB");
    f.poll();
    require(!f.acknowledge_vi(), "reinitialized runtime must have only one VI source");
}

int main(int argc, char** argv) {
    require(argc == 2, "select one timing case");
    const std::string mode = argv[1];
    if (mode == "presentations_120") presentations(120);
    else if (mode == "presentations_240") presentations(240);
    else if (mode == "presentations_120_wii") presentations(120, 60750000);
    else if (mode == "presentations_240_wii") presentations(240, 60750000);
    else if (mode == "idle") idle();
    else if (mode == "idle_wii") idle(60750000, 1012500);
    else if (mode == "deadlines") deadlines();
    else if (mode == "busy") busy();
    else if (mode == "coalescing") coalescing();
    else if (mode == "wall") wall();
    else if (mode == "reset") reset();
    else if (mode == "masked_idle") masked_idle();
    else if (mode == "scheduler_deadlines") scheduler_deadlines();
    else if (mode == "sdk_idle") sdk_idle();
    else if (mode == "sdk_deadlines") sdk_deadlines();
    else if (mode == "sdk_invalid") sdk_invalid();
    else if (mode == "sdk_guards") sdk_guards();
    else require(false, "unknown timing case");
    std::cout << "PASS: guest timing " << mode << '\n';
}
