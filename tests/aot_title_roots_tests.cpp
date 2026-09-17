#include "runtime/cpu_context.h"

extern "C" void run_game(nwii::runtime::CPUContext&);
namespace {
bool interpreted = false;
bool finished = false;
unsigned visits = 0;
}
namespace nwii::runtime {
bool g_trace_calls = false;
void trace_call(uint32_t, CPUContext&) {}
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
    std::cout << "PASS: title-configured indirect branch stub executes as AOT\n";
}
