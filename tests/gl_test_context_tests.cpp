#include "gl_test_context.h"
#include <cstring>
#include <iostream>

namespace {
const GLubyte* APIENTRY legacy_get_string(GLenum name) {
    static const GLubyte version[] = "1.1 synthetic legacy driver";
    static const GLubyte empty[] = "";
    return name == GL_VERSION ? version : empty;
}
void* legacy_loader(const char* name) {
    return std::strcmp(name, "glGetString") == 0
        ? reinterpret_cast<void*>(&legacy_get_string) : nullptr;
}
}

int main() {
    // Use the real GLAD loader. It accepts a valid 1.1 version string without
    // loading shader functions, as can happen with SDL's WGL fallback.
    if (LoadShaderTestGL(legacy_loader)) {
        std::cerr << "FAIL: legacy GL was accepted despite missing shader support\n";
        return 1;
    }
    if (GLVersion.major != 1 || GLVersion.minor != 1 || glCreateShader != nullptr) {
        std::cerr << "FAIL: legacy-loader fixture did not exercise the expected boundary\n";
        return 1;
    }
    std::cout << "PASS: legacy GL rejected before unavailable shader calls\n";
    return 0;
}
