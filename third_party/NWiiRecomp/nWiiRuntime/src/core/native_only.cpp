#include "runtime/cpu_context.h"
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

namespace nwii::runtime {
namespace {
[[noreturn]] void fail_native_coverage(const CPUContext& ctx, const char* reason,
                                      uint32_t pc, const uint32_t* opcode) {
    std::fprintf(stderr,
                 "[AOT] %s: pc=0x%08" PRIX32 " lr=0x%08" PRIX32
                 " ctr=0x%08" PRIX32,
                 reason, pc, ctx.lr, ctx.ctr);
    if (opcode) std::fprintf(stderr, " opcode=0x%08" PRIX32, *opcode);
    std::fputs("; interpreter disabled\n", stderr);
    std::fflush(stderr);
    // This can run on the CPU worker through the extern-C dispatcher. Do not
    // throw across that boundary or report a missing AOT path as a clean quit.
    std::exit(EXIT_FAILURE);
}
}

void interpret_step(CPUContext& ctx) {
    fail_native_coverage(ctx, "uncovered AOT PC", ctx.pc, nullptr);
}

void micro_interpret(CPUContext& ctx, uint32_t opcode, uint32_t pc) {
    // Generated straight-line code need not publish each instruction to ctx.pc.
    fail_native_coverage(ctx, "unsupported AOT instruction", pc, &opcode);
}

void add_recompiled_range(uint32_t, uint32_t) {
    // Registration only tells the bring-up interpreter when it can return to
    // AOT code. The loader also calls it in native-only builds, which never
    // interpret or consult these ranges.
}
}
