#include "recompiler/recompiler.h"
#include <filesystem>
#include <iostream>

// Original synthetic PPC: executing the guest body would increment r6.
// An explicit custom hook must compile without a runtime-header declaration.
int main(int argc, char** argv) {
    if (argc != 3) return 1;
    nwii::loader::Executable executable;
    executable.entry_point = 0x80004000;
    nwii::loader::Section section{};
    section.address = executable.entry_point;
    section.is_text = true;
    for (uint32_t word : {0x38C60001u, 0x4E800020u}) {
        for (int shift : {24, 16, 8, 0})
            section.data.push_back(static_cast<uint8_t>(word >> shift));
    }
    section.size = static_cast<uint32_t>(section.data.size());
    executable.sections.push_back(section);
    nwii::analyzer::Analyzer analyzer(executable);
    analyzer.analyze({executable.entry_point});
    if (analyzer.get_functions().size() != 1) {
        std::cerr << "Fixture must contain exactly one guest function\n";
        return 1;
    }
    nwii::recomp::RecompilerConfig config;
    config.output_dir = argv[1];
    config.split_output = std::string(argv[2]) == "split";
    config.hle_hooks.emplace(executable.entry_point, "NativeControllerProbeTest");
    std::filesystem::create_directories(config.output_dir);
    nwii::recomp::Recompiler recompiler(analyzer, nullptr, config);
    return recompiler.generate_cpp(executable.entry_point).empty() ? 1 : 0;
}
