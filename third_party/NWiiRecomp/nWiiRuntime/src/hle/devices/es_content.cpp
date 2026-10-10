#include "runtime/es_content.h"
#include "runtime/config.h"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>

namespace nwii::runtime {
namespace {
constexpr int32_t invalid = -1017;
constexpr size_t max_handles = 64;
namespace fs = std::filesystem;

std::string hex_name(uint64_t number, int width) {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(width) << number;
    return out.str();
}

// Validate the entire contiguous RAM span, not just the first mapped byte.
// Hardware, address wrapping, and MMU's end padding are never IPC buffers.
bool ram_span(const MMU& mmu, uint32_t addr, uint32_t len) {
    if (!addr || !len || uint64_t(addr) + len > 0x100000000ull) return false;
    const uint32_t physical = addr & 0x3FFFFFFF;
    if (physical < mmu.mem1.size() - 8)
        return uint64_t(physical) + len <= mmu.mem1.size() - 8;
    if (physical >= 0x10000000 && physical < 0x10000000 + mmu.mem2.size() - 8)
        return uint64_t(physical) + len <= 0x10000000 + mmu.mem2.size() - 8;
    return false;
}

uint32_t be32(const std::vector<uint8_t>& bytes, size_t at) {
    return uint32_t(bytes[at]) << 24 | uint32_t(bytes[at + 1]) << 16 |
           uint32_t(bytes[at + 2]) << 8 | bytes[at + 3];
}
uint16_t be16(const std::vector<uint8_t>& bytes, size_t at) {
    return uint16_t(bytes[at]) << 8 | bytes[at + 1];
}
uint64_t be64(const std::vector<uint8_t>& bytes, size_t at) {
    return uint64_t(be32(bytes, at)) << 32 | be32(bytes, at + 4);
}

bool find_content(uint32_t index, fs::path& content, uint32_t& size) {
    const auto& config = Config::get();
    if (config.game_dir.empty()) return false;
    const uint64_t title = make_title_id(config.game_id);
    const auto game = fs::path(config.game_dir).lexically_normal();
    fs::path metadata;
    for (const auto& directory : {game, game.parent_path()}) {
        for (const auto& name : {std::string("title.tmd"), hex_name(title, 16) + ".tmd"}) {
            std::error_code ec;
            const auto candidate = directory / name;
            if (fs::is_regular_file(candidate, ec)) { metadata = candidate; break; }
        }
        if (!metadata.empty()) break;
    }
    if (metadata.empty()) return false;
    std::ifstream input(metadata, std::ios::binary | std::ios::ate);
    const auto length = input.tellg();
    // Largest supported signature plus header and at most 512 content records.
    if (length < 4 || length > 0x240 + 0xA4 + 512 * 36) return false;
    std::vector<uint8_t> bytes(static_cast<size_t>(length));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), length)) return false;
    size_t payload = 0;
    switch (be32(bytes, 0)) {
    case 0x10000: payload = 0x240; break; // RSA4096
    case 0x10001: payload = 0x140; break; // RSA2048
    case 0x10002: payload = 0x80; break;  // ECC
    default: return false;
    }
    if (bytes.size() < payload + 0xA4 || be64(bytes, payload + 0x4C) != title)
        return false;
    const uint16_t count = be16(bytes, payload + 0x9E);
    if (!count || count > 512 || bytes.size() < payload + 0xA4 + size_t(count) * 36)
        return false;
    std::set<uint16_t> indices;
    bool found = false;
    for (uint16_t i = 0; i < count; ++i) {
        const auto at = payload + 0xA4 + size_t(i) * 36;
        const auto record_index = be16(bytes, at + 4);
        if (!indices.insert(record_index).second) return false;
        if (record_index != index) continue;
        const auto logical_size = be64(bytes, at + 8);
        if (logical_size > uint64_t(std::numeric_limits<int32_t>::max())) return false;
        content = metadata.parent_path() / (hex_name(be32(bytes, at), 8) + ".app");
        size = uint32_t(logical_size);
        found = true;
    }
    return found;
}
}

