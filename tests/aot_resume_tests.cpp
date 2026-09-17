#include "runtime/cpu_context.h"

extern "C" void run_game(nwii::runtime::CPUContext&);
namespace {
bool yielded = false;
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
        ctx.pc = 0xFFFFFFFC; // dispatcher control sentinel; no further guest instruction
        return true;
    }
    if (ctx.pc == 0x80004014 && !yielded) {
        yielded = true;
        return true; // yield to the real emitted dispatcher at the saved PC
    }
    return false;
}
}
int main() {
    nwii::runtime::CPUContext ctx;
    ctx.pc = 0x80004010;
    ctx.lr = 0x80005000;
    ctx.msr = 0;
    ctx.gpr[4] = 0x8000;
    ctx.gpr[6] = 0;
    run_game(ctx);
    if (interpreted) {
        std::cerr << "FAIL: covered interrupt continuation reached interpreter\n";
        return 1;
    }
    if (!finished || !yielded || ctx.gpr[6] != 1 || ctx.msr != 0x8000) {
        std::cerr << "FAIL: continuation must run once with interrupts enabled\n";
        return 1;
    }
    std::cout << "PASS: interrupt continuation uses its existing AOT function\n";
    return 0;
}
