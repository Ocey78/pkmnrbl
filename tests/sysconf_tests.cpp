#include "runtime/devices.h"

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// The real FS device and MMU run here; unrelated hardware is link-only.
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

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

void configuration_is_readable_by_a_byte_item_consumer() {
    nwii::runtime::CPUContext ctx;
    auto device = nwii::runtime::devices::create_fs_device();
    const auto fd = device->open(ctx, "/shared2/sys/SYSCONF", 1);
    require(fd >= 0, "synthetic SYSCONF must open");
    constexpr uint32_t physical = 0x2000, cached = 0x80002000;
    ctx.mmu.write8(cached + 0x4000, 0xA5);
    require(device->read(ctx, fd, physical, 32) == 32, "first chunk must read 32 bytes");
    require(device->read(ctx, fd, physical + 32, 0x4000) == 0x3FE0,
            "remaining read stops at exactly 0x4000 bytes");
    require(ctx.mmu.read8(cached + 0x4000) == 0xA5, "read must preserve destination tail");
    require(device->read(ctx, fd, physical, 1) == 0, "read at EOF returns zero");
    std::vector<uint8_t> data(0x4000);
    for (uint32_t i = 0; i < data.size(); ++i) data[i] = ctx.mmu.read8(cached + i);
    require(std::string(data.begin(), data.begin() + 4) == "SCv0", "SYSCONF header must be SCv0");
    require(std::string(data.end() - 4, data.end()) == "SCed", "SYSCONF footer must be SCed");
    const auto be16 = [&](size_t at) { return unsigned(data.at(at)) * 256 + data.at(at + 1); };
    const auto count = be16(4);
    require(count > 0 && count < 100, "bounded nonempty entry count");
    unsigned pos = 6 + 2 * (count + 1);
    std::map<std::string, unsigned> settings;
    for (unsigned i = 0; i < count; ++i) {
        require(be16(6 + 2 * i) == pos, "offset table must describe contiguous records");
        require(pos < 0x3FFC, "record starts before footer");
        const auto descriptor = data.at(pos++);
        require((descriptor >> 5) == 3, "minimal configuration items must have byte type");
        const unsigned nameSize = (descriptor & 31) + 1;
        require(pos + nameSize < 0x3FFC, "complete name and value must fit");
        const std::string name(data.begin() + pos, data.begin() + pos + nameSize);
        require(name.find('\0') == std::string::npos, "item names must not include NUL terminators");
        pos += nameSize;
        require(settings.emplace(name, data.at(pos++)).second, "item names must be unique");
    }
    require(be16(6 + 2 * count) == pos, "last offset must mark the end of record data");
    require(settings.contains("IPL.LNG") && settings.at("IPL.LNG") == 1,
            "supported USA title must receive English system language");
    require(settings.contains("IPL.AR") && settings.at("IPL.AR") == 0,
            "aspect ratio must be the 4:3 byte, not an NTSC string");
    require(device->seek(ctx, fd, 0, 0) == 0, "rewind configuration");
    require(device->read(ctx, fd, physical, 4) == 4 && ctx.mmu.read32(cached) == 0x53437630,
            "rewound read returns the same SCv0 header through physical RAM alias");
    require(device->close(ctx, fd) == 0, "close configuration handle");
}
}

int main() {
    try {
        configuration_is_readable_by_a_byte_item_consumer();
        std::cout << "PASS: original synthetic SYSCONF is readable through FS/MMU\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
