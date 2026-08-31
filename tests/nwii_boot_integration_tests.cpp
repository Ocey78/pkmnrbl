#include "runtime/boot/nwii_guest_memory.h"
#include "runtime/cpu_context.h"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace nwii::runtime {

uint32_t g_watch_addr = 0;

void watch_hit(uint32_t, uint32_t, int) {}
void GX_WGPIPE_Write8(uint8_t) {}
void GX_WGPIPE_Write16(uint16_t) {}
void GX_WGPIPE_Write32(uint32_t) {}
void GX_DL_Snapshot(uint32_t, uint32_t) {}
bool GX_DL_Fetch(uint32_t, uint32_t, std::vector<uint8_t>&) { return false; }
void GX_WGPIPE_WriteF32(float) {}
void GX_WGPIPE_WriteF64(double) {}
uint16_t HW_Reg_Read16(uint32_t) { return 0; }
uint32_t HW_Reg_Read32(uint32_t) { return 0; }
void HW_Reg_Write16(uint32_t, uint16_t) {}
void HW_Reg_Write32(uint32_t, uint32_t) {}

}  // namespace nwii::runtime

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void test_installs_wpse01_boot_contract_into_real_nwii_mmu() {
    // Removing the adapter's call to MMU::write32, choosing the wrong address,
    // or writing little-endian bytes must make this integration test fail.
    nwii::runtime::CPUContext context;
    context.mmu.mem1.at(0x1FU) = 0xA5U;
    context.mmu.mem1.at(0x38U) = 0x3CU;
    context.mmu.mem1.at(0x3108U) = 0x6DU;
    context.mmu.mem1.at(0x3114U) = 0xD6U;
    context.mmu.mem1.at(0x3157U) = 0x9EU;
    context.mmu.mem1.at(0x315CU) = 0xE9U;

    std::string error;
    require(install_wpse01_boot_memory(context, 0x81000000U, 0x81700000U, error),
            "the validated WPSE01 boot contract must install");
    require(error.empty(), "a successful boot-memory installation must not report an error");

    require(context.mmu.read32(0x80000020U) == 0x0D15EA5EU,
            "boot magic must use the literal WPSE01 value");
    require(context.mmu.read32(0x8000310CU) == 0x81000000U,
            "MEM1 usable start must match the validated arena low bound");
    require(context.mmu.read32(0x80003110U) == 0x81700000U,
            "MEM1 usable end must match the validated arena high bound");
    require(context.mmu.read32(0x80003124U) == 0x90000800U,
            "MEM2 usable start must use the WPSE01 literal bound");
    require(context.mmu.read32(0x80003128U) == 0x93E00000U,
            "MEM2 usable end must use the WPSE01 literal bound");
    require(context.mmu.read32(0x80003130U) == 0x93E00000U,
            "IPC start must use the WPSE01 literal bound");
    require(context.mmu.read32(0x80003134U) == 0x94000000U,
            "IPC end must use the WPSE01 literal bound");

    require(context.mmu.read8(0x80000020U) == 0x0DU &&
                context.mmu.read8(0x80000021U) == 0x15U &&
                context.mmu.read8(0x80000022U) == 0xEAU &&
                context.mmu.read8(0x80000023U) == 0x5EU,
            "the real MMU must contain big-endian boot-magic bytes");

    require(context.mmu.read8(0x8000001FU) == 0xA5U,
            "the byte before boot magic must remain untouched");
    require(context.mmu.read8(0x80000038U) == 0x3CU,
            "the byte after the MEM1 arena-high field must remain untouched");
    require(context.mmu.read8(0x80003108U) == 0x6DU,
            "the gap between simulated MEM1 size and usable MEM1 start must remain untouched");
    require(context.mmu.read8(0x80003114U) == 0xD6U,
            "the byte after the MEM1 usable-end field must remain untouched");
    require(context.mmu.read8(0x80003157U) == 0x9EU,
            "the byte before GDDR vendor code must remain untouched");
    require(context.mmu.read8(0x8000315CU) == 0xE9U,
            "the byte after GDDR vendor code must remain untouched");
}

}  // namespace

int main() {
    try {
        test_installs_wpse01_boot_contract_into_real_nwii_mmu();
        std::cout << "PASS: installs WPSE01 boot contract into real NWii MMU\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "FAIL: installs WPSE01 boot contract into real NWii MMU: "
                  << exception.what() << '\n';
        return 1;
    }
}
