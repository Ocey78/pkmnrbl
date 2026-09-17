#include "recompiler/recompiler.h"
#include <filesystem>
#include <iostream>

// Original code: a sparse outer function spans a separate interrupt-enabling
// function. Range-only dispatch must not select the hole in the outer function.
int main(int argc, char** argv) {
    if (argc != 3) return 1;
    nwii::loader::Executable executable;
    executable.entry_point = 0x80004010;
    nwii::loader::Section section{};
    section.address = 0x80004000;
    section.is_text = true;
    for (uint32_t word : {0x48000020u, 0u, 0u, 0u,
                          0x7C800124u, 0x38C60001u, 0x4E800020u, 0u,
                          0x4E800020u}) {
        for (int shift : {24, 16, 8, 0})
            section.data.push_back(static_cast<uint8_t>(word >> shift));
    }
    section.size = static_cast<uint32_t>(section.data.size());
    executable.sections.push_back(section);
    nwii::analyzer::Analyzer analyzer(executable);
    analyzer.analyze({0x80004000});
    const auto& outer = analyzer.get_functions().at(0x80004000);
    if (outer.end_address <= 0x80004014) {
        std::cerr << "Fixture did not produce overlapping ranges\n";
        return 1;
    }
    for (const auto& instruction : outer.instructions) {
        if (instruction.address == 0x80004014) return 1;
    }
    nwii::recomp::RecompilerConfig config;
    config.output_dir = argv[1];
    config.split_output = std::string(argv[2]) == "split";
    std::filesystem::create_directories(config.output_dir);
    nwii::recomp::Recompiler recompiler(analyzer, nullptr, config);
    return recompiler.generate_cpp(executable.entry_point).empty() ? 1 : 0;
}
