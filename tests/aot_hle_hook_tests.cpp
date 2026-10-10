#include "runtime/cpu_context.h"

extern "C" void run_game(nwii::runtime::CPUContext&);
namespace {
unsigned hook_calls = 0;
unsigned visits = 0;
bool interpreted = false;
bool returned_to_lr = false;
}

// Defined only in this separate translation unit: generated code must supply
// its own declaration with matching C linkage, not rely on functions.h.
extern "C" void NativeControllerProbeTest(nwii::runtime::CPUContext& ctx) {
    ++hook_calls;
    ctx.gpr[3] = 0x12345678;
    ctx.pc = 0xDEADBEEF; // The wrapper must return to LR after the hook.
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
    ctx.pc = 0x80004000;
    ctx.lr = 0x80005000;
    ctx.gpr[6] = 41;
    run_game(ctx);
    if (hook_calls != 1 || !returned_to_lr || interpreted ||
        ctx.lr != 0x80005000 || ctx.gpr[3] != 0x12345678 || ctx.gpr[6] != 41) {
        std::cerr << "FAIL: dispatcher must execute custom hook once without guest fallback\n";
        return 1;
    }
    std::cout << "PASS: undeclared custom HLE hook compiles, replaces guest body, and returns to LR\n";
    return 0;
}
