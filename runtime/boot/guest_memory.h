#pragma once

#include <cstdint>

class GuestMemory {
public:
    virtual ~GuestMemory() = default;

    virtual void write32_be(std::uint32_t address, std::uint32_t value) = 0;
};
