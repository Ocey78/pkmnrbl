#include "recompiler/recompiler.h"

#include <array>
#include <fstream>
#include <iomanip>

// Original synthetic instructions only. Each independently callable snippet
// executes one instruction and returns through the ordinary native AOT path.
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    nwii::loader::Executable executable;
    nwii::analyzer::Analyzer analyzer(executable);
    nwii::recomp::Recompiler recompiler(analyzer);

    const auto spr_word = [](unsigned spr, unsigned reg, unsigned xo) {
        return (31u << 26) | (reg << 21) | ((spr & 31u) << 16) |
               ((spr >> 5) << 11) | (xo << 1);
    };
    const auto zero_word = [](unsigned opcode, unsigned ra) {
        return (opcode << 26) | (ra << 16) | (6u << 11) | (1014u << 1);
    };
    // IDs 0..2 write HID2/DMAU/DMAL from r3; IDs 3..5 read them into r4.
    // IDs 6/7 are dcbz_l with RA5/RA0, IDs 8/9 ordinary dcbz with RA5/RA0.
    const auto cache_word = [](unsigned xo, unsigned ra) {
        return (31u << 26) | (ra << 16) | (6u << 11) | (xo << 1);
    };
    const std::array<uint32_t, 14> words = {
        spr_word(920, 3, 467), spr_word(922, 3, 467), spr_word(923, 3, 467),
        spr_word(920, 4, 339), spr_word(922, 4, 339), spr_word(923, 4, 339),
        zero_word(4, 5), zero_word(4, 0), zero_word(31, 5), zero_word(31, 0),
        cache_word(470, 5), cache_word(470, 0), cache_word(86, 0), cache_word(54, 0)
    };

    std::ofstream out(argv[1]);
    out << "#include \"runtime/cpu_context.h\"\n#include <stdexcept>\n"
           "using namespace nwii::runtime;\n";
    for (unsigned id = 0; id < words.size(); ++id) {
        const uint32_t addr = 0x80038000u + id * 16;
        nwii::analyzer::Function function{};
        function.start_address = addr;
        function.end_address = addr + 8;
        function.instructions = {{addr, words[id]}, {addr + 4, 0x4E800020}};
        out << recompiler.generate_function_cpp(function);
    }
    out << "void execute_locked_cache_fixture(CPUContext& ctx, unsigned id) { switch (id) {\n";
    for (unsigned id = 0; id < words.size(); ++id) {
        const uint32_t addr = 0x80038000u + id * 16;
        out << "case " << std::dec << id << ": ctx.pc=0x" << std::hex << std::uppercase
            << addr << "; func_" << addr << "(ctx); return;\n";
    }
    out << "default: throw std::runtime_error(\"bad locked-cache fixture id\"); } }\n";
    return out.good() ? 0 : 1;
}
