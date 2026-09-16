#include "runtime/config.h"
#include "runtime/title_identity.h"

#include <iostream>

int main() {
    const auto& cfg = nwii::runtime::Config::get();

    if (cfg.game_id != "WPSE") {
        std::cerr << "FAIL: Config default game ID is " << cfg.game_id
                  << ", expected WPSE\n";
        return 1;
    }

    if (nwii::runtime::pack_game_id(cfg.game_id) != 0x57505345u) {
        std::cerr << "FAIL: packed WPSE word is not 0x57505345\n";
        return 1;
    }

    if (nwii::runtime::kWiiWareTitleIdHigh != 0x00010001u) {
        std::cerr << "FAIL: WiiWare title high word is not 0x00010001\n";
        return 1;
    }

    if (nwii::runtime::make_title_id(cfg.game_id) !=
        0x0001000157505345ull) {
        std::cerr << "FAIL: full title ID is not 0001000157505345\n";
        return 1;
    }

    std::cout << "PASS: WPSE title identity = 0001000157505345\n";
    return 0;
}
