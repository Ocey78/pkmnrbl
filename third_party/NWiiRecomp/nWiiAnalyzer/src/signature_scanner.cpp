#include "analyzer/signature_scanner.h"
#include <iomanip>
#include <sstream>

namespace nwii {
namespace analyzer {

SignatureScanner::SignatureScanner() {
    signatures_ = {
        { "OSReport", { "9421FF80", "7C0802A6", "9001????", "40860024", "D8210028", "D8410030", "D8610038" } },
        { "IOS_Open", { "9421FFE0", "7C0802A6", "9001????", "34010008", "93E1????", "93C1????", "3BC0????", "93A1????" } },
        { "IOS_IoctlAsync", { "9421FFC0", "7C0802A6", "9001????", "3961????", "48??????", "34010008", "7C771B78" } },
        { "IOS_Ioctl", { "9421FFD0", "7C0802A6", "9001????", "3961????", "48??????", "34010008", "7C791B78" } },
        // Frame setup, register saves, and argument captures do not identify
        // IOS_Ioctlv or IOS_IoctlvAsync: unrelated functions share these prologues.
        // Keep those functions native; verified explicit HLE hooks remain opt-in.
        

        

    };
}

std::string SignatureScanner::match(const std::vector<uint32_t>& instructions) const {
    for (const auto& sig : signatures_) {
        if (instructions.size() < sig.instructions.size()) {
            continue;
        }

        bool matched = true;
        for (size_t i = 0; i < sig.instructions.size(); ++i) {
            std::stringstream ss;
            ss << std::uppercase << std::setfill('0') << std::setw(8) << std::hex << instructions[i];
            std::string hex_inst = ss.str();
            const std::string& pattern = sig.instructions[i];

            for (size_t j = 0; j < 8; ++j) {
                if (pattern[j] != '?' && pattern[j] != hex_inst[j]) {
                    matched = false;
                    break;
                }
            }
            if (!matched) break;
        }

        if (matched) {
            return sig.name;
        }
    }
    return "";
}

} 
} 
