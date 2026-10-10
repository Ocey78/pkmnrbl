#include "runtime/cpu_context.h"
#include "runtime/hw/hw.h"
#include "runtime/ios_device.h"

#include <atomic>
#include <array>
#include <cstdint>
#include <iostream>

namespace nwii::runtime {
CPUContext* g_ctx_ptr = nullptr;
int g_ipc_interrupt_delay = 0;
uint32_t g_watch_addr = 0;
void watch_hit(uint32_t, uint32_t, int) {}
uint32_t HW_Reg_Read32(uint32_t addr) {
  return MMIODispatcher::get().read32(addr);
}
void HW_Reg_Write32(uint32_t addr, uint32_t value) {
  MMIODispatcher::get().write32(addr, value);
}
void GX_WGPIPE_Write32(uint32_t) {}
}

namespace {
bool complete_synchronously = false;
uint32_t observed_command = 0;
uint32_t observed_fd = 0;
int32_t device_result = 0;
}

namespace nwii::runtime::hw {
std::atomic<uint32_t> pi_intsr = 0;

// Substitute only the device boundary: the real MMIO, queue and MMU handle
// submission and reply publication, with complete synthetic request buffers.
extern "C" int32_t handle_ios_ipc(CPUContext& ctx, uint32_t request) {
  constexpr uint32_t first_request = 0x13E00020;
  constexpr uint32_t second_request = 0x13E00080;
  observed_command = ctx.mmu.read32(request);
  observed_fd = ctx.mmu.read32(request + 8);
  if (complete_synchronously) {
    ctx.mmu.write32(request + 4, uint32_t(device_result));
    return device_result;
  }
  if (request == second_request) {
    ctx.mmu.write32(first_request + 4, 6);
    ipc_post_reply(first_request);
    ctx.mmu.write32(request + 4, IPC_OK);
    return IPC_OK;
  }
  return IPC_NO_REPLY;
}
}

namespace {
constexpr uint32_t kPpcMessage = 0xCD000000;
constexpr uint32_t kPpcControl = 0xCD000004;
constexpr uint32_t kArmMessage = 0xCD000008;
constexpr uint32_t kFirstRequest = 0x13E00020;
constexpr uint32_t kSecondRequest = 0x13E00080;

constexpr uint32_t kExecute = 0x01;
constexpr uint32_t kAcknowledge = 0x02;
constexpr uint32_t kReply = 0x04;
constexpr uint32_t kReload = 0x08;
constexpr uint32_t kReplyInterruptEnable = 0x10;
constexpr uint32_t kAcknowledgeInterruptEnable = 0x20;
constexpr uint32_t kInterruptEnables =
    kReplyInterruptEnable | kAcknowledgeInterruptEnable;
constexpr uint32_t kPendingResult = 0xAABBCCDD;
// Five argument words, callback, userdata, relaunch, sync queue, magic and pad.
constexpr std::array<uint32_t, 13> kRequestTail = {
    0x13E08000, 0x00000040, 0x10203040, 0x13E09000, 0x00000080,
    0x802F13B4, 0x81502348, 0x11223344, 0x81506780, 0xC0DEC0DE,
    0x55667788, 0x99AABBCC, 0xDDEEFF00};

bool expect_equal(uint32_t actual, uint32_t expected, const char* label) {
  if (actual == expected)
    return true;

  std::cerr << label << ": expected 0x" << std::hex << expected << ", got 0x"
            << actual << std::dec << '\n';
  return false;
}

void initialize_request(nwii::runtime::CPUContext& ctx, uint32_t request,
                        uint32_t command, uint32_t fd) {
  ctx.mmu.write32(request, command);
  ctx.mmu.write32(request + 4, kPendingResult);
  ctx.mmu.write32(request + 8, fd);
  for (uint32_t i = 0; i < kRequestTail.size(); ++i)
    ctx.mmu.write32(request + 12 + i * 4, kRequestTail[i]);
}

bool expect_request_tail(nwii::runtime::CPUContext& ctx, uint32_t request) {
  bool ok = true;
  for (uint32_t i = 0; i < kRequestTail.size(); ++i)
    ok &= expect_equal(ctx.mmu.read32(request + 12 + i * 4), kRequestTail[i],
                       "reply preserves arguments and caller metadata");
  return ok;
}

bool expect_reply(nwii::runtime::CPUContext& ctx, uint32_t request,
                  uint32_t command, uint32_t result) {
  bool ok = expect_equal(ctx.mmu.read32(request), 8, "reply command");
  ok &= expect_equal(ctx.mmu.read32(request + 8), command,
                     "reply original command replaces descriptor");
  ok &= expect_equal(ctx.mmu.read32(request + 4), result,
                     "reply preserves device result");
  ok &= expect_request_tail(ctx, request);
  return ok;
}

bool immediate_replies_preserve_original_commands(nwii::runtime::CPUContext& ctx) {
  struct Case { uint32_t command, fd, address; int32_t result; };
  // Every command has a different descriptor. WRITE/fd7 is the native title
  // regression: interpreting fd7 as the reply command incorrectly selects IOCTLV.
  constexpr std::array<Case, 7> cases = {{
      {1, 17, 0x00004000, 9}, {2, 18, 0x80004000, 0},
      {3, 19, 0xC0004000, 32}, {4, 7, 0x13E00200, 64},
      {5, 21, 0x93E00200, 128}, {6, 22, 0xD3E00200, -4},
      {7, 3, 0x13E00200, -106}}};
  auto& dispatcher = nwii::runtime::MMIODispatcher::get();
  bool ok = true;
  complete_synchronously = true;
  for (const auto& test : cases) {
    initialize_request(ctx, test.address, test.command, test.fd);
    device_result = test.result;
    dispatcher.write32(kPpcMessage, test.address);
    dispatcher.write32(kPpcControl, kExecute | kInterruptEnables);
    ok &= expect_equal(observed_command, test.command,
                       "device consumes original command before reply conversion");
    ok &= expect_equal(observed_fd, test.fd,
                       "device consumes original descriptor before reply conversion");
    dispatcher.write32(kPpcControl, kAcknowledge | kInterruptEnables);
    ok &= expect_equal(dispatcher.read32(kArmMessage), test.address & 0x1FFFFFFF,
                       "immediate reply physical address");
    ok &= expect_equal(dispatcher.read32(kPpcControl), kReply | kInterruptEnables,
                       "immediate reply control bits");
    ok &= expect_reply(ctx, test.address, test.command, uint32_t(test.result));
    dispatcher.write32(kPpcControl, kReply | kInterruptEnables);
  }
  complete_synchronously = false;
  return ok;
}
}

