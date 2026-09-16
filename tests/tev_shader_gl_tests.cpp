#include "runtime/gx/tev_shader_gen.h"
#include <SDL.h>
#include <glad/glad.h>
#include <iostream>

static bool compile(GLuint shader, const std::string& source) {
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[8192]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "FAIL: generated shader compilation: " << log << '\n';
    }
    return ok != 0;
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 77;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    auto* window = SDL_CreateWindow("Shader regression", 0, 0, 32, 32,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) { SDL_Quit(); return 77; }
    auto context = SDL_GL_CreateContext(window);
    if (!context || !gladLoadGLLoader(SDL_GL_GetProcAddress)) {
        if (context) SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window); SDL_Quit(); return 77;
    }
    bool passed = true;
    for (unsigned count : {1u, 2u, 16u}) {
        nwii::runtime::gx::GXState state{};
        state.numTevStages = static_cast<uint8_t>(count);
        for (auto& stage : state.tevStages) {
            stage.texMap = 0;
            stage.texCoord = 0;
            stage.colorInA = 15; // zero
            stage.colorInB = 8;  // texture
            stage.colorInC = 10; // raster
            stage.colorInD = 0;  // preceding stage result
            stage.alphaInA = 7;
            stage.alphaInB = 4;
            stage.alphaInC = 5;
            stage.alphaInD = 0;
            stage.colorClamp = stage.alphaClamp = 1;
        }
        const auto source = nwii::runtime::gx::GenerateTEVShader(state, 0x90);
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        bool ok = compile(vs, source.vertex_source);
        ok = compile(fs, source.fragment_source) && ok;
        GLuint program = glCreateProgram();
        glAttachShader(program, vs); glAttachShader(program, fs);
        glLinkProgram(program);
        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[8192]{};
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            std::cerr << "FAIL: " << unsigned(count) << "-stage link: " << log << '\n';
        }
        passed = passed && ok && linked;
        glDeleteProgram(program); glDeleteShader(vs); glDeleteShader(fs);
    }
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    if (passed) std::cout << "PASS: 1, 2 and 16 TEV stages compile and link\n";
    return passed ? 0 : 1;
}
