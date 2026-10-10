#include "runtime/config.h"
#include "runtime/devices.h"
#include "runtime/ios_kernel.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

// This standalone test uses the real ES device, IOS descriptor routing and MMU.
// Unrelated hardware/device entry points are link-only test substitutes.
namespace nwii::runtime {
uint32_t g_watch_addr = 0;
void watch_hit(uint32_t, uint32_t, int) {}
uint32_t HW_Reg_Read32(uint32_t) { return 0; }
void HW_Reg_Write32(uint32_t, uint32_t) {}
void HW_Reg_Write16(uint32_t, uint16_t) {}
void GX_WGPIPE_Write8(uint8_t) {}
void GX_WGPIPE_Write16(uint16_t) {}
void GX_WGPIPE_Write32(uint32_t) {}
namespace devices {
std::unique_ptr<IDevice> create_di_device() { return {}; }
std::unique_ptr<IDevice> create_fs_device() { return {}; }
std::unique_ptr<IDevice> create_stm_device() { return {}; }
std::unique_ptr<IDevice> create_usb_device() { return {}; }
}
}

using namespace nwii::runtime;
namespace fs = std::filesystem;
namespace {
constexpr uint32_t output = 0x80002000;
constexpr int32_t invalid = -1017;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void put32(std::vector<uint8_t>& bytes, size_t at, uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes.at(at + i) = uint8_t(value >> (24 - 8 * i));
}
void save(const fs::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    require(bool(file), "synthetic fixture write failed");
}
struct Fixture {
    fs::path root = fs::current_path() / ("es-content-fixture-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    CPUContext ctx;
    IOSKernel& kernel = IOSKernel::get();
    std::vector<uint8_t> tmd = std::vector<uint8_t>(0x208, 0);
    std::vector<uint8_t> bytes = std::vector<uint8_t>(37);
    uint32_t fd;
    Fixture() {
        fs::create_directories(root / "extracted");
        Config::get().game_dir = (root / "extracted").string();
        Config::get().game_id = "WPSE";
        put32(tmd, 0, 0x00010001); // Synthetic RSA2048 signature envelope.
        put32(tmd, 0x18C, 0x00010001);
        put32(tmd, 0x190, 0x57505345);
        tmd[0x1DF] = 1;
        put32(tmd, 0x1E4, 42); // Content ID deliberately differs from index 5.
        tmd[0x1E9] = 5;
        tmd[0x1EB] = 1;
        put32(tmd, 0x1F0, 37);
        for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = uint8_t(i + 1);
        save(root / "title.tmd", tmd);
        save(root / "0000002a.app", bytes);
        kernel.init();
        kernel.register_device(devices::create_es_device());
        fd = kernel.open(ctx, "/dev/es", 0);
        require(int32_t(fd) >= 0, "ES device must open");
    }
    ~Fixture() {
        kernel.init(); // Close streams before deleting the unique test directory.
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    IpcRequest request(uint32_t cmd, std::initializer_list<uint32_t> args,
                       uint32_t len = 0, uint32_t dest = output) {
        IpcRequest req{};
        req.fd = fd;
        req.ioctl_cmd = cmd;
        req.arg_cnt_in = uint32_t(args.size());
        uint32_t addr = 0x80001000;
        for (auto arg : args) {
            ctx.mmu.write32(addr, arg);
            req.ioctlv_vecs.push_back({addr, 4});
            addr += 32;
        }
        if (cmd == 0x0A) {
            req.arg_cnt_out = 1;
            req.ioctlv_vecs.push_back({dest, len});
        }
        return req;
    }
    int32_t call(uint32_t cmd, std::initializer_list<uint32_t> args,
                 uint32_t len = 0, uint32_t dest = output) {
        return kernel.ioctlv(ctx, request(cmd, args, len, dest));
    }
    uint32_t open() {
        const auto handle = call(9, {5});
        require(handle >= 0, "TMD index 5 must resolve to content ID 42");
        return uint32_t(handle);
    }
};

void header_read_seek_and_eof() {
    Fixture f;
    const auto handle = f.open();
    f.ctx.mmu.write8(output + 37, 0xA5);
    require(f.call(10, {handle}, 32) == 32, "header read must return 32 real bytes, not empty success");
    for (uint32_t i = 0; i < 32; ++i)
        require(f.ctx.mmu.read8(output + i) == i + 1, "header bytes must match original fixture");
    require(f.call(35, {handle, 0, 0}) == 0, "SEEK_SET must return the new offset");
    require(f.call(10, {handle}, 64) == 37, "partial read must stop at logical EOF");
    require(f.ctx.mmu.read8(output + 36) == 37, "partial read must include last content byte");
    require(f.ctx.mmu.read8(output + 37) == 0xA5, "partial read must preserve unused destination tail");
    f.ctx.mmu.write8(output, 0xCC);
    require(f.call(10, {handle}, 32) == 0, "EOF must return zero");
    require(f.ctx.mmu.read8(output) == 0xCC, "EOF must not zero destination");
}

void independent_handles_signed_seeks_and_close() {
    Fixture f;
    const auto a = f.open(), b = f.open();
    require(a != b, "two content opens must allocate independent handles");
    require(f.call(35, {a, 10, 0}) == 10, "absolute seek");
    require(f.call(35, {a, uint32_t(-2), 1}) == 8, "negative relative seek");
    require(f.call(10, {a}, 1) == 1 && f.ctx.mmu.read8(output) == 9, "relative position must select byte9");
    require(f.call(10, {b}, 1) == 1 && f.ctx.mmu.read8(output) == 1, "other handle retains position0");
    require(f.call(35, {a, uint32_t(-1), 2}) == 36, "negative end-relative seek");
    require(f.call(10, {a}, 32) == 1 && f.ctx.mmu.read8(output) == 37, "last byte read");
    require(f.call(35, {a, 1, 2}) == invalid, "seek past logical EOF rejected");
    require(f.call(35, {a, uint32_t(-38), 2}) == invalid, "seek before start rejected");
    require(f.call(35, {a, 0, 3}) == invalid, "invalid whence rejected");
    require(f.call(35, {a, 0, 1}) == 37, "failed seeks preserve cursor");
    require(f.call(11, {a}) == 0, "content close succeeds");
    require(f.call(10, {a}, 1) == invalid, "closed content handle rejected");
    require(f.call(11, {a}) == invalid, "double close rejected");
    require(f.call(10, {b}, 1) == 1 && f.ctx.mmu.read8(output) == 2, "closing one handle preserves another");
}

void invalid_requests_preserve_memory_and_cursor() {
    Fixture f;
    const auto handle = f.open();
    f.ctx.mmu.write8(output, 0xAB);
    auto req = f.request(10, {handle}, 32);
    req.ioctlv_vecs.clear();
    require(f.kernel.ioctlv(f.ctx, req) == invalid, "missing vectors rejected");
    req = f.request(10, {handle}, 32);
    req.ioctlv_vecs[0].len = 3;
    require(f.kernel.ioctlv(f.ctx, req) == invalid, "short scalar rejected");
    req = f.request(9, {5});
    req.arg_cnt_in = 0xFFFFFFFF;
    req.arg_cnt_out = 1;
    require(f.kernel.ioctlv(f.ctx, req) == invalid, "overflowing vector counts rejected");
    req = f.request(9, {5});
    req.ioctlv_vecs[0].addr = 0x817FFFFF;
    require(f.kernel.ioctlv(f.ctx, req) == invalid, "scalar crossing RAM boundary rejected");
    require(f.call(10, {handle}, 32, 0x817FFFF0) == invalid, "MEM1 output crossing boundary rejected");
    require(f.call(10, {handle}, 32, 0x93FFFFF0) == invalid, "MEM2 output crossing boundary rejected");
    require(f.call(10, {handle}, 0xFFFFFFFF) == invalid, "overflowing output range rejected");
    require(f.call(10, {handle}, 32, 0xCC006C00) == invalid, "MMIO output rejected");
    require(f.call(10, {999}, 32) == invalid, "unknown handle rejected");
    require(f.call(10, {handle}, 0) == invalid, "zero-sized read rejected like the guest ES wrapper");
    require(f.call(9, {0x10005}) == invalid, "oversized index cannot alias index5");
    require(f.call(9, {7}) < 0, "unlisted content must not fake success");
    require(f.ctx.mmu.read8(output) == 0xAB, "failed requests must not mutate output");
    require(f.call(35, {handle, 0, 1}) == 0, "failed reads preserve cursor");
    require(f.call(10, {handle}, 4, 0x10002000) == 4, "physical MEM2 IPC address supported");
    require(f.ctx.mmu.read32(0x90002000) == 0x01020304, "physical read populates cached RAM alias");
}

void session_ownership_and_close_cleanup() {
    Fixture f;
    const auto firstFd = f.fd;
    const auto firstHandle = f.open();
    f.fd = f.kernel.open(f.ctx, "/dev/es", 0);
    require(f.call(10, {firstHandle}, 1) == invalid, "another ES session cannot use this handle");
    const auto secondHandle = f.open();
    require(f.kernel.close(f.ctx, firstFd) == 0, "ES session close succeeds");
    require(f.call(10, {secondHandle}, 1) == 1, "closing first session preserves second");
    f.fd = f.kernel.open(f.ctx, "/dev/es", 0);
    require(f.call(10, {firstHandle}, 1) == invalid, "reused IOS descriptor must not revive old content handles");
}

void invalid_metadata_and_files_fail() {
    Fixture f;
    f.tmd[0x190] ^= 1;
    save(f.root / "title.tmd", f.tmd);
    require(f.call(9, {5}) < 0, "wrong-title TMD rejected");
    f.tmd[0x190] ^= 1;
    f.tmd.resize(0x200);
    save(f.root / "title.tmd", f.tmd);
    require(f.call(9, {5}) < 0, "truncated TMD record rejected");
    f.tmd.resize(0x208);
    save(f.root / "title.tmd", f.tmd);
    save(f.root / "0000002a.app", {1, 2});
    require(f.call(9, {5}) < 0, "truncated content rejected");
    fs::rename(f.root / "0000002a.app", f.root / "unavailable.bin");
    require(f.call(9, {5}) < 0, "missing content rejected");
    // Extraction padding is not readable past the TMD logical size.
    f.bytes.resize(64, 0xEE);
    save(f.root / "0000002a.app", f.bytes);
    fs::rename(f.root / "title.tmd", f.root / "0001000157505345.tmd");
    const auto h = f.open();
    require(f.call(10, {h}, 64) == 37, "title-named TMD works and padding stays outside EOF");
}
}

int main() {
    try {
        header_read_seek_and_eof();
        independent_handles_signed_seeks_and_close();
        invalid_requests_preserve_memory_and_cursor();
        session_ownership_and_close_cleanup();
        invalid_metadata_and_files_fail();
        std::cout << "PASS: ES reads local synthetic content through IOS and the real MMU\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
