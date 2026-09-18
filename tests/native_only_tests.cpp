#include "runtime/cpu_context.h"
#include <string_view>

extern "C" void run_game(nwii::runtime::CPUContext&);
void func_80006000(nwii::runtime::CPUContext&);

namespace {
bool yielded = false;
bool finished = false;
unsigned visits = 0;
}

namespace nwii::runtime {
bool g_trace_calls = false;
void trace_call(uint32_t, CPUContext&) {}
bool process_pending_callbacks(CPUContext& ctx) {
    if (ctx.pc == 0x80005000 || ++visits > 10) {
        finished = ctx.pc == 0x80005000;
        ctx.is_running = false;
        ctx.pc = 0xFFFFFFFC;
        return true;
    }
    if (ctx.pc == 0x80004014 && !yielded) {
        yielded = true;
        return true;
    }
    return false;
}
}

int main(int argc, char** argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "covered";
    nwii::runtime::CPUContext ctx;
    ctx.pc = 0x80004010;
    ctx.lr = 0x80005000;
    ctx.ctr = 0x12345678;
    ctx.gpr[4] = 0x8000;

    // The real runtime registers text ranges even when interpretation is off.
    nwii::runtime::add_recompiled_range(0x80004000, 0x8000600C);
    if (mode == "micro") {
        ctx.pc = 0x80006000;
        func_80006000(ctx);
        std::cerr << "FAIL: unsupported instruction returned to native code\n";
        return 0; // The subprocess regression requires a failing exit.
    }
    if (mode == "step") {
        // The sparse outer AOT function spans this hole in both output layouts.
        ctx.pc = 0x80004008;
    } else if (mode != "covered") {
        std::cerr << "FAIL: unknown test mode\n";
        return 1;
    }
    run_game(ctx);
    if (mode == "step") {
        std::cerr << "FAIL: uncovered PC returned to native code\n";
        return 0;
    }
    if (!finished || !yielded || ctx.gpr[6] != 1 || ctx.msr != 0x8000) {
        std::cerr << "FAIL: native-only AOT continuation must run exactly once\n";
        return 1;
    }
    std::cout << "PASS: generated AOT continuation runs without an interpreter\n";
}
