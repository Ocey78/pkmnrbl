#include "runtime/cpu_context.h"
#include <stdexcept>
#include <string>

void execute_float_update_fixture(nwii::runtime::CPUContext&, unsigned);
namespace nwii::runtime {
uint32_t g_watch_addr = 0;
void watch_hit(uint32_t, uint32_t, int) { throw std::runtime_error("unexpected watch"); }
void GX_WGPIPE_Write32(uint32_t) { throw std::runtime_error("unexpected FIFO"); }
void GX_WGPIPE_WriteF32(float) { throw std::runtime_error("unexpected FIFO"); }
uint32_t HW_Reg_Read32(uint32_t) { throw std::runtime_error("unexpected MMIO"); }
void HW_Reg_Write32(uint32_t, uint32_t) { throw std::runtime_error("unexpected MMIO"); }
bool g_trace_calls = false;
void trace_call(uint32_t, CPUContext&) {}
bool process_pending_callbacks(CPUContext&) { return false; }
void interpret_step(CPUContext&) { throw std::runtime_error("float update must not interpret"); }
void micro_interpret(CPUContext&, uint32_t, uint32_t) { throw std::runtime_error("float update reached unsupported interpreter path"); }
}
static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    const bool store = argc > 1 && std::string(argv[1]) == "store";
    nwii::runtime::CPUContext ctx;
    const uint32_t bases[] = {0x80008FF8, 0x80009008, 0x80009000, 0x80001001, 0x80011000, 0xFFFFFFF0};
    try {
        for (unsigned scenario = 0; scenario < 6; ++scenario) {
            // Avoid the MMU's preexisting special word at physical0x0C.
            const uint32_t addr = scenario == 5 ? 0x20 : 0x80009000;
            const unsigned ra = scenario == 2 ? 5 : 6;
            for (uint32_t bits : {0x40600000u, 0x80000000u, 0xC0200000u}) {
                for (unsigned r = 0; r < 32; ++r) {
                    ctx.gpr[r] = 0x12340000 + r;
                    ctx.fpr[r] = 100.0 + r;
                    ctx.ps1[r] = 200.0 + r;
                }
                ctx.gpr[ra] = bases[scenario];
                ctx.lr = 0x80006000;
                ctx.fpscr = 0x12345678;
                for (auto& cr : ctx.cr) { cr.lt = true; cr.gt = false; cr.eq = true; cr.so = false; }
                ctx.mmu.write32(addr - 4, 0xA5A5A5A5);
                ctx.mmu.write32(addr, store ? 0xDEADBEEF : bits);
                ctx.mmu.write32(addr + 4, 0xA5A5A5A5);
                const double value = static_cast<double>(std::bit_cast<float>(bits));
                ctx.fpr[5] = store ? value : 99.0;
                ctx.ps1[5] = 98.0;
                execute_float_update_fixture(ctx, (store ? 6 : 0) + scenario);
                require(ctx.pc == ctx.lr, "float update did not return natively");
                require(ctx.gpr[ra] == addr, "signed offset/wrap/update incorrect");
                for (unsigned r = 0; r < 32; ++r) {
                    if (r != ra) require(ctx.gpr[r] == 0x12340000 + r, "unrelated GPR changed");
                    if (r != 5) require(ctx.fpr[r] == 100.0 + r && ctx.ps1[r] == 200.0 + r, "unrelated FPR changed");
                }
                require(std::bit_cast<uint64_t>(ctx.fpr[5]) == std::bit_cast<uint64_t>(value), "float first lane/sign incorrect");
                require(std::bit_cast<uint64_t>(ctx.ps1[5]) == std::bit_cast<uint64_t>(store ? 98.0 : value),
                        "lfsu must broadcast single; stfsu must preserve PS1");
                require(ctx.mmu.read32(addr) == bits, "float memory value/sign incorrect");
                require(ctx.mmu.read32(addr - 4) == 0xA5A5A5A5 && ctx.mmu.read32(addr + 4) == 0xA5A5A5A5,
                        "float update touched neighboring memory");
                require(ctx.fpscr == 0x12345678, "FPSCR changed");
                for (const auto& cr : ctx.cr) require(cr.lt && !cr.gt && cr.eq && !cr.so, "CR changed");
            }
        }
        std::cout << "PASS: single-precision update " << (store ? "store" : "load") << '\n';
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