int32_t ESContent::open_session() {
    if (next_session_ == std::numeric_limits<int32_t>::max()) return invalid;
    return ++next_session_;
}

void ESContent::close_session(int32_t session) {
    std::erase_if(handles_, [session](const auto& entry) { return entry.second.owner == session; });
}

int32_t ESContent::open_content(int32_t session, uint32_t index) {
    if (index > 0xFFFF || handles_.size() >= max_handles ||
        next_handle_ == std::numeric_limits<int32_t>::max()) return invalid;
    fs::path path;
    uint32_t size = 0;
    if (!find_content(index, path, size)) return invalid;
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < std::streamoff(size)) return IPC_ENOENT;
    input.seekg(0);
    if (!input) return IPC_ENOENT;
    const auto handle = next_handle_++;
    handles_.emplace(handle, Handle{session, std::move(input), size});
    if (std::getenv("NWII_SAMPLE"))
        std::cout << "[ES Content] open index=" << index << " handle=" << handle << " size=" << size << '\n';
    return handle;
}

int32_t ESContent::ioctlv(CPUContext& ctx, const IpcRequest& req) {
    const uint32_t inputs = req.ioctl_cmd == 0x23 ? 3 : 1;
    const uint32_t outputs = req.ioctl_cmd == 0x0A ? 1 : 0;
    if (req.arg_cnt_in != inputs || req.arg_cnt_out != outputs ||
        req.ioctlv_vecs.size() != size_t(inputs + outputs)) return invalid;
    for (uint32_t i = 0; i < inputs; ++i) {
        const auto& v = req.ioctlv_vecs[i];
        if (v.len != 4 || !ram_span(ctx.mmu, v.addr, 4)) return invalid;
    }
    const uint32_t first = ctx.mmu.read32(req.ioctlv_vecs[0].addr);
    if (req.ioctl_cmd == 9) return open_content(req.device_fd, first);
    if (first > uint32_t(std::numeric_limits<int32_t>::max())) return invalid;
    const auto it = handles_.find(int32_t(first));
    if (it == handles_.end() || it->second.owner != req.device_fd) return invalid;
    auto& h = it->second;
    if (req.ioctl_cmd == 11) {
        handles_.erase(it);
        return 0;
    }
    if (req.ioctl_cmd == 0x23) {
        const auto offset = int32_t(ctx.mmu.read32(req.ioctlv_vecs[1].addr));
        const auto whence = ctx.mmu.read32(req.ioctlv_vecs[2].addr);
        if (whence > 2) return invalid;
        const int64_t base = whence == 0 ? 0 : whence == 1 ? h.position : h.size;
        const int64_t target = base + offset;
        if (target < 0 || target > h.size) return invalid;
        h.position = uint32_t(target);
        return int32_t(target);
    }
    if (req.ioctl_cmd != 10) return invalid;
    const auto& out = req.ioctlv_vecs[1];
    if (out.len > uint32_t(std::numeric_limits<int32_t>::max()) ||
        !ram_span(ctx.mmu, out.addr, out.len)) return invalid;
    const auto count = std::min(out.len, h.size - h.position);
    if (!count) return 0;
    h.file.clear();
    h.file.seekg(h.position);
    if (!h.file) return invalid;
    h.file.read(reinterpret_cast<char*>(ctx.mmu.get_ptr(out.addr)), count);
    const auto actual = h.file.gcount();
    if (actual == 0) return invalid; // Unexpected host I/O failure, not logical EOF.
    h.position += uint32_t(actual);
    if (std::getenv("NWII_SAMPLE"))
        std::cout << "[ES Content] read handle=" << first << " requested=" << out.len
                  << " returned=" << actual << " position=" << h.position << '\n';
    return int32_t(actual);
}
}
