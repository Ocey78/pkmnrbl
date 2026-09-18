#include "recompiler/recompiler.h"
#include <fstream>

// Synthetic instructions exercise the emitter's unsupported-instruction path.
// The supported first instruction leaves ctx.pc stale, so the trap must report
// the explicit instruction PC supplied by the generated micro_interpret call.
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    nwii::loader::Executable executable;
    nwii::analyzer::Analyzer analyzer(executable);
    nwii::recomp::Recompiler recompiler(analyzer);
    nwii::analyzer::Function function{};
    function.start_address = 0x80006000;
    function.end_address = 0x8000600C;
    function.instructions = {
        {0x80006000, 0x38C60001}, // addi r6,r6,1
        {0x80006004, 0x00000000}, // reserved opcode: must never execute natively
        {0x80006008, 0x4E800020}, // blr
    };
    std::ofstream output(argv[1]);
    output << "#include \"runtime/cpu_context.h\"\n"
              "using namespace nwii::runtime;\n"
           << recompiler.generate_function_cpp(function);
    return output.good() ? 0 : 1;
}
