// Asset-free regression using the real display-list parser, snapshot, and MMU.
// Incorrect MEM2 addressing, bank-colliding snapshot keys, or missing complete-
// span validation must fail these expected-correctness assertions.
#include "runtime/cpu_context.h"
#include "runtime/gx/fifo_parser.h"
#include "runtime/gx/renderer.h"
#include "runtime/hw/hw.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// Only unrelated link boundaries are substituted. No parser, snapshot, fetch,
// MMU, or CP state update is replaced.
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
unsigned checks = 0, failures = 0;
constexpr uint32_t kMarker = 0x0055aacc;
void check(bool result, const std::string& name) {
    ++checks;
    if (!result) ++failures;
    std::cout << (result ? "PASS " : "FAIL ") << name << '\n';
}
void put32(std::vector<uint8_t>& bytes, uint32_t word) {
    for (int shift = 24; shift >= 0; shift -= 8)
        bytes.push_back(uint8_t(word >> shift));
}
void cp(std::vector<uint8_t>& bytes, uint8_t reg, uint32_t word) {
    bytes.push_back(0x08);
    bytes.push_back(reg);
    put32(bytes, word);
}
std::vector<uint8_t> list(uint32_t value) {
    std::vector<uint8_t> bytes;
    cp(bytes, 0xa3, value);
    bytes.resize(32, 0); // one CP write and original NOP padding
    return bytes;
}
void write(MMU& mmu, uint32_t address, const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i)
        mmu.write8(address + uint32_t(i), bytes[i]);
}
std::vector<GXCommand> parse_call(uint32_t address, uint32_t length) {
    std::vector<uint8_t> fifo{0x40};
    put32(fifo, address);
    put32(fifo, length);
    cp(fifo, 0xa4, kMarker); // prove the parent stream remains consumable
    std::vector<GXCommand> commands;
    FifoParser::Parse(fifo, commands);
    check(fifo.empty(), "CALL_DL consumes its complete parent FIFO framing");
    return commands;
}
bool is_cp(const GXCommand& c, uint8_t reg, uint32_t value) {
    return c.type == GXCommandType::CPRegister && c.reg == reg && c.val == value;
}
void expect_list(uint32_t address, uint32_t value, const std::string& name) {
    const auto commands = parse_call(address, 32);
    check(commands.size() == 2 && is_cp(commands.front(), 0xa3, value) &&
              is_cp(commands.back(), 0xa4, kMarker), name);
    std::cout << "OBS addr=0x" << std::hex << address << std::dec
              << " expanded_commands=" << commands.size() -
                  (!commands.empty() && is_cp(commands.back(), 0xa4, kMarker) ? 1 : 0)
              << '\n';
}
void expect_rejected(uint32_t address, uint32_t length, const std::string& name) {
    const auto commands = parse_call(address, length);
    check(commands.size() == 1 && is_cp(commands.front(), 0xa4, kMarker), name);
}
bool fetched_equals(uint32_t address, uint32_t length, const std::vector<uint8_t>& wanted) {
    std::vector<uint8_t> got;
    return GX_DL_Fetch(address, length, got) && got == wanted;
}
void expect_no_snapshot(uint32_t address, uint32_t length, const std::string& name) {
    GX_DL_Snapshot(address, length);
    std::vector<uint8_t> out;
    check(!GX_DL_Fetch(address, length, out), name);
}
int decode_diagnostic_fixture() {
    GXCommand command{};
    command.type = GXCommandType::DrawPrimitive;
    command.prim_type = 0x90;
    command.raw = std::make_shared<DrawRaw>();
    auto& raw = *command.raw;
    raw.vat = {};
    raw.vat.posMatIdx = true;
    raw.vat.posMask = VtxAttrMask::Direct;
    raw.vat.posType = VtxAttrType::F32;
    raw.vat.posElements = true;
    raw.need_normal = true; // qualify as a lit draw without unrelated attributes
    raw.count = 3;
    raw.snap_pos_bank.assign(256, 0.0f);
    raw.snap_pos_bank[12] = 1.0f; // another slot is nonzero; selected slot 0 is zero
    for (unsigned vertex = 0; vertex < 3; ++vertex) {
        raw.bytes.push_back(0); // per-vertex position-matrix index
        for (float component : {2.0f + vertex, 3.0f + vertex, 4.0f + vertex}) {
            uint32_t bits;
            std::memcpy(&bits, &component, sizeof(bits));
            put32(raw.bytes, bits);
        }
    }
    FifoParser::DecodeDraw(command);
    check(command.vertices.size() == 3, "lit diagnostic fixture decodes all original vertices");
    bool zero = command.vertices.size() == 3;
    bool transformed = zero;
    for (const auto& vertex : command.vertices) {
        zero &= vertex.pos[0] == 0 && vertex.pos[1] == 0 && vertex.pos[2] == 0;
        transformed &= vertex.pre_xf;
    }
    check(zero, "zero selected bank transforms nonzero object positions to zero");
    check(transformed, "zero selected bank still marks vertices pre-transformed");
    return failures ? 1 : 0;
}
}

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "decode-diagnostic")
        return decode_diagnostic_fixture();
    using namespace nwii::runtime;
    MMU mmu;
    g_mmu = &mmu;
    std::cout << "Production MMU guest sizes: MEM1=" << mmu.mem1.size()-8
              << " MEM2=" << mmu.mem2.size()-8 << "\n";

    const auto mem1 = list(0x00112233), mem2 = list(0x00445566);
    write(mmu, 0x80002000, mem1);
    write(mmu, 0x90002000, mem2);
    for (uint32_t a : {0x00002000u, 0x80002000u, 0xc0002000u}) {
        check(mmu.get_ptr(a) == mmu.mem1.data()+0x2000, "MMU maps MEM1 physical/cached/uncached aliases");
        expect_list(a, 0x00112233, "MEM1 CALL_DL expands real CP write");
    }
    for (uint32_t a : {0x10002000u, 0x90002000u, 0xd0002000u}) {
        check(mmu.get_ptr(a) == mmu.mem2.data()+0x2000, "MMU maps MEM2 physical/cached/uncached aliases");
        expect_list(a, 0x00445566, "MEM2 CALL_DL expands real CP write");
    }

    write(mmu, 0x817fffe0, mem1);
    write(mmu, 0x93ffffe0, mem2);
    expect_list(0x817fffe0, 0x00112233, "MEM1 exact end span is valid");
    expect_list(0x93ffffe0, 0x00445566, "MEM2 exact end span is valid");
    expect_rejected(0x817ffffa, 7, "MEM1 one-byte-over-end CALL_DL rejected");
    expect_rejected(0x93fffffa, 7, "MEM2 one-byte-over-end CALL_DL rejected");
    expect_rejected(0x81800000, 32, "MEM1 one-past-end CALL_DL rejected");
    expect_rejected(0x94000000, 32, "MEM2 one-past-end CALL_DL rejected");
    expect_rejected(0xcc006000, 32, "MMIO CALL_DL rejected");
    expect_rejected(0xfffffff0, 32, "wrapping CALL_DL rejected");
    expect_rejected(0x80003000, 0, "empty CALL_DL rejected");
    expect_rejected(0x80003000, 0x400001, "over-parser-cap CALL_DL rejected");

    const auto a = list(0x00010203), b = list(0x00040506), changed = list(0x00070809);
    write(mmu, 0x80004000, a);
    write(mmu, 0x90004000, b);
    GX_DL_Snapshot(0x80004000, 32);
    for (uint32_t alias : {0x00004000u, 0x80004000u, 0xc0004000u})
        check(fetched_equals(alias, 32, a), "MEM1 snapshot aliases preserve original bytes");
    std::vector<uint8_t> unrelated;
    check(!GX_DL_Fetch(0x90004000, 32, unrelated), "MEM2 snapshot key must not alias existing MEM1 key");
    write(mmu, 0x80004000, changed);
    GX_DL_Snapshot(0x90004000, 32);
    check(fetched_equals(0x80004000, 32, a), "snapshot of MEM2 must not replace MEM1 snapshot");
    for (uint32_t alias : {0x10004000u, 0x90004000u, 0xd0004000u})
        check(fetched_equals(alias, 32, b), "MEM2 snapshot aliases must contain MEM2 bytes");

    // Also cover MEM2 offsets beyond the size of MEM1: these must remain in
    // the second bank instead of depending on an invalid MEM1 pointer.
    write(mmu, 0x93005000, b);
    GX_DL_Snapshot(0xd3005000, 32);
    check(fetched_equals(0x13005000, 32, b), "MEM2 high-offset snapshot preserves second-bank bytes");
    GX_DL_Snapshot(0x817fffe0, 32);
    check(fetched_equals(0xc17fffe0, 32, mem1), "MEM1 exact-end snapshot accepted");
    GX_DL_Snapshot(0x93ffffe0, 32);
    check(fetched_equals(0xd3ffffe0, 32, mem2), "MEM2 exact-end snapshot accepted");

    // Cross only one byte into MMU's allocated +8 padding, never outside host
    // allocations. These prove guest-span validation without provoking UB.
    expect_no_snapshot(0x817ffffa, 7, "MEM1 snapshot must reject guest span entering host padding");
    expect_no_snapshot(0x93fffffa, 7, "MEM2 snapshot must reject guest span crossing end");
    expect_no_snapshot(0x81800020, 16, "unmapped snapshot must reject rather than cache zeros");
    expect_no_snapshot(0xcc006040, 16, "MMIO snapshot must reject rather than cache HW reads");
    expect_no_snapshot(0xfffffff0, 32, "wrapping snapshot must reject");
    expect_no_snapshot(0x80006000, 0, "zero snapshot rejected");
    expect_no_snapshot(0x80006000, 0x800001, "over-snapshot-cap rejected");
    g_mmu = nullptr;
    expect_no_snapshot(0x80007000, 32, "missing-MMU snapshot rejected");

    std::cout << "RESULT checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
