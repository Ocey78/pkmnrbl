#include "recompiler/recompiler.h"
#include <fstream>
#include <iostream>

// Original synthetic PPC encodings, not copied from any title.
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    nwii::loader::Executable executable;
    nwii::analyzer::Analyzer analyzer(executable);
    nwii::recomp::Recompiler recompiler(analyzer);
    if (std::string(argv[1]) == "--check") {
        auto emit = [&](uint32_t word) {
            nwii::analyzer::Function f{};
            f.start_address = 0x80020000;
            f.end_address = 0x80020008;
            f.instructions = {{0x80020000, word}, {0x80020004, 0x4E800020}};
            return recompiler.generate_function_cpp(f);
        };
        for (uint32_t word : {0x10A3200Du, 0x10A3200Fu, 0x10A3204Du, 0x10A3204Fu,
                              0x10A0204Cu, 0x10A0204Eu}) {
            if (emit(word).find("UNIMPLEMENTED Opcode 4") == std::string::npos) {
                std::cerr << "FAIL: invalid indexed form was accepted\n";
                return 1;
            }
        }
        if (emit(0x10A3202A).find("ctx.ps_add(5, 3, 4)") == std::string::npos ||
            emit(0x10A32420).find("ctx.ps_merge00(5, 3, 4)") == std::string::npos) return 1;
        if (emit(0x10A3200D).find("at 0x80020000") == std::string::npos) {
            std::cerr << "FAIL: unsupported operation must report its static instruction address\n";
            return 1;
        }
        return 0;
    }
    std::ofstream out(argv[1]);
    out << "#include \"runtime/cpu_context.h\"\nusing namespace nwii::runtime;\n";
    const unsigned xos[] = {6, 7, 38, 39};
    for (unsigned op = 0; op < 4; ++op) {
        for (unsigned w = 0; w < 2; ++w) {
            for (unsigned i = 0; i < 8; ++i) {
                for (unsigned scenario = 0; scenario < 6; ++scenario) {
                    const unsigned id = ((op * 2 + w) * 8 + i) * 6 + scenario;
                    const unsigned ra = (scenario == 0 || scenario == 5) && op < 2 ? 0 : 3;
                    const unsigned rb = scenario == 0 || scenario == 4 ? 0 : scenario == 2 ? 3 : 4;
                    const uint32_t word = (4u << 26) | (5u << 21) | (ra << 16) |
                                          (rb << 11) | (w << 10) | (i << 7) | (xos[op] << 1);
                    nwii::analyzer::Function f{};
                    f.start_address = 0x80010000 + id * 16;
                    f.end_address = f.start_address + 8;
                    f.instructions = {{f.start_address, word}, {f.start_address + 4, 0x4E800020}};
                    out << recompiler.generate_function_cpp(f);
                }
            }
        }
    }
    out << "void execute_psq_fixture(CPUContext& ctx, unsigned id) { switch (id) {\n";
    for (unsigned id = 0; id < 384; ++id) {
        out << "case " << std::dec << id << ": ctx.pc = 0x" << std::hex << std::uppercase
            << 0x80010000 + id * 16 << "; func_" << 0x80010000 + id * 16 << "(ctx); return;\n";
    }
    out << "default: throw std::runtime_error(\"bad fixture id\"); } }\n";
    return out.good() ? 0 : 1;
}
