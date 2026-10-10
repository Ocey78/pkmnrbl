// Asset-free regression using original commands and the real GX parser/cache.
// Misclassifying a cached direct XF write as LOAD_INDX must fail the payload
// and decoded-position assertions. Genuine LOAD_INDX must still read live RAM.
#include "runtime/cpu_context.h"
#include "runtime/gx/fifo_parser.h"
#include "runtime/gx/renderer.h"
#include "runtime/hw/hw.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// Only unrelated hardware/rendering link boundaries are substituted. Parser,
// decoder, MMU, cache, snapshots, and CP state updates are production code.
namespace nwii::runtime {
MMU* g_mmu = nullptr;
CPUContext* g_ctx_ptr = nullptr;
uint32_t g_watch_addr = 0;
void watch_hit(uint32_t, uint32_t, int) {}
uint16_t HW_Reg_Read16(uint32_t) { return 0; }
uint32_t HW_Reg_Read32(uint32_t) { return 0; }
void HW_Reg_Write16(uint32_t, uint16_t) {}
void HW_Reg_Write32(uint32_t, uint32_t) {}
namespace hw {
void pe_signal_finish() {}
void pe_signal_token(uint32_t, bool) {}
void pi_fifo_get(uint32_t& base, uint32_t& end, uint32_t& wptr) {
    base = end = wptr = 0;
}
void pi_fifo_set_wptr(uint32_t) {}
uint32_t cp_fifo_base_reg() { return 0; }
}
namespace gx {
std::unique_ptr<IRenderer> IRenderer::Create() { return {}; }
}
}

