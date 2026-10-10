#pragma once
#include "runtime/ios_device.h"
#include <fstream>
#include <map>

namespace nwii::runtime {

// Read-only access to separately supplied, decrypted local title content.
// This is not a NAND installer, decryptor, or signature verifier.
class ESContent {
public:
    int32_t open_session();
    void close_session(int32_t session);
    int32_t ioctlv(CPUContext& ctx, const IpcRequest& req);
private:
    struct Handle {
        int32_t owner;
        std::ifstream file;
        uint32_t size;
        uint32_t position = 0;
    };
    std::map<int32_t, Handle> handles_;
    int32_t next_session_ = 0;
    int32_t next_handle_ = 0;
    int32_t open_content(int32_t session, uint32_t index);
};
}
