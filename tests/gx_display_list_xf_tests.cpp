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

template <size_t N>
void xf(std::vector<uint8_t>& bytes, uint16_t reg, const std::array<float, N>& values) {
    bytes.push_back(0x10);
    bytes.push_back(uint8_t((N - 1) >> 8));
    bytes.push_back(uint8_t(N - 1));
    bytes.push_back(uint8_t(reg >> 8));
    bytes.push_back(uint8_t(reg));
    for (float value : values) put32(bytes, std::bit_cast<uint32_t>(value));
}
GXCommand* only_draw(std::vector<GXCommand>& commands) {
    GXCommand* result = nullptr;
    for (auto& command : commands) {
        if (command.type != GXCommandType::DrawPrimitive) continue;
        if (result) return nullptr;
        result = &command;
    }
    return result;
}
template <size_t N>
bool bank_starts_with(const std::vector<float>& bank, const std::array<float, N>& values) {
    return bank.size() >= N && std::equal(values.begin(), values.end(), bank.begin());
}
bool decoded_positions_equal(const GXCommand& draw, const Triangle& expected) {
    if (draw.vertices.size() != expected.size()) return false;
    for (size_t i = 0; i < expected.size(); ++i) {
        const auto& vertex = draw.vertices[i];
        if (!vertex.has_pos || !vertex.pre_xf) return false;
        for (size_t axis = 0; axis < 3; ++axis)
            if (vertex.pos[axis] != expected[i][axis]) return false;
    }
    return true;
}
void full_bank_replay(MMU& mmu, uint32_t address, const std::string& bank) {
    constexpr std::array<float, 9> normal_a{1, 0, 0, 0, 1, 0, 0, 0, 1};
    constexpr std::array<float, 9> normal_b{0, -1, 0, 1, 0, 0, 0, 0, 1};
    // The list never loads a matrix. All three original vertices explicitly
    // select matrix index 0, which selects the full-bank snapshot path.
    std::vector<uint8_t> bytes{0x90, 0x00, 0x03};
    for (const auto& xyz : positions) {
        bytes.push_back(0);
        for (float value : xyz) put32(bytes, std::bit_cast<uint32_t>(value));
    }
    bytes.resize(64, 0);
    write_dl(mmu, address, bytes);

    std::vector<uint8_t> first_fifo;
    cp(first_fifo, 0x30, 0);
    cp(first_fifo, 0x50, 0x201); // per-vertex matrix index, direct position
    cp(first_fifo, 0x60, 0);
    cp(first_fifo, 0x70, 9); // XYZ float32; no normal attribute to decode
    xf(first_fifo, 0, matrix);
    xf(first_fifo, 0x400, normal_a);
    first_fifo.push_back(0x40);
    put32(first_fifo, address);
    put32(first_fifo, uint32_t(bytes.size()));
    const uint64_t first_hits = g_prof_dl_hits;
    std::vector<GXCommand> first;
    FifoParser::Parse(first_fifo, first);
    auto* first_draw = only_draw(first);
    check(first_fifo.empty() && first_draw && first_draw->raw &&
              first_draw->vertices.empty(),
          bank + " full-bank first call queues one undecoded draw");
    check(g_prof_dl_hits == first_hits, bank + " full-bank first call is a cache miss");
    if (!first_draw || !first_draw->raw) return;
    check(first_draw->raw->vat.posMatIdx && !first_draw->raw->have_snap_mtx &&
              first_draw->raw->snap_pos_bank.size() == 256 &&
              first_draw->raw->snap_nrm_bank.size() == 96,
          bank + " full-bank fixture captures both matrix banks");
    check(bank_starts_with(first_draw->raw->snap_pos_bank, matrix),
          bank + " first queued draw captures position matrix A");
    // Verify normal snapshot contents without depending on the separate
    // parse-time lighting decision or changing any parsed command fields.
    check(bank_starts_with(first_draw->raw->snap_nrm_bank, normal_a),
          bank + " first queued draw captures normal matrix A");
    const auto original_positions = first_draw->raw->snap_pos_bank;
    const auto original_normals = first_draw->raw->snap_nrm_bank;

    // Only matrices outside the immutable DL change. CP state stays identical,
    // and both commands remain queued until this second parse has completed.
    std::vector<uint8_t> second_fifo;
    xf(second_fifo, 0, changed_matrix);
    xf(second_fifo, 0x400, normal_b);
    second_fifo.push_back(0x40);
    put32(second_fifo, address);
    put32(second_fifo, uint32_t(bytes.size()));
    const uint64_t hits = g_prof_dl_hits;
    std::vector<GXCommand> second;
    FifoParser::Parse(second_fifo, second);
    auto* second_draw = only_draw(second);
    check(second_fifo.empty() && second_draw && second_draw->raw &&
              second_draw->vertices.empty(),
          bank + " full-bank second call queues one undecoded draw");
    check(g_prof_dl_hits == hits + 1, bank + " full-bank second call uses the real cache");
    if (!second_draw || !second_draw->raw) return;
    check(first_draw->raw->snap_pos_bank == original_positions,
          bank + " replay preserves the entire first queued position bank");
    check(first_draw->raw->snap_nrm_bank == original_normals,
          bank + " replay preserves the entire first queued normal bank");
    check(second_draw->raw->snap_pos_bank.size() == 256 &&
              bank_starts_with(second_draw->raw->snap_pos_bank, changed_matrix),
          bank + " cached draw captures current position matrix B");
    check(second_draw->raw->snap_nrm_bank.size() == 96 &&
              bank_starts_with(second_draw->raw->snap_nrm_bank, normal_b),
          bank + " cached draw captures current normal matrix B");
    FifoParser::DecodeDraw(*first_draw);
    FifoParser::DecodeDraw(*second_draw);
    check(decoded_positions_equal(*first_draw, transformed),
          bank + " first queued draw decodes all vertices with A after the second parse");
    check(decoded_positions_equal(*second_draw, changed_transformed),
          bank + " cache-hit draw decodes all vertices with B after the second parse");
    if (!first_draw->vertices.empty() && !second_draw->vertices.empty()) {
        const auto& a = first_draw->vertices.front();
        const auto& b = second_draw->vertices.front();
        std::cout << "OBS " << bank << " full-bank cache_hit_delta="
                  << g_prof_dl_hits - hits << " first_view="
                  << a.pos[0] << ',' << a.pos[1] << ',' << a.pos[2]
                  << " second_view=" << b.pos[0] << ',' << b.pos[1] << ',' << b.pos[2]
                  << '\n';
    }
}
}

int main() {
    nwii::runtime::MMU mmu;
    nwii::runtime::g_mmu = &mmu;
    direct_xf_replay(mmu, 0x80018000, "MEM1");
    direct_xf_replay(mmu, 0x90018000, "MEM2");
    indexed_xf_replay(mmu, 0x80019000, 0x8001a000, "MEM1");
    indexed_xf_replay(mmu, 0x90019000, 0x9001a000, "MEM2");
    full_bank_replay(mmu, 0x8001b000, "MEM1");
    full_bank_replay(mmu, 0x9001b000, "MEM2");
    nwii::runtime::g_mmu = nullptr;
    std::cout << "RESULT checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
