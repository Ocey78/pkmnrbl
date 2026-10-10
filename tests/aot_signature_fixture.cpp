#include "recompiler/recompiler.h"
#include <filesystem>
#include <iostream>
#include <string_view>

namespace {
constexpr uint32_t d_form(uint32_t opcode, uint32_t rt, uint32_t ra, int16_t value) {
    return (opcode << 26) | (rt << 21) | (ra << 16) | static_cast<uint16_t>(value);
}
constexpr uint32_t move_register(uint32_t destination, uint32_t source) {
    return (31u << 26) | (source << 21) | (destination << 16) | (source << 11) | (444u << 1);
}
void append_word(nwii::loader::Section& section, uint32_t word) {
    for (int shift : {24, 16, 8, 0})
        section.data.push_back(static_cast<uint8_t>(word >> shift));
}
}

int main(int argc, char** argv) {
    const bool check_only = argc == 2 && std::string_view(argv[1]) == "--check";
    if (!check_only && argc != 3) return 1;
    nwii::loader::Executable executable;
    executable.entry_point = 0x80004000;
    nwii::loader::Section body{};
    body.address = executable.entry_point;
    body.is_text = true;
    // Original synthetic function, assembled from the PPC ABI: a frame,
    // saved LR, an unrelated helper call, and three argument captures. This
    // generic compiler prologue is not evidence of an IOS_Ioctlv contract.
    const uint32_t read_lr = (31u << 26) | (8u << 16) | (339u << 1);
    const uint32_t write_lr = (31u << 26) | (8u << 16) | (467u << 1);
    const uint32_t return_lr = (19u << 26) | (20u << 21) | (16u << 1);
    for (uint32_t word : {
             d_form(37, 1, 1, -112), read_lr, d_form(36, 0, 1, 116),
             d_form(14, 11, 1, 112),
             (18u << 26) | (0x80004200u - 0x80004010u) | 1u,
             move_register(27, 3), move_register(28, 4), move_register(29, 5),
             d_form(14, 6, 0, 37), d_form(32, 0, 1, 116), write_lr,
             d_form(14, 1, 1, 112), return_lr})
        append_word(body, word);
    body.size = static_cast<uint32_t>(body.data.size());
    executable.sections.push_back(body);
    nwii::loader::Section saved_arguments{};
    saved_arguments.address = 0x80004100;
    saved_arguments.is_text = true;
    // A second original ABI variant saves two callee-saved registers before
    // capturing arguments. That also does not establish an async IOS ABI.
    for (uint32_t word : {
             d_form(37, 1, 1, -112), read_lr, d_form(36, 0, 1, 116),
             d_form(36, 31, 1, 108), move_register(31, 5),
             d_form(36, 30, 1, 104), move_register(30, 4),
             (18u << 26) | (0x80004200u - 0x8000411Cu) | 1u,
             move_register(8, 30), move_register(9, 31), d_form(14, 6, 0, 73),
             d_form(32, 30, 1, 104), d_form(32, 31, 1, 108),
             d_form(32, 0, 1, 116), write_lr, d_form(14, 1, 1, 112), return_lr})
        append_word(saved_arguments, word);
    saved_arguments.size = static_cast<uint32_t>(saved_arguments.data.size());
    executable.sections.push_back(saved_arguments);
    nwii::loader::Section helper{};
    helper.address = 0x80004200;
    helper.is_text = true;
    append_word(helper, d_form(14, 7, 7, 1));
    append_word(helper, return_lr);
    helper.size = static_cast<uint32_t>(helper.data.size());
    executable.sections.push_back(helper);
    nwii::analyzer::Analyzer analyzer(executable);
    analyzer.analyze({executable.entry_point, saved_arguments.address});
    if (check_only) {
        for (uint32_t address : {executable.entry_point, saved_arguments.address}) {
            const auto& function = analyzer.get_functions().at(address);
            if (!function.hle_hook_name.empty()) {
                std::cerr << "FAIL: unrelated ABI prologue at 0x" << std::hex << address
                          << " was classified as " << function.hle_hook_name << '\n';
                return 1;
            }
        }
        std::cout << "PASS: both unrelated ABI prologues retain guest execution\n";
        return 0;
    }
    nwii::recomp::RecompilerConfig config;
    config.output_dir = argv[1];
    config.split_output = std::string_view(argv[2]) == "split";
    std::filesystem::create_directories(config.output_dir);
    nwii::recomp::Recompiler recompiler(analyzer, nullptr, config);
    return recompiler.generate_cpp(executable.entry_point).empty() ? 1 : 0;
}