namespace {
using namespace nwii::runtime;
using namespace nwii::runtime::gx;
using Matrix = std::array<float, 12>;
using Triangle = std::array<std::array<float, 3>, 3>;
unsigned checks = 0, failures = 0;
constexpr Matrix matrix{1, 0, 0, 7,
                        0, 1, 0, -3,
                        0, 0, 1, -11};
constexpr Matrix changed_matrix{1, 0, 0, 20,
                                0, 1, 0, 5,
                                0, 0, 1, -5};
constexpr Triangle positions{{{1, 2, 3}, {2, 2, 3}, {1, 3, 3}}};
// Hand-checked expected view positions, independent of the production decoder.
constexpr Triangle transformed{{{8, -1, -8}, {9, -1, -8}, {8, 0, -8}}};
constexpr Triangle changed_transformed{{{21, 7, -2}, {22, 7, -2}, {21, 8, -2}}};

void check(bool pass, const std::string& name) {
    ++checks;
    if (!pass) ++failures;
    std::cout << (pass ? "PASS " : "FAIL ") << name << '\n';
}
void put32(std::vector<uint8_t>& bytes, uint32_t word) {
    for (int shift = 24; shift >= 0; shift -= 8)
        bytes.push_back(uint8_t(word >> shift));
}
void cp(std::vector<uint8_t>& bytes, uint8_t reg, uint32_t value) {
    bytes.push_back(0x08);
    bytes.push_back(reg);
    put32(bytes, value);
}
std::vector<uint8_t> make_dl(bool indexed) {
    std::vector<uint8_t> bytes;
    if (indexed) {
        bytes.push_back(0x20); // LOAD_INDX_A: array 12, index 2, 12 words to XF 0
        put32(bytes, 0x0002b000);
    } else {
        bytes = {0x10, 0x00, 0x0b, 0x00, 0x00}; // direct XF, 12 words to XF 0
        for (float value : matrix) put32(bytes, std::bit_cast<uint32_t>(value));
    }
    bytes.insert(bytes.end(), {0x90, 0x00, 0x03}); // one original triangle
    for (const auto& xyz : positions)
        for (float value : xyz) put32(bytes, std::bit_cast<uint32_t>(value));
    bytes.resize(128, 0);
    return bytes;
}
void write_dl(MMU& mmu, uint32_t address, const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i)
        mmu.write8(address + uint32_t(i), bytes[i]);
}
void write_matrix(MMU& mmu, uint32_t array_base, const Matrix& values) {
    // A nonzero index exercises byte stride as well as the array base.
    for (size_t i = 0; i < values.size(); ++i)
        mmu.write32(array_base + 2 * 48 + uint32_t(i) * 4,
                    std::bit_cast<uint32_t>(values[i]));
}
std::vector<GXCommand> parse(uint32_t address, uint32_t size, bool setup,
                             uint32_t matrix_array = 0) {
    std::vector<uint8_t> bytes;
    if (setup) {
        // Keep all CP setup outside the immutable DL so its entry-state hash
        // is identical on the first call and each subsequent cache hit.
        cp(bytes, 0x30, 0);
        cp(bytes, 0x50, 0x200); // direct positions, default matrix index 0
        cp(bytes, 0x60, 0);
        cp(bytes, 0x70, 9); // XYZ, float32
        cp(bytes, 0xac, matrix_array);
        cp(bytes, 0xbc, 48); // matrix array stride in bytes
    }
    bytes.push_back(0x40);
    put32(bytes, address);
    put32(bytes, size);
    std::vector<GXCommand> commands;
    FifoParser::Parse(bytes, commands);
    check(bytes.empty(), "complete CALL_DL consumed");
    return commands;
}
void validate(std::vector<GXCommand>& commands, const Matrix& wanted_matrix,
              const Triangle& wanted_positions, const std::string& stage) {
    GXCommand* draw = nullptr;
    unsigned xf_count = 0, draw_count = 0;
    bool xf_before_draw = false;
    for (auto& command : commands) {
        if (command.type == GXCommandType::XFRegister) {
            ++xf_count;
            const bool exact = command.reg == 0 && command.length == 11 &&
                command.payload.size() == wanted_matrix.size() &&
                std::equal(wanted_matrix.begin(), wanted_matrix.end(),
                           command.payload.begin());
            check(exact, stage + " XF payload matches the expected matrix");
            unsigned nonzero = 0;
            for (float value : command.payload) nonzero += value != 0;
            std::cout << "OBS " << stage << " xf_words=" << command.payload.size()
                      << " xf_nonzero=" << nonzero << '\n';
        } else if (command.type == GXCommandType::DrawPrimitive) {
            ++draw_count;
            xf_before_draw = xf_count == 1;
            draw = &command;
        }
    }
    check(xf_count == 1 && draw_count == 1 && xf_before_draw && draw && draw->raw,
          stage + " exactly one XF load precedes the deferred draw");
    if (!draw || !draw->raw) return;
    check(draw->raw->have_snap_mtx && draw->raw->snap_pos_bank.empty(),
          stage + " selected-matrix snapshot path isolates XF replay");
    FifoParser::DecodeDraw(*draw);
    bool expected = draw->vertices.size() == wanted_positions.size();
    for (size_t i = 0; i < draw->vertices.size() && i < wanted_positions.size(); ++i) {
        const auto& v = draw->vertices[i];
        expected = expected && v.pre_xf &&
            v.pos[0] == wanted_positions[i][0] &&
            v.pos[1] == wanted_positions[i][1] &&
            v.pos[2] == wanted_positions[i][2];
    }
    check(expected, stage + " decoded positions match the expected transform");
    if (!draw->vertices.empty()) {
        const auto& v = draw->vertices.front();
        std::cout << "OBS " << stage << " first_view=" << v.pos[0] << ','
                  << v.pos[1] << ',' << v.pos[2] << " pre_xf=" << v.pre_xf << '\n';
    }
}
void direct_xf_replay(MMU& mmu, uint32_t address, const std::string& bank) {
    const auto bytes = make_dl(false);
    write_dl(mmu, address, bytes);
    // No command field is initialized or poisoned by this test. Only the real
    // parser constructs GXCommand objects from original synthetic FIFO bytes.
    const uint64_t first_hits = g_prof_dl_hits;
    auto first = parse(address, uint32_t(bytes.size()), true);
    check(g_prof_dl_hits == first_hits, bank + " direct initial call is a cache miss");
    validate(first, matrix, transformed, bank + " direct miss");
    for (unsigned attempt = 1; attempt <= 4; ++attempt) {
        const uint64_t hits = g_prof_dl_hits;
        auto replay = parse(address, uint32_t(bytes.size()), false);
        const auto label = bank + " direct hit" + std::to_string(attempt);
        check(g_prof_dl_hits == hits + 1, label + " uses the real cache");
        validate(replay, matrix, transformed, label);
    }
}
void indexed_xf_replay(MMU& mmu, uint32_t address, uint32_t array_base,
                       const std::string& bank) {
    const auto bytes = make_dl(true);
    write_dl(mmu, address, bytes);
    write_matrix(mmu, array_base, matrix);
    const uint64_t first_hits = g_prof_dl_hits;
    auto first = parse(address, uint32_t(bytes.size()), true, array_base);
    check(g_prof_dl_hits == first_hits, bank + " indexed initial call is a cache miss");
    validate(first, matrix, transformed, bank + " indexed miss");

    // The DL bytes and all CP registers stay unchanged. An authentic indexed
    // load must refresh from this RAM update even when its DL is cached.
    write_matrix(mmu, array_base, changed_matrix);
    const uint64_t hits = g_prof_dl_hits;
    auto replay = parse(address, uint32_t(bytes.size()), false);
    check(g_prof_dl_hits == hits + 1, bank + " indexed replay uses the real cache");
    validate(replay, changed_matrix, changed_transformed, bank + " indexed hit");
}
}

int main() {
    nwii::runtime::MMU mmu;
    nwii::runtime::g_mmu = &mmu;
    direct_xf_replay(mmu, 0x80018000, "MEM1");
    direct_xf_replay(mmu, 0x90018000, "MEM2");
    indexed_xf_replay(mmu, 0x80019000, 0x8001a000, "MEM1");
    indexed_xf_replay(mmu, 0x90019000, 0x9001a000, "MEM2");
    nwii::runtime::g_mmu = nullptr;
    std::cout << "RESULT checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
