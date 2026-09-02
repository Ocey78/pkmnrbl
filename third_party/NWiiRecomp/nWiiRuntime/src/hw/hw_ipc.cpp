#include "runtime/hw/hw.h"
#include "runtime/config.h"
#include "runtime/cpu_context.h"
#include "runtime/ios_device.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <deque>

namespace nwii::runtime {
    extern CPUContext* g_ctx_ptr;
    extern MMU* g_mmu;
    extern int g_ipc_interrupt_delay;
}

namespace nwii::runtime::hw {

static uint32_t ipc_arm_msg = 0;
static uint32_t ipc_arm_ctrl = 0;
static uint32_t ipc_ppc_ctrl = 0;
static std::deque<uint32_t> ipc_reply_queue;
uint32_t ipc_ppc_msg = 0;

namespace {
constexpr uint32_t IPC_CTRL_X1 = 0x01;
constexpr uint32_t IPC_CTRL_Y2 = 0x02;
constexpr uint32_t IPC_CTRL_Y1 = 0x04;
constexpr uint32_t IPC_CTRL_X2 = 0x08;

void post_next_reply_if_ready() {
  if (ipc_reply_queue.empty() ||
      (ipc_ppc_ctrl & (IPC_CTRL_X1 | IPC_CTRL_Y2 | IPC_CTRL_Y1 | IPC_CTRL_X2))) {
    return;
  }

  ipc_arm_msg = ipc_reply_queue.front() & 0x1FFFFFFF;
  ipc_reply_queue.pop_front();
  ipc_ppc_ctrl |= IPC_CTRL_Y1;
  g_ipc_interrupt_delay = 50;
}
}

extern "C" int32_t handle_ios_ipc(nwii::runtime::CPUContext& ctx, uint32_t request_addr);

int32_t dispatch_ipc(CPUContext& ctx, uint32_t virt_addr) {
  return handle_ios_ipc(ctx, virt_addr);
}

int32_t ipc_dispatch_request(CPUContext &ctx, uint32_t req_addr) {
  uint32_t virt_addr = req_addr;
  if ((virt_addr & 0xF0000000) == 0xC0000000) virt_addr = (virt_addr & 0x0FFFFFFF) | 0x80000000;
  if ((virt_addr & 0xF0000000) == 0xD0000000) virt_addr = (virt_addr & 0x0FFFFFFF) | 0x90000000;

  return dispatch_ipc(ctx, virt_addr);
}

void hle_set_ipc_arm_msg(uint32_t req_addr) {
  ipc_arm_msg = req_addr & 0x1FFFFFFF;
  ipc_arm_ctrl = 0x00000002;
  ipc_ppc_ctrl |= 0x00000004;
  g_ipc_interrupt_delay = 5000;
}



void ipc_post_reply(uint32_t req_addr) {
  ipc_reply_queue.push_back(req_addr);
  post_next_reply_if_ready();
}

void register_ipc(MMIODispatcher& dispatcher) {{
    dispatcher.register_region(0xCD000000, 0xCD00FFFF, 
        [](uint32_t addr) -> uint32_t {
            switch (addr & 0x00FFFFFF) {
            case 0x000000: return ipc_ppc_msg;
            case 0x000004: return ipc_ppc_ctrl;
            case 0x000008: return ipc_arm_msg;
            case 0x00000C: return ipc_arm_ctrl;
            default: return 0;
            }
        },
        [](uint32_t addr, uint32_t val) {
            switch (addr & 0x00FFFFFF) {
            case 0x000000: {
                ipc_ppc_msg = val;
                break;
            }
            case 0x000004: {
                ipc_ppc_ctrl = (ipc_ppc_ctrl & ~0x30) | (val & 0x30);

                if (val & IPC_CTRL_Y1) ipc_ppc_ctrl &= ~IPC_CTRL_Y1;
                if (val & IPC_CTRL_Y2) ipc_ppc_ctrl &= ~IPC_CTRL_Y2;
                if (val & IPC_CTRL_X2) ipc_ppc_ctrl &= ~IPC_CTRL_X2;

                if (val & IPC_CTRL_X1) {
                    
                    ipc_ppc_ctrl |= IPC_CTRL_X1;
                    int32_t result = 0;
                    if (g_ctx_ptr) {
                        
                        result = ipc_dispatch_request(*g_ctx_ptr, ipc_ppc_msg);
                    }
                    std::cout << "[IPC] Request sent from PPC! result=" << result << "\n";
                    // IOS acknowledges every accepted request first.  A
                    // synchronous result joins the same ordered reply queue
                    // used by devices that finish asynchronously.
                    ipc_ppc_ctrl &= ~(IPC_CTRL_X1 | IPC_CTRL_Y1 | IPC_CTRL_X2);
                    ipc_ppc_ctrl |= IPC_CTRL_Y2;
                    if (result != IPC_NO_REPLY)
                        ipc_reply_queue.push_back(ipc_ppc_msg);
                    g_ipc_interrupt_delay = 50;
                }

                post_next_reply_if_ready();
                break;
            }

            case 0x00000C: {
                ipc_arm_ctrl &= ~(val & 0x03);
                if (!(ipc_arm_ctrl & 3)) {
                    if (std::getenv("NWII_SAMPLE") && (pi_intsr & 0x4000)) {
                        static int n = 0;
                        if (n++ < 6)
                            std::cout << "[IPC] armctrl write 0x" << std::hex
                                      << val << " swallows pending irq\n" << std::dec;
                    }
                    clear_pi_interrupt(0x00004000);
                }
                break;
            }
            } 
        }
    );
    dispatcher.register_region(0x0D000000, 0x0D00FFFF, 
        [&dispatcher](uint32_t a) { return dispatcher.read32(a | 0xC0000000); },
        [&dispatcher](uint32_t a, uint32_t v) { dispatcher.write32(a | 0xC0000000, v); }
    );
}}

} 
