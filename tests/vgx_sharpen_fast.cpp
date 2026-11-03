// The present's two sharpening programs (vgx_backend_gl.cpp: the clamped one and g_sharpFast, whose coordinates come from
// the vertex program) on the same picture: an EFB-sized texture of noise, edges and flat areas drawn at the screen's size,
// upright and flipped, at several amounts. Away from the picture's outermost screen pixels the two must give the same
// bytes (at most 1 apart: the coordinates are interpolated instead of offset per pixel); the border pixels may differ
// (the fast program's neighbour fetches there blend with the next texel). The shader sources are the file's own, taken
// out by tools/test_vgx_sharpen_fast.sh (sharpen_sources.h). Mesa, EGL without a surface.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "sharpen_sources.h"  // kClampedVs, kClampedFs, kFastVs, kFastFs

static GLuint Compile(GLenum type, const char* src) {
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "compile failed: %s\n%s\n", log, src);
        std::exit(1);
    }
    return s;
}

struct Prog {
    GLuint prog;
    GLint rect, size, lim, p;
};
static Prog Link(const char* vs, const char* fs) {
    Prog r{};
    r.prog = glCreateProgram();
    glAttachShader(r.prog, Compile(GL_VERTEX_SHADER, vs));
    glAttachShader(r.prog, Compile(GL_FRAGMENT_SHADER, fs));
    glBindAttribLocation(r.prog, 0, "a_pos");
    glLinkProgram(r.prog);
    GLint ok = 0;
    glGetProgramiv(r.prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        std::fprintf(stderr, "link failed\n");
        std::exit(1);
    }
    r.rect = glGetUniformLocation(r.prog, "u_rect");
    r.size = glGetUniformLocation(r.prog, "u_size");
    r.lim = glGetUniformLocation(r.prog, "u_lim");
    r.p = glGetUniformLocation(r.prog, "u_p");
    glUseProgram(r.prog);
    glUniform1i(glGetUniformLocation(r.prog, "u_src"), 0);
    return r;
}

static uint32_t sRng = 12345;
static uint32_t Next() {
    sRng = sRng * 1664525u + 1013904223u;
    return sRng >> 8;
}

int main() {
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, nullptr, nullptr)) return std::puts("no EGL display"), 1;
    eglBindAPI(EGL_OPENGL_ES_API);
    const EGLint cfgAttr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_NONE};
    EGLConfig cfg;
    EGLint n = 0;
    eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n);
    const EGLint ctxAttr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    const EGLint pbAttr[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pbAttr);
    if (!eglMakeCurrent(dpy, surf, surf, ctx)) return std::puts("no EGL context"), 1;

    const Prog clamped = Link(kClampedVs, kClampedFs), fast = Link(kFastVs, kFastFs);
    if (fast.lim >= 0) return std::puts("the fast program has u_lim: not the expected source"), 1;

    const float quad[] = {-1, -1, 1, -1, -1, 1, 1, 1};
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof quad, quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    unsigned failures = 0;
    const int sizes[][2] = {{592, 424}, {544, 388}, {640, 456}, {480, 340}};
    for (const auto& sz : sizes) {
        const int tw = sz[0], th = sz[1], dw = 960, dh = 544;
        std::vector<uint8_t> pic(static_cast<size_t>(tw) * th * 4);
        for (int y = 0; y < th; ++y)
            for (int x = 0; x < tw; ++x) {
                uint8_t* p = &pic[(static_cast<size_t>(y) * tw + x) * 4];
                const int region = (x / 64 + y / 64) % 3;  // noise, hard edges, flat
                for (int c = 0; c < 4; ++c)
                    p[c] = region == 0 ? static_cast<uint8_t>(Next()) : region == 1 ? (((x / 3 + y / 5 + c) & 1) ? 255 : 0) : static_cast<uint8_t>(40 * c + 30);
            }
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, pic.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLuint outTex[2], fbo[2];
        glGenTextures(2, outTex);
        glGenFramebuffers(2, fbo);
        for (int i = 0; i < 2; ++i) {
            glBindTexture(GL_TEXTURE_2D, outTex[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dw, dh, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex[i], 0);
        }
        for (const bool flipY : {true, false})
            for (const float amount : {0.0f, 0.3f, 0.62f, 0.8f}) {
                std::vector<uint8_t> out[2];
                for (int i = 0; i < 2; ++i) {
                    const Prog& pr = i == 0 ? clamped : fast;
                    glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
                    glViewport(0, 0, dw, dh);
                    glUseProgram(pr.prog);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, tex);
                    // (PresentSharpened's uniforms for the whole picture: x = y = 0, w x h = the texture)
                    if (flipY) glUniform4f(pr.rect, 0.0f, 1.0f, 1.0f, -1.0f);
                    else glUniform4f(pr.rect, 0.0f, 0.0f, 1.0f, 1.0f);
                    glUniform4f(pr.size, static_cast<float>(tw), static_cast<float>(th), 1.0f / tw, 1.0f / th);
                    if (pr.lim >= 0) glUniform4f(pr.lim, 0.5f / tw, 0.5f / th, (tw - 0.5f) / tw, (th - 0.5f) / th);
                    glUniform4f(pr.p, static_cast<float>(dw) / tw, static_cast<float>(dh) / th, amount, 0.0f);
                    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                    out[i].resize(static_cast<size_t>(dw) * dh * 4);
                    glReadPixels(0, 0, dw, dh, GL_RGBA, GL_UNSIGNED_BYTE, out[i].data());
                }
                unsigned inner = 0, innerMax = 0, border = 0, borderMax = 0;
                for (int y = 0; y < dh; ++y)
                    for (int x = 0; x < dw; ++x)
                        for (int c = 0; c < 3; ++c) {
                            const size_t k = (static_cast<size_t>(y) * dw + x) * 4 + c;
                            const unsigned d = static_cast<unsigned>(std::abs(out[0][k] - out[1][k]));
                            if (!d) continue;
                            // the outermost screen pixels: those whose own coordinate or a neighbour's crosses the
                            // first or last texel centre (two screen pixels per texel at most at these sizes)
                            const bool edge = x < 2 || y < 2 || x >= dw - 2 || y >= dh - 2;
                            if (edge) { ++border; borderMax = d > borderMax ? d : borderMax; }
                            else { ++inner; innerMax = d > innerMax ? d : innerMax; }
                        }
                const bool ok = innerMax <= 1;
                std::printf("%dx%d %s amount %.2f: inner %u values differ (max %u), border %u (max %u)%s\n", tw, th, flipY ? "flipped" : "upright",
                            amount, inner, innerMax, border, borderMax, ok ? "" : "  FAIL");
                failures += ok ? 0 : 1;
            }
        glDeleteTextures(1, &tex);
        glDeleteTextures(2, outTex);
        glDeleteFramebuffers(2, fbo);
    }
    std::printf("sharpen fast: %s\n", failures ? "FAILED" : "pass");
    return failures ? 1 : 0;
}
