#include "recompiler/recompiler.h"
#include <toml++/toml.hpp>
#include <filesystem>

// Original synthetic instructions, not title code. Exercise the title's real
// discovery configuration with an otherwise unreachable indirect branch stub.
int main(int argc, char** argv) {
    if (argc != 4) return 1;
    nwii::loader::Executable executable;
    executable.entry_point = 0x80004000;
    auto section = [&](uint32_t address, std::initializer_list<uint32_t> words) {
        nwii::loader::Section text{};
        text.address = address;
        text.is_text = true;
        for (auto word : words)
            for (int shift : {24, 16, 8, 0})
                text.data.push_back(static_cast<uint8_t>(word >> shift));
        text.size = static_cast<uint32_t>(text.data.size());
        executable.sections.push_back(text);
    };
    section(0x80004000, {0x4E800420}); // bctr: target supplied at runtime
    section(0x8012B740, {0x48000000 | (0x80200000 - 0x8012B740)});
    section(0x80200000, {0x38C60001, 0x4E800020}); // addi r6,r6,1; blr

    const auto title = toml::parse_file(argv[3]);
    nwii::recomp::SymbolTable symbols;
    const auto csv = title["symbols_csv"].value_or(std::string());
    if (!csv.empty() && !symbols.load_csv(csv)) return 1;
    std::vector<uint32_t> roots;
    for (const auto& [address, name] : symbols.get_all_symbols())
        roots.push_back(address);
    nwii::analyzer::Analyzer analyzer(executable);
    analyzer.analyze(roots);
    nwii::recomp::RecompilerConfig config;
    config.output_dir = argv[1];
    config.split_output = std::string(argv[2]) == "split";
    std::filesystem::create_directories(config.output_dir);
    nwii::recomp::Recompiler recompiler(analyzer, &symbols, config);
    return recompiler.generate_cpp(executable.entry_point).empty() ? 1 : 0;
}
