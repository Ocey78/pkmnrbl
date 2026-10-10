#include "runtime/cpu_context.h"
#include <stdexcept>

extern "C" void run_game(nwii::runtime::CPUContext&);
namespace {
bool ios_called = false;
bool returned_to_lr = false;
unsigned visits = 0;
}
extern "C" void IOS_Ioctlv(nwii::runtime::CPUContext&) {
    ios_called = true;
}
extern "C" void IOS_IoctlvAsync(nwii::runtime::CPUContext&) {
    ios_called = true;
}
namespace nwii::runtime {
bool g_trace_calls = false;
uint32_t g_watch_addr = 0;
void trace_call(uint32_t, CPUContext&) {}
void watch_hit(uint32_t, uint32_t, int) {}
uint32_t HW_Reg_Read32(uint32_t) { throw std::runtime_error("unexpected MMIO read"); }
void HW_Reg_Write32(uint32_t, uint32_t) { throw std::runtime_error("unexpected MMIO write"); }
void GX_WGPIPE_Write32(uint32_t) { throw std::runtime_error("unexpected GPU write"); }
void interpret_step(CPUContext&) { throw std::runtime_error("unexpected interpreter fallback"); }
bool process_pending_callbacks(CPUContext& ctx) {
    if (ctx.pc == 0x80005000 || ++visits > 10) {
        returned_to_lr = ctx.pc == 0x80005000;
        ctx.is_running = false;
        ctx.pc = 0xFFFFFFFC;
        return true;
    }
    return false;
}
}
int main() {
    nwii::runtime::CPUContext ctx;
    try {
        for (uint32_t entry : {0x80004000u, 0x80004100u}) {
            ios_called = false;
            returned_to_lr = false;
            visits = 0;
            ctx.is_running = true;
            ctx.pc = entry;
            ctx.lr = 0x80005000;
            ctx.gpr[1] = 0x80008000;
            ctx.gpr[3] = 11;
            ctx.gpr[4] = 22;
            ctx.gpr[5] = 33;
            ctx.gpr[6] = 0;
            ctx.gpr[7] = 0;
            ctx.gpr[30] = 0xABCDEF01;
            ctx.gpr[31] = 0x23456789;
            run_game(ctx);
            const bool body_executed = entry == 0x80004000
                ? ctx.gpr[6] == 37 && ctx.gpr[27] == 11 && ctx.gpr[28] == 22 && ctx.gpr[29] == 33
                : ctx.gpr[6] == 73 && ctx.gpr[8] == 22 && ctx.gpr[9] == 33 &&
                  ctx.gpr[30] == 0xABCDEF01 && ctx.gpr[31] == 0x23456789;
            if (!returned_to_lr || ctx.lr != 0x80005000 || ctx.gpr[1] != 0x80008000 ||
                ctx.gpr[7] != 1 || !body_executed || ios_called) {
                std::cerr << "FAIL: unrelated body at 0x" << std::hex << entry
                          << " must execute natively, not as an IOS ioctlv hook\n";
                return 1;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    std::cout << "PASS: both unrelated ABI prologues execute guest bodies and return to LR\n";
    return 0;
}
