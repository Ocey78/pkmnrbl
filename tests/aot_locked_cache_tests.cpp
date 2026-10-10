#include "runtime/cpu_context.h"

#include <array>
#include <stdexcept>
#include <string>

void execute_locked_cache_fixture(nwii::runtime::CPUContext&, unsigned);

// Link boundaries unrelated to cache/RAM behavior. Any accidental fallback or
// device access fails; all tested instructions execute real emitted native code.
namespace nwii::runtime {
uint32_t g_watch_addr = 0;
void watch_hit(uint32_t, uint32_t, int) { throw std::runtime_error("unexpected watch"); }
void GX_WGPIPE_Write8(uint8_t) { throw std::runtime_error("unexpected FIFO"); }
void GX_WGPIPE_Write16(uint16_t) { throw std::runtime_error("unexpected FIFO"); }
void GX_WGPIPE_Write32(uint32_t) { throw std::runtime_error("unexpected FIFO"); }
void GX_WGPIPE_WriteF32(float) { throw std::runtime_error("unexpected FIFO"); }
void GX_WGPIPE_WriteF64(double) { throw std::runtime_error("unexpected FIFO"); }
uint16_t HW_Reg_Read16(uint32_t) { throw std::runtime_error("unexpected MMIO"); }
uint32_t HW_Reg_Read32(uint32_t) { throw std::runtime_error("unexpected MMIO"); }
void HW_Reg_Write16(uint32_t, uint16_t) { throw std::runtime_error("unexpected MMIO"); }
void HW_Reg_Write32(uint32_t, uint32_t) { throw std::runtime_error("unexpected MMIO"); }
bool g_trace_calls = false;
void trace_call(uint32_t, CPUContext&) {}
bool process_pending_callbacks(CPUContext&) { return false; }
void interpret_step(CPUContext&) { throw std::runtime_error("locked cache must not interpret"); }
void micro_interpret(CPUContext&, uint32_t, uint32_t) {
    throw std::runtime_error("locked cache reached unsupported interpreter path");
}
}

