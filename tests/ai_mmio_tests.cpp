#include "runtime/hw/hw.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace nwii::runtime {

static uint64_t g_fake_time_us = 0;

uint64_t get_os_time() {
    return g_fake_time_us;
}

void set_fake_time(uint64_t value) {
    g_fake_time_us = value;
}

} // namespace nwii::runtime

namespace nwii::runtime::hw {

std::atomic<uint32_t> pi_intsr{0};
std::atomic<uint32_t> pi_intmr{0};
std::atomic<uint32_t> g_pe_sr{0};
int g_di_interrupt_delay = 0;

} // namespace nwii::runtime::hw

static void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

static void verify_counter_advances(uint32_t control_addr,
                                    uint32_t counter_addr,
                                    const char* message) {
    using namespace nwii::runtime;
    using namespace nwii::runtime::hw;

    auto& dispatcher = MMIODispatcher::get();
    dispatcher.clear();

    set_fake_time(1000);
    register_ai(dispatcher);

    // Pokemon Rumble does exactly this: set PSTAT (bit 0), then wait for
    // two reads of AISCNT to differ.
    dispatcher.write32(control_addr, 0x01);
    const uint32_t first = dispatcher.read32(counter_addr);

    set_fake_time(2000);
    const uint32_t second = dispatcher.read32(counter_addr);

    require(second > first, message);
}

int main() {
    verify_counter_advances(
        0xCC006C00u, 0xCC006C08u,
        "PSTAT bit 0 must advance AISCNT on the legacy CC mirror");

    verify_counter_advances(
        0xCD006C00u, 0xCD006C08u,
        "Wii Broadway CD006Cxx AI registers must advance AISCNT");

    verify_counter_advances(
        0x0D006C00u, 0x0D006C08u,
        "physical 0D006Cxx AI registers must advance AISCNT after MMU masking");

    std::cout
        << "PASS: AI PSTAT and Wii MMIO mirrors advance the sample counter\n";
    return 0;
}
