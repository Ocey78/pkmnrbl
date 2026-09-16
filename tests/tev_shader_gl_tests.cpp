#include "runtime/gx/tev_shader_gen.h"
#include "runtime/gx/renderer.h"
#include "runtime/cpu_context.h"
#include <SDL.h>
#include <glad/glad.h>
#include <cmath>
#include <iostream>

namespace nwii::runtime {
MMU* g_mmu = nullptr;
CPUContext* g_ctx_ptr = nullptr;
}

static bool color_bank_regression() {
    using namespace nwii::runtime::gx;
    auto reset = std::make_unique<GXState>();
    g_state = *reset;
    g_state.projSet = true;
    g_state.projType = 1;
    g_state.projection[0] = g_state.projection[2] = g_state.projection[4] = 1;
    auto renderer = IRenderer::Create();
    renderer->Initialize(nullptr);
    std::vector<GXCommand> commands;
    auto bp = [&](uint8_t reg, uint32_t value) {
        GXCommand command{};
        command.type = GXCommandType::BPRegister;
        command.reg = reg;
        command.val = value;
        commands.push_back(command);
    };
    bp(0x00, 0); // one TEV stage
    bp(0x41, 0x18); // color and alpha writes, no blend
    bp(0x28, 0); // no texture
    bp(0xC0, 0x08FFF2); // output regular color register 1
    bp(0xC1, 0x08FF90); // output regular alpha register 1
    bp(0xE2, 0x0FF040); // regular red=64, alpha=255
    bp(0xE3, 0x0800C0); // regular green=128, blue=192
    GXCommand draw{};
    draw.type = GXCommandType::DrawPrimitive;
    draw.prim_type = 0x90;
    for (auto position : {std::array<float, 2>{-1, -1}, {3, -1}, {-1, 3}}) {
        VertexData vertex{};
        vertex.has_pos = vertex.pre_xf = true;
        vertex.pos[0] = position[0]; vertex.pos[1] = position[1];
        draw.vertices.push_back(vertex);
    }
    commands.push_back(draw);
    glViewport(0, 0, 32, 32);
    glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
    renderer->Render(commands);
    unsigned char pixel[4]{};
    glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    if (pixel[0] != 64 || pixel[1] != 128 || pixel[2] != 192 || pixel[3] != 255) {
        std::cerr << "FAIL: regular-bank control draw\n";
        return false;
    }
    commands.clear();
    bp(0xE2, 0x8FF0FF); // same addresses, constant white (must not erase regular)
    bp(0xE3, 0x8FF0FF);
    commands.push_back(draw);
    renderer->Render(commands);
    glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    bool passed = pixel[0] == 64 && pixel[1] == 128 && pixel[2] == 192 && pixel[3] == 255;
    if (!passed) std::cerr << "FAIL: regular TEV bank after constant write: "
        << unsigned(pixel[0]) << ',' << unsigned(pixel[1]) << ',' << unsigned(pixel[2])
        << ',' << unsigned(pixel[3]) << '\n';
    commands.clear();
    bp(0xC0, 0x08FFFE); // select constant RGB
    bp(0xC1, 0x08FFE0); // select constant alpha
    bp(0xF6, (0x0D << 4) | (0x1D << 9)); // K1 RGB, K1 alpha
    bp(0xE2, 0x0807FF); // regular RA only: red=-1, alpha=128
    commands.push_back(draw);
    renderer->Render(commands);
    glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    if (pixel[0] != 255 || pixel[1] != 255 || pixel[2] != 255 || pixel[3] != 255) {
        std::cerr << "FAIL: constant bank was erased by a regular RA write\n";
        passed = false;
    }
    commands.clear();
    bp(0xC0, 0x08FFF2); bp(0xC1, 0x08FF90);
    commands.push_back(draw);
    renderer->Render(commands);
    glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    if (pixel[0] != 0 || pixel[1] != 128 || pixel[2] != 192 || pixel[3] != 128) {
        std::cerr << "FAIL: regular RA update did not preserve BG\n";
        passed = false;
    }
    GLint program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    const GLint color = glGetUniformLocation(program, "uTevColor[1]");
    float rgba[4]{};
    if (color >= 0) glGetUniformfv(program, color, rgba);
    if (color < 0 || std::abs(rgba[0] + 1.f / 255.f) > 0.00001f) {
        std::cerr << "FAIL: signed eleven-bit color upload\n";
        passed = false;
    }
    if (glGetError() != GL_NO_ERROR) {
        std::cerr << "FAIL: GL error during color-bank regression\n";
        passed = false;
    }
    if (passed) std::cout << "PASS: independent TEV banks, partial writes and signed color upload\n";
    return passed;
}

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
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cout << "SKIP: SDL video unavailable: " << SDL_GetError() << '\n';
        return 77;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    // Pixel assertions include alpha; SDL otherwise permits an RGB-only buffer.
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    auto* window = SDL_CreateWindow("Shader regression", 0, 0, 32, 32,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) {
        std::cout << "SKIP: GL window unavailable: " << SDL_GetError() << '\n';
        SDL_Quit(); return 77;
    }
    auto context = SDL_GL_CreateContext(window);
    if (!context || !gladLoadGLLoader(SDL_GL_GetProcAddress)) {
        std::cout << "SKIP: OpenGL 3.3 unavailable: " << SDL_GetError() << '\n';
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
    passed = color_bank_regression() && passed;
    SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
    if (passed) std::cout << "PASS: 1, 2 and 16 TEV stages compile and link\n";
    return passed ? 0 : 1;
}
