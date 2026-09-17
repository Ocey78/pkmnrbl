#pragma once
#include <glad/glad.h>

// Shared by the real GPU test and its asset-free legacy-loader regression.
inline bool LoadShaderTestGL(GLADloadproc loader) {
    // Loader success only means a recognizable GL version, not GL 3.3.
    return gladLoadGLLoader(loader) != 0 && GLAD_GL_VERSION_3_3 != 0;
}
