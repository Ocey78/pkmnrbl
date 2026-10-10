#include "runtime/cpu_context.h"
#include <cstdlib>

extern "C" void run_game(nwii::runtime::CPUContext&);
namespace {
bool interpreted = false;
bool finished = false;
unsigned visits = 0;
}
namespace nwii::runtime {
bool g_trace_calls = false;
void trace_call(uint32_t, CPUContext&) {}
// This original fixture uses only ordinary RAM and a non-null callback.
// Fail immediately if it unexpectedly crosses a device or null-call boundary.
uint32_t g_watch_addr = 0;
void watch_hit(uint32_t, uint32_t, int) { std::abort(); }
uint32_t HW_Reg_Read32(uint32_t) { std::abort(); }
void HW_Reg_Write32(uint32_t, uint32_t) { std::abort(); }
void GX_WGPIPE_Write32(uint32_t) { std::abort(); }
void note_null_call(uint32_t, uint32_t) { std::abort(); }
void interpret_step(CPUContext& ctx) {
    interpreted = true;
    ctx.is_running = false;
}
bool process_pending_callbacks(CPUContext& ctx) {
    if (ctx.pc == 0x80005000 || ++visits > 10) {
        finished = ctx.pc == 0x80005000;
        ctx.is_running = false;
        ctx.pc = 0xFFFFFFFC;
        return true;
    }
    return false;
}
}
int main() {
    nwii::runtime::CPUContext ctx;
    ctx.pc = 0x80004000;
    ctx.ctr = 0x8012B740;
    ctx.lr = 0x80005000;
    ctx.gpr[6] = 0;
    run_game(ctx);
    if (interpreted) {
        std::cerr << "FAIL: title's indirect branch root reached interpreter\n";
        return 1;
    }
    if (!finished || ctx.gpr[6] != 1 || ctx.lr != 0x80005000) {
        std::cerr << "FAIL: AOT branch stub must run target once and preserve LR\n";
        return 1;
    }
    interpreted = false;
    finished = false;
    visits = 0;
    nwii::runtime::CPUContext callback_ctx;
    callback_ctx.pc = 0x80004100;
    callback_ctx.lr = 0x80005000;
    callback_ctx.gpr[3] = 0x80008000;
    callback_ctx.gpr[7] = callback_ctx.gpr[8] = callback_ctx.gpr[9] = 0;
    callback_ctx.mmu.write32(0x80008038, 0x8028F440);
    run_game(callback_ctx);
    if (interpreted) {
        std::cerr << "FAIL: title's configured indirect callback reached interpreter\n";
        return 1;
    }
    if (!finished || callback_ctx.gpr[7] != 1 || callback_ctx.gpr[8] != 1 ||
        callback_ctx.gpr[9] != 0x80004110 || callback_ctx.lr != 0x80005000) {
        std::cerr << "FAIL: AOT callback must run once, resume at exact LR, and return to caller\n";
        return 1;
    }
    std::cout << "PASS: title-configured indirect branch and callback execute as AOT\n";
}
