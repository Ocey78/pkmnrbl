#include "runtime/cpu_context.h"
#include <array>
#include <stdexcept>
#include <string>

void execute_psq_fixture(nwii::runtime::CPUContext&, unsigned);
namespace nwii::runtime {
// Fixtures use only mapped RAM; an accidental device address is a test failure.
uint32_t g_watch_addr = 0;
void watch_hit(uint32_t, uint32_t, int) { throw std::runtime_error("unexpected watch"); }
void GX_WGPIPE_Write8(uint8_t) { throw std::runtime_error("unexpected FIFO"); }
void GX_WGPIPE_Write16(uint16_t) { throw std::runtime_error("unexpected FIFO"); }
void GX_WGPIPE_Write32(uint32_t) { throw std::runtime_error("unexpected FIFO"); }
void GX_WGPIPE_WriteF32(float) { throw std::runtime_error("unexpected FIFO"); }
uint16_t HW_Reg_Read16(uint32_t) { throw std::runtime_error("unexpected MMIO"); }
uint32_t HW_Reg_Read32(uint32_t) { throw std::runtime_error("unexpected MMIO"); }
void HW_Reg_Write16(uint32_t, uint16_t) { throw std::runtime_error("unexpected MMIO"); }
void HW_Reg_Write32(uint32_t, uint32_t) { throw std::runtime_error("unexpected MMIO"); }
bool g_trace_calls = false;
void trace_call(uint32_t, CPUContext&) {}
bool process_pending_callbacks(CPUContext&) { return false; }
void interpret_step(CPUContext&) { throw std::runtime_error("indexed AOT must not interpret"); }
}
static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    const unsigned op = argc > 1 ? std::stoul(argv[1]) : 0;
    const bool store = op == 1 || op == 3;
    const bool update = op >= 2;
    nwii::runtime::CPUContext ctx;
    constexpr uint32_t addr = 0x80009000;
    try {
        require(op < 4, "unknown indexed operation");
        for (unsigned w = 0; w < 2; ++w) {
            for (unsigned i = 0; i < 8; ++i) {
                for (unsigned scenario = 0; scenario < 6; ++scenario) {
                    for (unsigned r = 0; r < 32; ++r) {
                        ctx.gpr[r] = 0x12340000 + r;
                        ctx.fpr[r] = 100.0 + r;
                        ctx.ps1[r] = 200.0 + r;
                    }
                    ctx.lr = 0x80006000;
                    ctx.fpscr = 0x12345678;
                    for (auto& cr : ctx.cr) { cr.lt = true; cr.gt = false; cr.eq = true; cr.so = false; }
                    for (auto& gqr : ctx.gqr) gqr = 0;
                    // I=0 exercises float; others use selected signed16, distinct
                    // load scale1 and store scale2. No negative scales/clipping.
                    if (i) ctx.gqr[i] = 0x01070207;
                    ctx.gpr[0] = scenario == 0 ? addr : 0x1000;
                    ctx.gpr[3] = scenario == 0 ? 0 : scenario == 2 ? 0x40004800 :
                                 scenario == 3 ? 0xFFFFFFF0 : 0x80008000;
                    ctx.gpr[4] = scenario == 3 ? 0x80009010 : scenario == 5 && !update ? addr : 0x1000;
                    const std::array<uint32_t, 32> old_gprs = [&] {
                        std::array<uint32_t, 32> result{};
                        for (unsigned r = 0; r < 32; ++r) result[r] = ctx.gpr[r];
                        return result;
                    }();
                    for (int b = -4; b < 16; ++b) ctx.mmu.write8(addr + b, 0xA5);
                    ctx.fpr[5] = 3.5;
                    ctx.ps1[5] = -2.5;
                    if (!store) {
                        ctx.fpr[5] = 99;
                        ctx.ps1[5] = 98;
                        if (i) { ctx.mmu.write16(addr, 7); ctx.mmu.write16(addr + 2, 0xFFFB); }
                        else { ctx.mmu.write_f32(addr, 3.5f); ctx.mmu.write_f32(addr + 4, -2.5f); }
                    }
                    execute_psq_fixture(ctx, ((op * 2 + w) * 8 + i) * 6 + scenario);
                    require(ctx.pc == ctx.lr, "indexed function must return natively");
                    for (unsigned r = 0; r < 32; ++r) {
                        require(ctx.gpr[r] == (update && r == 3 ? addr : old_gprs[r]), "EA/update clobbered GPR");
                        if (r != 5) require(ctx.fpr[r] == 100.0 + r && ctx.ps1[r] == 200.0 + r, "unrelated FPR clobbered");
                    }
                    require(ctx.fpscr == 0x12345678, "FPSCR clobbered");
                    for (const auto& cr : ctx.cr) require(cr.lt && !cr.gt && cr.eq && !cr.so, "CR clobbered");
                    require(ctx.mmu.read32(addr - 4) == 0xA5A5A5A5, "wrote before EA");
                    if (store) {
                        require(ctx.fpr[5] == 3.5 && ctx.ps1[5] == -2.5, "store changed source lanes");
                        if (i) {
                            require(ctx.mmu.read16(addr) == 14, "store GQR/first lane incorrect");
                            require(ctx.mmu.read16(addr + 2) == (w ? 0xA5A5 : 0xFFF6), "store W/second lane incorrect");
                        } else {
                            require(ctx.mmu.read_f32(addr) == 3.5f, "float store first lane incorrect");
                            require(w ? ctx.mmu.read32(addr + 4) == 0xA5A5A5A5 : ctx.mmu.read_f32(addr + 4) == -2.5f,
                                    "float store W/second lane incorrect");
                        }
                    } else {
                        require(ctx.fpr[5] == 3.5 && ctx.ps1[5] == (w ? 1.0 : -2.5), "load lanes/GQR/W incorrect");
                        require(i ? ctx.mmu.read32(addr) == 0x0007FFFB : ctx.mmu.read_f32(addr) == 3.5f,
                                "load modified memory");
                    }
                    require(ctx.mmu.read32(addr + (i ? 4 : 8)) == 0xA5A5A5A5, "wrote beyond pair");
                }
            }
        }
        std::cout << "PASS: indexed paired load/store operation " << op << " (96 cases)\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
