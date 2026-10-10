#include "input/input_manager.h"
#include "runtime/cpu_context.h"

#include <iostream>
#include <stdexcept>
#include <atomic>
#include <thread>

extern "C" void KPADRead(nwii::runtime::CPUContext&);
extern "C" void KPADInit(nwii::runtime::CPUContext&);
extern "C" void WPADProbe(nwii::runtime::CPUContext&);

namespace nwii::runtime {
uint32_t g_watch_addr = 0;
void watch_hit(uint32_t, uint32_t, int) {}
uint32_t HW_Reg_Read32(uint32_t) { return 0; }
void HW_Reg_Write32(uint32_t, uint32_t) {}
void HW_Reg_Write16(uint32_t, uint16_t) {}
void GX_WGPIPE_Write8(uint8_t) {}
void GX_WGPIPE_Write16(uint16_t) {}
void GX_WGPIPE_Write32(uint32_t) {}
}

using namespace nwii::runtime;
using namespace nwii::runtime::input;
namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
class ControlledSource final : public IInputSource {
public:
    WiimoteState value{0x0800, 0.5f, 0.5f, 0, 0, 1, 0, 0, 0, 0};
    void update(GameCubePadState[4], WiimoteState motes[4]) override { motes[0] = value; }
};
void basic_remote_populates_complete_guest_record() {
    CPUContext ctx;
    KPADInit(ctx);
    auto source = std::make_unique<ControlledSource>();
    auto* control = source.get();
    InputManager::get().add_source(std::move(source));
    InputManager::get().update();
    constexpr uint32_t dest = 0x80002000;
    for (unsigned i = 0; i < 0xB1; ++i) ctx.mmu.write8(dest + i, 0xA5);
    ctx.gpr[3] = 0; ctx.gpr[4] = dest; ctx.gpr[5] = 1;
    KPADRead(ctx);
    require(ctx.gpr[3] == 1, "connected native source must produce a sample");
    require(ctx.mmu.read8(dest + 0x5D) == 0, "complete KPAD record must report connected status at byte5D");
    require(ctx.mmu.read8(dest + 0x5C) == 0, "basic remote device type at byte5C");
    require(ctx.mmu.read32(dest) == 0x0800 && ctx.mmu.read32(dest + 4) == 0x0800,
            "first A press must set hold and trigger");
    require(ctx.mmu.read32(dest + 8) == 0, "first A press must not set release");
    require(ctx.mmu.read8(dest + 0xB0) == 0xA5, "single sample must not overwrite destination tail");
    for (unsigned i = 0x60; i < 0xB0; ++i)
        require(ctx.mmu.read8(dest + i) == 0, "unused extension fields must be initialized");

    const auto read = [&](int channel, uint32_t ptr, int capacity) {
        ctx.gpr[3] = uint32_t(channel); ctx.gpr[4] = ptr; ctx.gpr[5] = uint32_t(capacity);
        KPADRead(ctx); return ctx.gpr[3];
    };
    require(read(0, dest, 2) == 1 && ctx.mmu.read32(dest + 4) == 0,
            "held button must not retrigger and only one available sample is returned");
    control->value.buttons = 0;
    InputManager::get().update();
    require(read(0, dest, 1) == 1 && ctx.mmu.read32(dest) == 0 &&
            ctx.mmu.read32(dest + 8) == 0x0800, "button release must be reported once");
    require(read(0, dest, 1) == 1 && ctx.mmu.read32(dest + 8) == 0,
            "release must not repeat");
    control->value.buttons = 0x0100;
    InputManager::get().update();
    ctx.mmu.write32(dest, 0xAABBCCDD);
    for (uint32_t ptr : {0u, 0x817FFF80u, 0x93FFFF80u, 0xCC006C00u, 0xFFFFFFF0u})
        require(read(0, ptr, 1) == 0, "invalid sample range must fail safely");
    require(read(0, dest, 0) == 0 && read(0, dest, -1) == 0 &&
            read(-1, dest, 1) == 0 && read(4, dest, 1) == 0,
            "invalid capacity/channel returns zero samples");
    require(ctx.mmu.read32(dest) == 0xAABBCCDD, "failed reads preserve destination");
    require(read(0, 0x10002000, 1) == 1 && ctx.mmu.read32(0x90002004) == 0x0100,
            "failed reads preserve edge state; physical MEM2 output is supported");
    control->value.err = -1;
    InputManager::get().update();
    ctx.mmu.write32(dest, 0xAABBCCDD);
    require(read(0, dest, 1) == 0 && ctx.mmu.read32(dest) == 0xAABBCCDD,
            "disconnected source returns no sample and preserves memory");
    control->value.err = 0;
    InputManager::get().update();
    require(read(0, dest, 1) == 1 && ctx.mmu.read32(dest + 4) == 0x0100,
            "reconnected held button starts a new edge sequence");

    ctx.gpr[3] = 0; ctx.gpr[4] = dest; WPADProbe(ctx);
    require(ctx.gpr[3] == 0 && ctx.mmu.read32(dest) == 0, "probe exposes connected basic native remote");
    ctx.gpr[3] = 1; ctx.gpr[4] = dest; WPADProbe(ctx);
    require(int32_t(ctx.gpr[3]) == -1 && ctx.mmu.read32(dest) == 0xFF,
            "unavailable channel reports disconnected and no-device type");
    require(read(1, dest, 1) == 0 && read(0, dest, 1) == 1 && ctx.mmu.read32(dest + 4) == 0,
            "another channel cannot reset channel0 edge state");
    ctx.gpr[3] = 0; ctx.gpr[4] = 0; WPADProbe(ctx);
    require(ctx.gpr[3] == 0, "probe permits omitted type output");
    control->value.err = -1;
    InputManager::get().update();
    ctx.gpr[3] = 0; ctx.gpr[4] = 0; WPADProbe(ctx);
    require(int32_t(ctx.gpr[3]) == -1, "probe observes disconnection without a read");
    control->value.err = 0;
    InputManager::get().update();
    require(read(0, dest, 1) == 1 && ctx.mmu.read32(dest + 4) == 0x0100,
            "probe-gated reconnect must start a fresh button edge sequence");
    ctx.mmu.write8(0x817FFFFF, 0xA5);
    ctx.gpr[3] = 0; ctx.gpr[4] = 0x817FFFFF; WPADProbe(ctx);
    require(int32_t(ctx.gpr[3]) == -1 && ctx.mmu.read8(0x817FFFFF) == 0xA5,
            "probe validates the entire output word");

    // Only the publisher modifies the source. A reader must see one completed
    // InputManager snapshot, never the reset/disconnected middle of update().
    control->value.buttons = 0; control->value.accel_x = 0;
    InputManager::get().update();
    std::atomic<bool> done{false}, coherent{true};
    std::thread reader([&] {
        while (!done.load()) {
            const auto state = InputManager::get().get_wiimote_state(0);
            if (state.err != 0 || float(state.buttons) != state.accel_x) coherent = false;
        }
    });
    for (unsigned i = 1; i < 10000; ++i) {
        control->value.buttons = i; control->value.accel_x = float(i);
        InputManager::get().update();
    }
    done = true; reader.join();
    require(coherent.load(), "concurrent publication must expose coherent connected snapshots");
}
}
int main() {
    try {
        basic_remote_populates_complete_guest_record();
        std::cout << "PASS: native Wii controller SDK boundary\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