namespace {
using nwii::runtime::CPUContext;
constexpr uint32_t kLce = 0x10000000;
constexpr uint32_t kLc = 0xE0000800;
constexpr uint32_t kRam = 0x80010000;
constexpr uint32_t kGuard = 0xA59C37E1;
enum Snippet : unsigned {
    WriteHid2, WriteDmau, WriteDmal, ReadHid2, ReadDmau, ReadDmal,
    LockedZero, LockedZeroRa0, OrdinaryZero, OrdinaryZeroRa0,
    Invalidate, InvalidateRa0, FlushCacheRa0, StoreCacheRa0
};

void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

void execute(CPUContext& ctx, Snippet id) {
    ctx.lr = 0x80006000;
    execute_locked_cache_fixture(ctx, id);
    require(ctx.pc == ctx.lr, "instruction did not return natively");
}

void write_spr(CPUContext& ctx, Snippet id, uint32_t value) {
    ctx.gpr[3] = value;
    execute(ctx, id);
    require(ctx.gpr[3] == value, "mtspr modified its source GPR");
}

uint32_t read_spr(CPUContext& ctx, Snippet id) {
    ctx.gpr[4] = 0xDEADBEEF;
    execute(ctx, id);
    return ctx.gpr[4];
}

void enable(CPUContext& ctx) { write_spr(ctx, WriteHid2, kLce); }

void zero(CPUContext& ctx, uint32_t address, bool locked, bool ra_zero = false) {
    ctx.gpr[0] = 0x12345678; // RA0 must not read this register.
    ctx.gpr[5] = ra_zero ? 0x87654321 : address - 7;
    ctx.gpr[6] = ra_zero ? address : 7;
    const uint32_t saved5 = ctx.gpr[5], saved6 = ctx.gpr[6];
    execute(ctx, locked ? (ra_zero ? LockedZeroRa0 : LockedZero)
                        : (ra_zero ? OrdinaryZeroRa0 : OrdinaryZero));
    require(ctx.gpr[0] == 0x12345678 && ctx.gpr[5] == saved5 && ctx.gpr[6] == saved6,
            "block zero modified address registers");
}

void allocate(CPUContext& ctx, uint32_t base, unsigned bytes) {
    for (unsigned offset = 0; offset < bytes; offset += 32)
        zero(ctx, base + offset, true);
}

uint32_t pattern(unsigned offset, uint32_t seed) {
    return seed ^ (0x01030709u * (offset / 4 + 1));
}

void fill(CPUContext& ctx, uint32_t base, unsigned bytes, uint32_t seed) {
    for (unsigned offset = 0; offset < bytes; offset += 4)
        ctx.mmu.write32(base + offset, pattern(offset, seed));
}

void expect_pattern(CPUContext& ctx, uint32_t base, unsigned bytes,
                    uint32_t seed, const std::string& label) {
    for (unsigned offset = 0; offset < bytes; offset += 4)
        require(ctx.mmu.read32(base + offset) == pattern(offset, seed),
                label + " at byte " + std::to_string(offset));
}

// Literal split-length fields, independently hand-derived from the DMA command
// format. The zero length field denotes 128 blocks, not an empty transfer.
struct Length { unsigned blocks; uint32_t upper; uint32_t lower; };
constexpr std::array<Length, 5> kLengths = {{{1, 0, 4}, {3, 0, 12},
                                           {4, 1, 0}, {127, 31, 12}, {128, 0, 0}}};

void command(CPUContext& ctx, uint32_t ram, uint32_t lc, Length length,
             uint32_t flags) {
    write_spr(ctx, WriteDmau, (ram & 0x3FFFFFE0u) | length.upper);
    write_spr(ctx, WriteDmal, (lc & 0xFFFFFFE0u) | length.lower | flags);
}

void expect_completed(CPUContext& ctx) {
    require((read_spr(ctx, ReadDmal) & 3u) == 0, "synchronous completion retained T/F");
    const uint32_t hid2 = read_spr(ctx, ReadHid2);
    require((hid2 & kLce) != 0, "DMA completion lost HID2 LCE");
    require((hid2 & 0x0F000000u) == 0, "synchronous DMA left a queued operation");
}

void registers() {
    CPUContext ctx;
    enable(ctx);
    require((read_spr(ctx, ReadHid2) & kLce) != 0, "HID2 LCE did not read back");
    write_spr(ctx, WriteDmau, 0x10012343);
    write_spr(ctx, WriteDmal, 0xE000108C); // Legal LC tag, length, no trigger.
    require(read_spr(ctx, ReadDmau) == 0x10012343, "DMAU address/length readback lost bits");
    require(read_spr(ctx, ReadDmal) == 0xE000108C, "DMAL tag/length readback lost bits");
    write_spr(ctx, WriteHid2, 0);
    require((read_spr(ctx, ReadHid2) & kLce) == 0, "HID2 LCE could not be disabled");
    write_spr(ctx, WriteHid2, kLce | 0x0F00FFFF);
    require(read_spr(ctx, ReadHid2) == kLce, "software fabricated queue count or reserved HID2 bits");
}

void allocation() {
    CPUContext ctx;
    enable(ctx);
    allocate(ctx, kLc - 32, 96);
    allocate(ctx, kLc + 0x2000, 32);
    fill(ctx, kLc - 32, 96, 0x76543210);
    ctx.mmu.write32(kLc + 0x2000, 0x10203040);
    ctx.mmu.write32(0x80000800, 0x89ABCDEF);
    ctx.mmu.write8(kLc, 0xC1);
    ctx.mmu.write8(kLc + 1, 0xD2);
    ctx.mmu.write16(kLc + 2, 0xE3F4);
    require(ctx.mmu.read32(kLc) == 0xC1D2E3F4, "allocated LC CPU byte/halfword writes did not round-trip");
    ctx.mmu.write64(kLc + 8, 0x0123456789ABCDEFull);
    require(ctx.mmu.read64(kLc + 8) == 0x0123456789ABCDEFull,
            "allocated LC CPU 64-bit access did not round-trip");
    ctx.mmu.write_f32(kLc + 16, 3.5f);
    require(ctx.mmu.read32(kLc + 16) == 0x40600000 && ctx.mmu.read_f32(kLc + 16) == 3.5f,
            "allocated LC float access did not share CPU backing");
    ctx.mmu.write_f64(kLc + 24, -2.5);
    require(ctx.mmu.read64(kLc + 24) == 0xC004000000000000ull && ctx.mmu.read_f64(kLc + 24) == -2.5,
            "allocated LC double access did not share CPU backing");
    require(ctx.mmu.read32(kLc + 0x2000) == 0x10203040, "LC banks alias each other");
    require(ctx.mmu.read32(0x80000800) == 0x89ABCDEF, "LC storage aliases MEM1");
    zero(ctx, kLc + 31, true);
    for (unsigned offset = 0; offset < 32; offset += 4)
        require(ctx.mmu.read32(kLc + offset) == 0, "dcbz_l did not zero its aligned 32-byte line");
    require(ctx.mmu.read32(kLc - 4) == pattern(28, 0x76543210) &&
            ctx.mmu.read32(kLc + 32) == pattern(64, 0x76543210),
            "dcbz_l crossed a line boundary");
    fill(ctx, kLc, 32, 0x31415926);
    zero(ctx, kLc + 9, true, true);
    for (unsigned offset = 0; offset < 32; offset += 4)
        require(ctx.mmu.read32(kLc + offset) == 0, "dcbz_l RA0 did not use zero base");
}

void transfers(bool load) {
    CPUContext ctx;
    enable(ctx);
    allocate(ctx, kLc - 32, 128 * 32 + 64);
    for (uint32_t ram : {0x80010000u, 0x90010000u}) {
        for (const Length length : kLengths) {
            const unsigned bytes = length.blocks * 32;
            const std::string label = std::string(load ? "load" : "store") +
                (ram == 0x80010000 ? " MEM1 " : " MEM2 ") + std::to_string(length.blocks);
            fill(ctx, kLc, bytes, 0x87654321);
            fill(ctx, ram, bytes, 0x12345678);
            for (uint32_t address : {kLc - 4, kLc + bytes, ram - 4, ram + bytes})
                ctx.mmu.write32(address, kGuard);
            command(ctx, ram, kLc, length, load ? 0x12 : 2);
            expect_pattern(ctx, load ? kLc : ram, bytes,
                           load ? 0x12345678 : 0x87654321, label + " payload mismatch");
            expect_pattern(ctx, load ? ram : kLc, bytes,
                           load ? 0x12345678 : 0x87654321, label + " source changed");
            for (uint32_t address : {kLc - 4, kLc + bytes, ram - 4, ram + bytes})
                require(ctx.mmu.read32(address) == kGuard, label + " crossed transfer boundary");
            expect_completed(ctx);
        }
    }
}

void no_trigger() {
    CPUContext ctx;
    enable(ctx);
    allocate(ctx, kLc, 32);
    for (uint32_t flags : {0u, 0x10u}) {
        fill(ctx, kLc, 32, 0x87654321);
        fill(ctx, kRam, 32, 0x12345678);
        command(ctx, kRam, kLc, kLengths[0], flags);
        expect_pattern(ctx, kRam, 32, 0x12345678, "DMA without trigger changed RAM");
        expect_pattern(ctx, kLc, 32, 0x87654321, "DMA without trigger changed LC");
        require(read_spr(ctx, ReadDmal) == (kLc | 4u | flags),
                "non-triggered command did not retain DMAL address/length/direction");
    }
}

void completion() {
    CPUContext ctx;
    enable(ctx);
    allocate(ctx, kLc, 32);
    fill(ctx, kRam, 32, 0x12345678);
    command(ctx, kRam, kLc, kLengths[0], 0x12);
    expect_completed(ctx);
    require(read_spr(ctx, ReadDmau) == 0x00010000, "completion changed DMAU");
    require(read_spr(ctx, ReadDmal) == 0xE0000814, "completion lost DMAL tag/length/direction");
    expect_pattern(ctx, kLc, 32, 0x12345678, "completed DMA was not visible to CPU");
}

void flush() {
    CPUContext ctx;
    enable(ctx);
    allocate(ctx, kLc, 32);
    for (uint32_t flags : {3u, 0x13u}) {
        fill(ctx, kLc, 32, 0x87654321);
        fill(ctx, kRam, 32, 0x12345678);
        command(ctx, kRam, kLc, kLengths[0], flags);
        expect_pattern(ctx, kRam, 32, 0x12345678, "flush+trigger performed a DMA store");
        expect_pattern(ctx, kLc, 32, 0x87654321, "flush+trigger performed a DMA load");
        expect_completed(ctx);
    }
}

void disabled() {
    CPUContext ctx;
    enable(ctx);
    allocate(ctx, kLc, 32);
    for (uint32_t flags : {2u, 0x12u}) {
        fill(ctx, kLc, 32, 0x87654321);
        fill(ctx, kRam, 32, 0x12345678);
        write_spr(ctx, WriteHid2, 0);
        command(ctx, kRam, kLc, kLengths[0], flags);
        require((read_spr(ctx, ReadHid2) & kLce) == 0, "DMA enabled a disabled LC");
        require((read_spr(ctx, ReadDmal) & 3) == 0, "disabled DMA retained trigger/flush bits");
        expect_pattern(ctx, kRam, 32, 0x12345678, "disabled LCE allowed a DMA store");
        enable(ctx);
        expect_pattern(ctx, kRam, 32, 0x12345678, "re-enabling LCE replayed an ignored DMA store");
        expect_pattern(ctx, kLc, 32, 0x87654321, "disabled LCE allowed a DMA load");
    }
}

void boundary() {
    CPUContext ctx;
    enable(ctx);
    allocate(ctx, 0xE0000000, 64);
    allocate(ctx, 0xE0003FC0, 64);
    for (uint32_t lc : {0xE0000000u, 0xE0003FE0u}) {
        const uint32_t neighbor = lc == 0xE0000000 ? lc + 32 : lc - 4;
        for (uint32_t ram : {0x817FFFE0u, 0x93FFFFE0u}) {
            fill(ctx, lc, 32, 0x87654321);
            fill(ctx, ram, 32, 0x12345678);
            ctx.mmu.write32(neighbor, kGuard);
            ctx.mmu.write32(ram - 4, kGuard);
            command(ctx, ram, lc, kLengths[0], 2);
            expect_pattern(ctx, ram, 32, 0x87654321, "end-of-RAM DMA store mismatch");
            fill(ctx, ram, 32, 0x19283746);
            command(ctx, ram, lc, kLengths[0], 0x12);
            expect_pattern(ctx, lc, 32, 0x19283746, "edge-of-LC DMA load mismatch");
            require(ctx.mmu.read32(neighbor) == kGuard && ctx.mmu.read32(ram - 4) == kGuard,
                    "boundary DMA touched an adjacent valid word");
            expect_completed(ctx);
        }
    }
}

void dcbz() {
    CPUContext ctx;
    for (uint32_t ram : {0x80010000u, 0x90010000u}) {
        for (bool ra_zero : {false, true}) {
            fill(ctx, ram, 32, 0x87654321);
            ctx.mmu.write32(ram - 4, kGuard);
            ctx.mmu.write32(ram + 32, kGuard);
            zero(ctx, ram + 31, false, ra_zero);
            for (unsigned offset = 0; offset < 32; offset += 4)
                require(ctx.mmu.read32(ram + offset) == 0, "ordinary dcbz did not zero the aligned RAM line");
            require(ctx.mmu.read32(ram - 4) == kGuard && ctx.mmu.read32(ram + 32) == kGuard,
                    "ordinary dcbz crossed a line boundary");
        }
    }
}

template<class F> void expect_diagnostic(F action, const char* message) {
    bool rejected = false;
    try { action(); } catch (const std::runtime_error&) { rejected = true; }
    require(rejected, message);
}

void guards() {
    CPUContext ctx;
    expect_diagnostic([&] { zero(ctx, kLc, true); }, "disabled dcbz_l silently succeeded");
    enable(ctx);
    allocate(ctx, kLc, 96);
    allocate(ctx, 0xE0003FE0, 32);
    fill(ctx, kLc, 96, 0x87654321);
    fill(ctx, 0x817FFFE0, 32, 0x12345678);
    fill(ctx, 0x93FFFFE0, 32, 0x12345678);
    // Three lines cannot fit in either final physical RAM line. Reject the
    // command before copying the first line, not after a partial host write.
    for (uint32_t ram : {0x817FFFE0u, 0x93FFFFE0u}) {
        for (uint32_t flags : {2u, 0x12u}) {
            expect_diagnostic([&] { command(ctx, ram, kLc, kLengths[1], flags); },
                              "DMA crossed a RAM allocation boundary");
            expect_pattern(ctx, ram, 32, 0x12345678, "invalid DMA partially wrote RAM");
            expect_pattern(ctx, kLc, 96, 0x87654321, "invalid DMA partially wrote LC");
        }
    }
    fill(ctx, kRam, 96, 0x12345678);
    for (uint32_t lc : {0xE0003FE0u, 0xE0004000u, 0xE0010800u, 0x00000800u}) {
        expect_diagnostic([&] { command(ctx, kRam, lc, kLengths[1], 2); },
                          "DMA wrapped or aliased an unsupported LC span");
        expect_pattern(ctx, kRam, 96, 0x12345678, "bad LC address overwrote RAM");
    }
    for (uint32_t physical : {0x80010000u, 0x90010000u, 0x0D006C00u}) {
        // Write raw DMAU: CPU cached aliases are NOT physical bus addresses.
        write_spr(ctx, WriteDmau, physical);
        expect_diagnostic([&] { write_spr(ctx, WriteDmal, kLc | 6); },
                          "DMA accepted a CPU alias or MMIO as physical RAM");
    }
    require(ctx.mmu.get_ptr(0xE0003FFF, 2) == nullptr &&
            ctx.mmu.get_ptr(0xE0010800) == nullptr,
            "LC CPU span crossed or wrapped the supported window");
    zero(ctx, kLc, false); // Ordinary dcbz also zeroes an existing locked line.
    require(ctx.mmu.read32(kLc) == 0 && ctx.mmu.read32(kLc + 32) == pattern(32, 0x87654321),
            "ordinary dcbz did not honor a locked-cache hit");
}

void missing_tag() {
    CPUContext ctx;
    enable(ctx);
    allocate(ctx, kLc, 32);
    allocate(ctx, kLc + 64, 32); // The middle tag is deliberately absent.
    fill(ctx, kRam, 96, 0x12345678);
    command(ctx, kRam, kLc, kLengths[1], 0x12);
    require(ctx.mmu.read32(kLc) == pattern(0, 0x12345678) &&
            ctx.mmu.read32(kLc + 64) == pattern(64, 0x12345678),
            "DMA miss prevented a later valid line from loading");
    require(ctx.mmu.get_ptr(kLc + 32) == nullptr, "DMA allocated a missing tag");
    require((read_spr(ctx, ReadHid2) & 0x00200000) != 0, "DMA cache miss did not set DCMERR");
    fill(ctx, kRam, 96, 0x87654321);
    command(ctx, kRam, kLc, kLengths[1], 2);
    require(ctx.mmu.read32(kRam) == pattern(0, 0x12345678) &&
            ctx.mmu.read32(kRam + 32) == pattern(32, 0x87654321) &&
            ctx.mmu.read32(kRam + 64) == pattern(64, 0x12345678),
            "DMA miss did not skip exactly the absent line");
    require((read_spr(ctx, ReadHid2) & 0x0F000000) == 0, "DMA miss fabricated queue state");
    zero(ctx, kLc, true);
    require((read_spr(ctx, ReadHid2) & 0x00800000) != 0, "dcbz_l hit did not set DCHERR");
    write_spr(ctx, WriteHid2, kLce | 0x00020000);
    ctx.msr = 0x9000; // EE and ME enabled: unsupported exception must be visible.
    expect_diagnostic([&] { command(ctx, kRam, kLc + 32, kLengths[0], 2); },
                      "enabled DMA miss silently ignored required machine check");
}

void maintenance() {
    CPUContext ctx;
    enable(ctx);
    for (Snippet snippet : {Invalidate, InvalidateRa0}) {
        zero(ctx, kLc, true);
        fill(ctx, kLc, 32, 0x87654321);
        fill(ctx, kRam, 32, 0x12345678);
        ctx.gpr[0] = 0x12345678;
        ctx.gpr[5] = snippet == Invalidate ? kLc - 5 : 0x11111111;
        ctx.gpr[6] = snippet == Invalidate ? 5 : kLc;
        execute(ctx, snippet);
        command(ctx, kRam, kLc, kLengths[0], 2);
        expect_pattern(ctx, kRam, 32, 0x12345678, "DMA exported a dcbi-invalidated line");
        require(ctx.mmu.get_ptr(kLc) == nullptr && (read_spr(ctx, ReadHid2) & 0x00200000),
                "dcbi did not invalidate the allocated tag");
    }
    zero(ctx, kLc, true);
    for (Snippet snippet : {StoreCacheRa0, FlushCacheRa0}) {
        ctx.gpr[6] = kLc;
        expect_diagnostic([&] { execute(ctx, snippet); }, "LC castout silently ignored unsupported bus destination");
        ctx.gpr[6] = kRam;
        execute(ctx, snippet); // Direct-mapped coherent RAM needs no writeback.
    }
}
}

int main(int argc, char** argv) {
    struct Test { const char* name; void (*run)(); };
    const Test tests[] = {
        {"registers", registers}, {"allocation", allocation},
        {"store", +[] { transfers(false); }}, {"load", +[] { transfers(true); }},
        {"no_trigger", no_trigger}, {"completion", completion}, {"flush", flush},
        {"disabled", disabled}, {"boundary", boundary}, {"dcbz", dcbz},
        {"guards", guards}, {"missing_tag", missing_tag}, {"maintenance", maintenance}
    };
    const std::string mode = argc > 1 ? argv[1] : "all";
    unsigned ran = 0, failed = 0;
    for (const Test& test : tests) {
        if (mode != "all" && mode != test.name) continue;
        ++ran;
        try {
            test.run();
            std::cout << "PASS: locked cache " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "FAIL: locked cache " << test.name << ": " << error.what() << '\n';
        }
    }
    if (ran == 0 || argc > 2) {
        std::cerr << "usage: aot_locked_cache_tests [all|registers|allocation|store|load|no_trigger|completion|flush|disabled|boundary|dcbz|guards|missing_tag|maintenance]\n";
        return 2;
    }
    return failed == 0 ? 0 : 1;
}
