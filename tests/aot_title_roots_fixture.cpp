#include "recompiler/recompiler.h"
#include <toml++/toml.hpp>
#include <filesystem>

// Original synthetic instructions, not title code. Exercise the title's real
// discovery configuration with an otherwise unreachable indirect branch stub.
int main(int argc, char** argv) {
    if (argc != 4) return 1;
    nwii::loader::Executable executable;
    executable.entry_point = 0x80004000;
    auto section = [&](uint32_t address, std::initializer_list<uint32_t> words,
                       bool is_text = true) {
        nwii::loader::Section text{};
        text.address = address;
        text.is_text = is_text;
        for (auto word : words)
            for (int shift : {24, 16, 8, 0})
                text.data.push_back(static_cast<uint8_t>(word >> shift));
        text.size = static_cast<uint32_t>(text.data.size());
        executable.sections.push_back(text);
    };
    section(0x80004000, {0x4E800420}); // bctr: target supplied at runtime
    section(0x8012B740, {0x48000000 | (0x80200000 - 0x8012B740)});
    section(0x80200000, {0x38C60001, 0x4E800020}); // addi r6,r6,1; blr

    // Original callback fixture: the data pointer discovers only the caller.
    // The callback address enters memory at runtime, so its body is reachable
    // only through the real title's configured AOT roots.
    section(0x80006000, {0x80004100}, false);
    section(0x80004100, {
        0x7FC802A6, // mflr r30: preserve caller's return address
        0x81830038, // lwz r12,0x38(r3): callback stored on an object
        0x7D8903A6, // mtctr r12
        0x4E800421, // bctrl: callback must return to 0x80004110
        0x39080001, // addi r8,r8,1: observable resumed caller
        0x7FC803A6, // mtlr r30
        0x4E800020  // blr
    });
    section(0x8028F440, {
        0x7D2802A6, // mflr r9: report exact callback return address
        0x38E70001, // addi r7,r7,1: observable callback body
        0x4E800020  // blr
    });

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
