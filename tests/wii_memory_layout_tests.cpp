#include "runtime/boot/guest_memory.h"
#include "runtime/boot/wii_memory_layout.h"

#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

class InMemoryGuestMemory final : public GuestMemory {
public:
    void write32_be(std::uint32_t address, std::uint32_t value) override {
        if (address < kBaseAddress || address + 4 > kBaseAddress + bytes_.size()) {
            throw std::out_of_range("low-memory write is outside the test mapping");
        }

        const auto offset = address - kBaseAddress;
        bytes_[offset] = static_cast<std::uint8_t>(value >> 24U);
        bytes_[offset + 1] = static_cast<std::uint8_t>(value >> 16U);
        bytes_[offset + 2] = static_cast<std::uint8_t>(value >> 8U);
        bytes_[offset + 3] = static_cast<std::uint8_t>(value);
        ++write_count_;
    }

    [[nodiscard]] std::uint8_t byte_at(std::uint32_t address) const {
        if (address < kBaseAddress || address >= kBaseAddress + bytes_.size()) {
            throw std::out_of_range("low-memory read is outside the test mapping");
        }
        return bytes_[address - kBaseAddress];
    }

    [[nodiscard]] std::size_t write_count() const { return write_count_; }

private:
    static constexpr std::uint32_t kBaseAddress = 0x80000000U;
    std::array<std::uint8_t, 0x4000> bytes_{};
    std::size_t write_count_ = 0;
};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_word_bytes(const InMemoryGuestMemory& memory, std::uint32_t address,
                        std::uint8_t byte0, std::uint8_t byte1,
                        std::uint8_t byte2, std::uint8_t byte3) {
    require(memory.byte_at(address) == byte0, "first byte differs");
    require(memory.byte_at(address + 1) == byte1, "second byte differs");
    require(memory.byte_at(address + 2) == byte2, "third byte differs");
    require(memory.byte_at(address + 3) == byte3, "fourth byte differs");
}

WiiMemoryLayout valid_layout() {
    return make_wpse01_layout(0x81000000U, 0x81700000U);
}

void test_installs_every_wpse01_low_memory_word_as_literal_big_endian_bytes() {
    InMemoryGuestMemory memory;
    std::string error;

    require(install_wii_memory_layout(memory, valid_layout(), error),
            "the baseline layout must install");
    require(error.empty(), "successful installation must not report an error");

    require_word_bytes(memory, 0x80000020U, 0x0D, 0x15, 0xEA, 0x5E);
    require_word_bytes(memory, 0x80000024U, 0x00, 0x00, 0x00, 0x01);
    require_word_bytes(memory, 0x80000028U, 0x01, 0x80, 0x00, 0x00);
    require_word_bytes(memory, 0x8000002CU, 0x00, 0x00, 0x00, 0x21);
    require_word_bytes(memory, 0x80000030U, 0x81, 0x00, 0x00, 0x00);
    require_word_bytes(memory, 0x80000034U, 0x81, 0x70, 0x00, 0x00);
    require_word_bytes(memory, 0x800000F0U, 0x01, 0x80, 0x00, 0x00);
    require_word_bytes(memory, 0x800000F8U, 0x0E, 0x7B, 0xE2, 0xC0);
    require_word_bytes(memory, 0x800000FCU, 0x2B, 0x73, 0xA8, 0x40);
    require_word_bytes(memory, 0x80003100U, 0x01, 0x80, 0x00, 0x00);
    require_word_bytes(memory, 0x80003104U, 0x01, 0x80, 0x00, 0x00);
    require_word_bytes(memory, 0x8000310CU, 0x81, 0x00, 0x00, 0x00);
    require_word_bytes(memory, 0x80003110U, 0x81, 0x70, 0x00, 0x00);
    require_word_bytes(memory, 0x80003118U, 0x04, 0x00, 0x00, 0x00);
    require_word_bytes(memory, 0x8000311CU, 0x04, 0x00, 0x00, 0x00);
    require_word_bytes(memory, 0x80003120U, 0x94, 0x00, 0x00, 0x00);
    require_word_bytes(memory, 0x80003124U, 0x90, 0x00, 0x08, 0x00);
    require_word_bytes(memory, 0x80003128U, 0x93, 0xE0, 0x00, 0x00);
    require_word_bytes(memory, 0x80003130U, 0x93, 0xE0, 0x00, 0x00);
    require_word_bytes(memory, 0x80003134U, 0x94, 0x00, 0x00, 0x00);
    require_word_bytes(memory, 0x80003138U, 0x00, 0x00, 0x00, 0x11);
    require_word_bytes(memory, 0x80003158U, 0x00, 0x00, 0x00, 0x23);
}

