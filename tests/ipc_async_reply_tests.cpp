#include "runtime/cpu_context.h"
#include "runtime/hw/hw.h"
#include "runtime/ios_device.h"

#include <atomic>
#include <cstdint>
#include <iostream>

namespace nwii::runtime {
CPUContext* g_ctx_ptr = nullptr;
int g_ipc_interrupt_delay = 0;
}

namespace nwii::runtime::hw {
std::atomic<uint32_t> pi_intsr = 0;

extern "C" int32_t handle_ios_ipc(CPUContext&, uint32_t request) {
  constexpr uint32_t first_request = 0x13E00020;
  constexpr uint32_t second_request = 0x13E00080;
  if (request == second_request) {
    ipc_post_reply(first_request);
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

bool expect_equal(uint32_t actual, uint32_t expected, const char* label) {
  if (actual == expected)
    return true;

  std::cerr << label << ": expected 0x" << std::hex << expected << ", got 0x"
            << actual << std::dec << '\n';
  return false;
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

  dispatcher.write32(kPpcMessage, kFirstRequest);
  dispatcher.write32(kPpcControl,
                     kExecute | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable);

  const uint32_t pending_control = dispatcher.read32(kPpcControl);
  bool ok = true;
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

  dispatcher.write32(kPpcControl,
                     kReply | kReplyInterruptEnable |
                         kAcknowledgeInterruptEnable);
  ok &= expect_equal(dispatcher.read32(kPpcControl),
                     kReplyInterruptEnable | kAcknowledgeInterruptEnable,
                     "reply queue drained");

  dispatcher.clear();
  g_ctx_ptr = nullptr;
  return ok ? 0 : 1;
}
