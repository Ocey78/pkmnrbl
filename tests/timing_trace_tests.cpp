#include "runtime/cpu_context.h"
#include "runtime/event_scheduler.h"
#include "runtime/hw/hw.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace nwii::runtime {
CPUContext* g_ctx_ptr = nullptr;
MMU* g_mmu = nullptr;
void GX_WGPIPE_Write32(uint32_t) { std::abort(); } // No fixture access may target graphics.
void trace_call(uint32_t func_addr, CPUContext& ctx);
}
namespace nwii::runtime::hw {
std::atomic<uint32_t> pi_intsr{0};
std::atomic<uint32_t> pi_intmr{0};
}

using namespace nwii::runtime;
using namespace nwii::runtime::hw;

static void require(bool ok, const char* message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

static size_t occurrences(const std::string& output, const std::string& prefix) {
    size_t count = 0;
    for (size_t at = output.find(prefix); at != std::string::npos;
         at = output.find(prefix, at + prefix.size())) ++count;
    return count;
}

int main(int argc, char** argv) {
    require(argc == 2, "select a diagnostic contract");
    const std::string mode = argv[1];
    CPUContext ctx;
    g_ctx_ptr = &ctx;
    g_mmu = &ctx.mmu;
    ctx.tb_freq = 60000;
    ctx.inst_count = 123456;
    ctx.mmu.write32(0x800000E4, 0x80010000);
    ctx.mmu.write32(0x800000D4, 0x80011000);
    ctx.dispatch_saved_ctx = 0x80015000;
    ctx.in_callback = true;

    std::ostringstream captured;
    auto* previous_output = std::cout.rdbuf(captured.rdbuf());
    trace_call(0x80012340, ctx);
    watch_hit(0x80012000, 7, 32);
    ctx.in_callback = false;
    ctx.dispatch_saved_ctx = 0;

    auto& mmio = MMIODispatcher::get();
    mmio.clear();
    register_ai(mmio);
    mmio.write32(0xCC006C00, 0x01);
    mmio.write32(0xCC006C00, 0x43);

    auto& scheduler = EventScheduler::get();
    scheduler.reset();
    register_dsp(mmio);
    mmio.write16(0xCC00500A, 0x0100);
    mmio.write16(0xCC005030, 0x0001);
    mmio.write16(0xCC005032, 0x4000);
    mmio.write16(0xCC005036, 0x8001);
    for (uint64_t i = 1; i <= 400; ++i) {
        ctx.inst_count = i * 3600 - 1;
        // Original AX metadata only: size mail then pointer mail. No command
        // list payload is present or needed for the runtime mail protocol.
        mmio.write16(0xCC005000, 0xBABE);
        mmio.write16(0xCC005002, 0x0011);
        mmio.write16(0xCC005000, 0x8001);
        mmio.write16(0xCC005002, 0x2000);
        ctx.inst_count = i * 3600;
        scheduler.advance(ctx, ctx.read_timebase());
    }
    std::cout.rdbuf(previous_output);
    const std::string output = captured.str();

    if (mode == "fields") {
        const std::string metadata = "inst=123456 tb=10288 thread=0x80010000 context=0x80011000 dispatch=0x80015000 in_callback=1";
        require(occurrences(output, metadata) == 2,
                "timing trace must append decimal clock and hex thread/context/dispatch fields to real call and watch lines");
    } else if (mode == "dma") {
        require(occurrences(output, "[DSPDMA]") == 18,
                "400 DMA completions must log only first16 and every200 (18 lines)");
        require(output.find("[DSPDMA] seq=200 blocks=1 bytes=32 addr=0x14000 inst=720000 tb=60000") != std::string::npos,
                "sampled DMA completion must identify size, address and same guest clock");
        require(output.find("[DSPDMA] seq=400 ") != std::string::npos &&
                output.find("[DSPDMA] seq=17 ") == std::string::npos,
                "DMA sampling must include400 and suppress17");
        require(output.find("ax_seq=200 ax_addr=0x80012000 ax_words=17 ax_size_mail=0xbabe0011") != std::string::npos,
                "DMA log must retain the corresponding latest AX mail metadata");
    } else if (mode == "mail") {
        require(occurrences(output, "[AXMail]") == 18,
                "400 AX metadata pairs must have bounded first16/every200 logging");
        require(output.find("[AXMail] seq=200 size_mail=0xbabe0011 addr=0x80012000 words=17 bytes=34") != std::string::npos,
                "AX metadata must show raw size mail and command-list pointer/size without payload");
    } else if (mode == "ai") {
        require(output.find("[AIControl] addr=0xcc006c00 old=0x0 written=0x1 new=0x1 old_rate_bits=0x0 new_rate_bits=0x0") != std::string::npos,
                "AI control trace must preserve raw old, written and applied control values");
        require(output.find("old=0x1 written=0x43 new=0x43 old_rate_bits=0x0 new_rate_bits=0x42") != std::string::npos,
                "AI control trace must expose both raw rate bits without changing rate policy");
    } else if (mode == "disabled") {
        require(output.find("inst=") == std::string::npos &&
                output.find("[DSPDMA]") == std::string::npos &&
                output.find("[AXMail]") == std::string::npos &&
                output.find("[AIControl]") == std::string::npos,
                "timing metadata and hardware logging must remain default-off");
    } else require(false, "unknown diagnostic contract");
    require(mmio.read32(0xCC006C00) == 0x43, "diagnostics must preserve applied AI control");
    require((mmio.read16(0xCC00500A) & 0x0008) != 0,
            "diagnostics must preserve real audio DMA completion state");
    std::cout << "PASS: timing diagnostics " << argv[1] << '\n';
}