int main() {
  using namespace nwii::runtime;
  using namespace nwii::runtime::hw;

  CPUContext context;
  g_ctx_ptr = &context;
  g_ipc_interrupt_delay = 0;

  auto& dispatcher = MMIODispatcher::get();
  dispatcher.clear();
  register_ipc(dispatcher);

  bool ok = immediate_replies_preserve_original_commands(context);
  initialize_request(context, kFirstRequest, 7, 3);
  initialize_request(context, kSecondRequest, 4, 7);

  dispatcher.write32(kPpcMessage, kFirstRequest);
  dispatcher.write32(kPpcControl,
                     kExecute | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable);

  const uint32_t pending_control = dispatcher.read32(kPpcControl);
  ok &= expect_equal(context.mmu.read32(kFirstRequest), 7,
                     "pending request keeps command");
  ok &= expect_equal(context.mmu.read32(kFirstRequest + 8), 3,
                     "pending request keeps descriptor");
  ok &= expect_equal(context.mmu.read32(kFirstRequest + 4), kPendingResult,
                     "pending request keeps result until completion");
  ok &= expect_request_tail(context, kFirstRequest);
  ok &= expect_equal(pending_control & kExecute, 0,
                     "pending request execute bit");
  ok &= expect_equal(pending_control & kAcknowledge, kAcknowledge,
                     "pending request acknowledge bit");
  ok &= expect_equal(pending_control & kReply, 0,
                     "pending request reply bit");
  ok &= expect_equal(pending_control & kReload, 0,
                     "pending request reload bit");
  ok &= expect_equal(pending_control &
                         (kReplyInterruptEnable | kAcknowledgeInterruptEnable),
                     kReplyInterruptEnable | kAcknowledgeInterruptEnable,
                     "pending request interrupt enables");

  // Broadway acknowledges Y2 while retaining both interrupt-enable bits.
  dispatcher.write32(kPpcControl,
                     kAcknowledge | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable);
  ok &= expect_equal(dispatcher.read32(kPpcControl),
                     kReplyInterruptEnable | kAcknowledgeInterruptEnable,
                     "acknowledge clear");

  // A second request completes the older asynchronous request while its own
  // handler is still active.  The older reply must not be overwritten by the
  // second request's acknowledgement or synchronous reply.
  dispatcher.write32(kPpcMessage, kSecondRequest);
  dispatcher.write32(kPpcControl,
                     kExecute | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable);
  ok &= expect_equal(dispatcher.read32(kPpcControl),
                     kAcknowledge | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable,
                     "nested completion acknowledge phase");

  dispatcher.write32(kPpcControl,
                     kAcknowledge | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable);
  ok &= expect_equal(dispatcher.read32(kArmMessage),
                     kFirstRequest & 0x1FFFFFFF,
                     "first queued reply address");
  ok &= expect_equal(dispatcher.read32(kPpcControl),
                     kReply | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable,
                     "first queued reply control bits");
  ok &= expect_reply(context, kFirstRequest, 7, 6);

  dispatcher.write32(kPpcControl,
                     kReply | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable);
  ok &= expect_equal(dispatcher.read32(kArmMessage),
                     kSecondRequest & 0x1FFFFFFF,
                     "second queued reply address");
  ok &= expect_equal(dispatcher.read32(kPpcControl),
                     kReply | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable,
                     "second queued reply control bits");
  ok &= expect_reply(context, kSecondRequest, 4, IPC_OK);
  ok &= expect_reply(context, kFirstRequest, 7, 6);

  dispatcher.write32(kPpcControl,
                     kReply | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable);
  ok &= expect_equal(dispatcher.read32(kPpcControl),
                     kReplyInterruptEnable | kAcknowledgeInterruptEnable,
                     "reply queue drained");

  // A new submission must not consume a reply that Broadway has not read.
  ipc_post_reply(kFirstRequest);
  ok &= expect_reply(context, kFirstRequest, 7, 6);
  initialize_request(context, 0x13E00100, 3, 9);
  dispatcher.write32(kPpcMessage, 0x13E00100);
  dispatcher.write32(kPpcControl,
                     kExecute | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable);
  ok &= expect_equal(dispatcher.read32(kPpcControl),
                     kReply | kAcknowledge | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable,
                     "new request preserves outstanding reply");
  ok &= expect_equal(dispatcher.read32(kArmMessage), kFirstRequest,
                     "outstanding reply address remains visible");
  dispatcher.write32(kPpcControl,
                     kAcknowledge | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable);
  ok &= expect_equal(dispatcher.read32(kPpcControl),
                     kReply | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable,
                     "acknowledging new request preserves old reply");
  dispatcher.write32(kPpcControl,
                     kReply | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable);

  dispatcher.clear();
  g_ctx_ptr = nullptr;
  return ok ? 0 : 1;
}
