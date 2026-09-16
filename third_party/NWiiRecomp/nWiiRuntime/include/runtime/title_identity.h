#pragma once

#include <cstdint>
#include <string_view>

namespace nwii::runtime {

inline constexpr uint32_t kWiiWareTitleIdHigh = 0x00010001u;
inline constexpr std::string_view kPokemonRumbleGameId = "WPSE";

constexpr uint32_t pack_game_id(std::string_view game_id) {
    uint32_t low = 0;
    for (size_t i = 0; i < 4 && i < game_id.size(); ++i) {
        low |= static_cast<uint32_t>(static_cast<uint8_t>(game_id[i]))
               << ((3u - static_cast<uint32_t>(i)) * 8u);
    }
    return low;
}

constexpr uint64_t make_title_id(std::string_view game_id) {
    return (static_cast<uint64_t>(kWiiWareTitleIdHigh) << 32) |
           pack_game_id(game_id);
}

static_assert(pack_game_id(kPokemonRumbleGameId) == 0x57505345u);
static_assert(make_title_id(kPokemonRumbleGameId) == 0x0001000157505345ull);

} // namespace nwii::runtime
