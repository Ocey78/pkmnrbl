#include "runtime/cpu_context.h"
#include <stdexcept>
#include <string>

void func_80004050(nwii::runtime::CPUContext&);

namespace {
struct Interrupted {};
std::string mode;
uint32_t resume_pc = 0;
int loop_visits = 0;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

namespace nwii::runtime {
bool g_trace_calls = false;
void trace_call(uint32_t, CPUContext&) {}
void interpret_step(CPUContext&) {
    throw std::runtime_error("synthetic AOT fixture must not need an interpreter");
}
bool process_pending_callbacks(CPUContext&) {
    require(++loop_visits <= 2, "resumed exhausted loop wrapped CTR");
    return false;
}
// The interrupt source is controlled; the code under test is the actual
// recompiler's emitted C++, including its PC publication and early return.
bool handle_syscall(CPUContext& ctx) {
    resume_pc = ctx.pc;
    if (mode == "interrupt") throw Interrupted{};
    if (mode == "yield") {
        ctx.pc = 0x80005000;
        return true;
    }
    return false;
}
}

int main(int argc, char** argv) {
    mode = argc > 1 ? argv[1] : "normal";
    nwii::runtime::CPUContext ctx;
    ctx.pc = 0x80004050;
    ctx.lr = 0x80006000;
    ctx.ctr = 1;
    ctx.gpr[3] = 0x80008000;
    ctx.gpr[6] = 0;
    try {
        bool interrupted = false;
        try { func_80004050(ctx); }
        catch (const Interrupted&) { interrupted = true; }
        require(resume_pc == 0x8000405C,
                "SC must publish its continuation, not the exhausted loop PC");
        require(ctx.ctr == 0, "one-iteration loop must exhaust CTR exactly once");
        require(ctx.gpr[3] == 0x80008020, "buffer must advance by one cache line");
        if (mode == "interrupt") {
            require(interrupted, "interrupt fixture must leave the AOT function");
            require(ctx.gpr[6] == 0, "continuation must not run before restore");
            ctx.pc = resume_pc;
            mode = "normal";
            func_80004050(ctx);
        } else if (mode == "yield") {
            require(ctx.pc == 0x80005000, "SC must preserve redirected PC");
            require(ctx.gpr[6] == 0, "SC yield must leave native function immediately");
            std::cout << "PASS: syscall yield preserves redirected execution\n";
            return 0;
        }
        require(ctx.pc == 0x80006000 && ctx.gpr[6] == 1 && ctx.ctr == 0,
                "continuation must run once and return without restarting loop");
        std::cout << "PASS: syscall continuation resumes after exhausted loop\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
