#include "recompiler/recompiler.h"
#include <fstream>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    nwii::loader::Executable executable;
    nwii::analyzer::Analyzer analyzer(executable);
    nwii::recomp::Recompiler recompiler(analyzer);
    auto emit = [&](uint32_t addr, uint32_t word) {
        nwii::analyzer::Function f{};
        f.start_address = addr;
        f.end_address = addr + 8;
        f.instructions = {{addr, word}, {addr + 4, 0x4E800020}};
        return recompiler.generate_function_cpp(f);
    };
    if (std::string(argv[1]) == "--check") {
        // Invalid RA0 must remain on the fail-fast unsupported path.
        return emit(0x80030000, 0xC4A00008).find("micro_interpret") == std::string::npos ||
               emit(0x80030000, 0xD4A00008).find("micro_interpret") == std::string::npos ||
               emit(0x80030000, 0xCCA60008).find("ctx.mmu.read_f64") == std::string::npos ||
               emit(0x80030000, 0xDCA60008).find("ctx.mmu.write_f64") == std::string::npos;
    }
    std::ofstream out(argv[1]);
    out << "#include \"runtime/cpu_context.h\"\nusing namespace nwii::runtime;\n";
    const int offsets[] = {8, -8, 0, 32767, -32768, 48};
    for (unsigned store = 0; store < 2; ++store) {
        for (unsigned scenario = 0; scenario < 6; ++scenario) {
            const unsigned id = store * 6 + scenario;
            const unsigned ra = scenario == 2 ? 5 : 6; // FPR5 and GPR5 are distinct.
            // Independent original synthetic encodings; no title payload.
            const uint32_t word = ((store ? 53u : 49u) << 26) | (5u << 21) |
                                  (ra << 16) | static_cast<uint16_t>(offsets[scenario]);
            out << emit(0x80030000 + id * 16, word);
        }
    }
    out << "void execute_float_update_fixture(CPUContext& ctx, unsigned id) { switch(id) {\n";
    for (unsigned id = 0; id < 12; ++id)
        out << "case " << std::dec << id << ": ctx.pc=0x" << std::hex << std::uppercase
            << 0x80030000 + id * 16 << "; func_" << 0x80030000 + id * 16 << "(ctx); return;\n";
    out << "default: throw std::runtime_error(\"bad float fixture id\"); } }\n";
    return out.good() ? 0 : 1;
}
