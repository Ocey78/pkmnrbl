#pragma once
#include <cstdint>

namespace nwii::runtime {
struct CPUContext;

// Presentation supplies a wake hint; guest timers use the guest timebase.
bool consume_host_vblank_hint(CPUContext& ctx);
// Called at runtime startup/reset, after establishing the guest clock policy.
void initialize_guest_timers(CPUContext& ctx);
void service_guest_timers(CPUContext& ctx);
}