void test_rejects_unaligned_mem1_bounds() {
    auto unaligned_start = valid_layout();
    unaligned_start.mem1_usable_start = 0x81000004U;
    require(!validate(unaligned_start).valid, "unaligned MEM1 start must be invalid");

    auto unaligned_end = valid_layout();
    unaligned_end.mem1_usable_end = 0x81700004U;
    require(!validate(unaligned_end).valid, "unaligned MEM1 end must be invalid");
}

void test_rejects_fully_validated_late_error_without_writing() {
    auto layout = valid_layout();
    layout.ipc_buffer_start = 0x93FFE240U;
    layout.ipc_buffer_end = 0x94000000U;
    InMemoryGuestMemory memory;
    std::string error;

    require(!validate(layout).valid, "undersized IPC must be invalid");
    require(!install_wii_memory_layout(memory, layout, error), "invalid layout must not install");
    require(!error.empty(), "failed installation must explain the error");
    require(memory.write_count() == 0,
            "an error discovered after range checks must still prevent every write");
}

void test_rejects_reversed_mem1_mem2_and_ipc_ranges() {
    auto reversed_mem1 = valid_layout();
    reversed_mem1.mem1_usable_start = 0x81700000U;
    reversed_mem1.mem1_usable_end = 0x81000000U;
    require(!validate(reversed_mem1).valid, "reversed MEM1 range must be invalid");

    auto reversed_mem2 = valid_layout();
    reversed_mem2.mem2_usable_start = 0x93E00000U;
    reversed_mem2.mem2_usable_end = 0x90000800U;
    require(!validate(reversed_mem2).valid, "reversed MEM2 range must be invalid");

    auto reversed_ipc = valid_layout();
    reversed_ipc.ipc_buffer_start = 0x94000000U;
    reversed_ipc.ipc_buffer_end = 0x93E00000U;
    require(!validate(reversed_ipc).valid, "reversed IPC range must be invalid");
}

void test_rejects_mem2_ipc_overlap() {
    auto layout = valid_layout();
    layout.mem2_usable_end = 0x93E00020U;

    require(!validate(layout).valid, "MEM2 usable range must not overlap IPC");
}

void test_rejects_ipc_outside_mapped_mem2() {
    auto layout = valid_layout();
    layout.ipc_buffer_start = 0x8FFFFFE0U;

    require(!validate(layout).valid, "IPC must be contained by mapped MEM2");
}

void test_rejects_ipc_smaller_than_sdk_boot_allocation_sequence() {
    auto layout = valid_layout();
    layout.ipc_buffer_start = 0x93FFE240U;
    layout.ipc_buffer_end = 0x94000000U;

    require(!validate(layout).valid, "IPC needs at least 0x1DE0 bytes for SDK boot allocations");
}

void run_test(const char* name, void (*test)(), int& failures) {
    try {
        test();
        std::cout << "PASS: " << name << '\n';
    }
    catch (const std::exception& exception) {
        ++failures;
        std::cerr << "FAIL: " << name << ": " << exception.what() << '\n';
    }
}

}  // namespace

int main() {
    int failures = 0;
    run_test("installs every WPSE01 low-memory word as literal big-endian bytes",
             test_installs_every_wpse01_low_memory_word_as_literal_big_endian_bytes, failures);
    run_test("rejects unaligned MEM1 bounds", test_rejects_unaligned_mem1_bounds, failures);
    run_test("rejects fully validated late error without writing",
             test_rejects_fully_validated_late_error_without_writing, failures);
    run_test("rejects reversed MEM1 MEM2 and IPC ranges", test_rejects_reversed_mem1_mem2_and_ipc_ranges,
             failures);
    run_test("rejects MEM2 IPC overlap", test_rejects_mem2_ipc_overlap, failures);
    run_test("rejects IPC outside mapped MEM2", test_rejects_ipc_outside_mapped_mem2, failures);
    run_test("rejects IPC smaller than SDK boot allocation sequence",
             test_rejects_ipc_smaller_than_sdk_boot_allocation_sequence, failures);
    return failures == 0 ? 0 : 1;
}
