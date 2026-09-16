#include "recompiler/recompiler.h"
#include <fstream>

// Original synthetic instructions, not bytes extracted from a title. Exercise
// an exhausted CTR loop followed by an interruptible system-call boundary.
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    nwii::loader::Executable executable;
    nwii::analyzer::Analyzer analyzer(executable);
    nwii::recomp::Recompiler recompiler(analyzer);
    nwii::analyzer::Function function{};
    function.start_address = 0x80004050;
    function.end_address = 0x80004064;
    function.instructions = {
        {0x80004050, 0x38630020}, // addi r3,r3,32
        {0x80004054, 0x4200FFFC}, // bdnz 0x80004050
        {0x80004058, 0x44000002}, // sc
        {0x8000405C, 0x38C60001}, // addi r6,r6,1
        {0x80004060, 0x4E800020}, // blr
    };
    std::ofstream output(argv[1]);
    output << "#include \"runtime/cpu_context.h\"\n"
              "using namespace nwii::runtime;\n"
           << recompiler.generate_function_cpp(function);
    return output.good() ? 0 : 1;
}
