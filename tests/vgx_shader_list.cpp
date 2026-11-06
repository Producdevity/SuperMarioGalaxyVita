#include "vgx_state.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

namespace {
constexpr unsigned kVs = 20, kFs = 150, kPairs = kVs * kFs;
unsigned frames = 0;
bool deferred = false;
bool truncatedVariant = false;
bool extraShader = false;
bool ignoreTruncate = false;
void Require(bool ok, const char* why) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
template <class T> void Bytes(std::vector<uint8_t>& out, const T& value) {
    const auto* p = reinterpret_cast<const uint8_t*>(&value);
    out.insert(out.end(), p, p + sizeof value);
}
std::vector<uint8_t> Recipe() {
    struct Header { char magic[8]; uint32_t vs, fs, version, reserved; };
    const Header h{{'V','G','X','S','H','L','0','1'}, sizeof(vgx::VsKey), sizeof(vgx::FsKey), vgx::kShaderGenVersion, 0};
    std::vector<uint8_t> out;
    Bytes(out, h);
    for (unsigned i = 0; i != kVs; ++i) {
        vgx::VsKey key{};
        key.numChans = 1;
        key.chan[0].enable = 1;
        key.chan[0].lightMask = static_cast<uint8_t>(i + 1);
        Bytes(out, 1u); Bytes(out, key);
    }
    for (unsigned i = 0; i != kFs; ++i) {
        vgx::FsKey key{};
        key.alphaTestOn = 1;
        key.alphaTest = static_cast<uint8_t>(i);
        Bytes(out, 2u); Bytes(out, key);
    }
    for (unsigned type : {3u, 4u})
        for (unsigned vs = 0; vs != kVs; ++vs)
            for (unsigned fs = 0; fs != kFs; ++fs) {
                Bytes(out, type); Bytes(out, vs); Bytes(out, fs);
                if (type == 4) { Bytes(out, 0u); Bytes(out, 0xC01u); }
            }
    return out;
}
void Observe(vgx::CmdBuf& buf) {
    const auto& commands = deferred ? buf.warm : buf.cmds;
    const size_t originalWork = kVs + kFs + 2 * kPairs - (truncatedVariant ? 1 : 0);
    const size_t work = frames == 0 ? originalWork + unsigned(extraShader) : 0;
    Require(commands.size() == work + (deferred ? 0 : 1), "replay produces every unique work command exactly once");
    for (size_t i = 0; i < work; ++i) {
        const auto& c = commands[i];
        if (i == originalWork) {
            Require(extraShader && c.type == vgx::CMD_SHADER && buf.shaders.at(c.arg)->fragment &&
                    buf.shaders.at(c.arg)->id == kFs, "valid key survives rejection of its incompatible pair");
            continue;
        }
        if (i < kVs + kFs) {
            Require(c.type == vgx::CMD_SHADER, "shader commands precede all programs/variants");
            const auto* shader = buf.shaders.at(c.arg);
            Require(shader->fragment == (i >= kVs) && shader->id == (i < kVs ? i : i - kVs),
                    "shader identities and replay order are unchanged");
        } else if (i < kVs + kFs + kPairs) {
            const unsigned pair = i - kVs - kFs;
            Require(c.type == vgx::CMD_PROGRAM && c.arg == ((pair / kFs) << 16 | pair % kFs),
                    "program pair identities and command order remain unchanged");
        } else {
            const unsigned pair = i - kVs - kFs - kPairs;
            Require(c.type == vgx::CMD_VARIANT, "all variants replay after their program dependencies");
            const auto& v = buf.variants.at(c.arg);
            Require(v.vsId == pair / kFs && v.fsId == pair % kFs && v.layoutMask == 0 && v.blendKey == 0xC01,
                    "shader IDs sharing layout/blend remain distinct and in the same replay order");
        }
    }
    Require(buf.draws.empty(), "prewarming does not introduce ordinary geometry draws");
    ++frames;
    vgx::backend::Execute(buf);
}
}
extern "C" FILE* __real_fopen(const char*, const char*);
extern "C" FILE* __wrap_fopen(const char* path, const char* mode) {
    if (ignoreTruncate && std::strcmp(mode, "wb") == 0) {
        if (FILE* f = __real_fopen(path, "rb+")) return f;
    }
    return __real_fopen(path, mode);
}
bool consoleRename = false; // rename refuses an existing destination, as the Vita's sceIoRename does
extern "C" int __real_rename(const char*, const char*);
extern "C" int __wrap_rename(const char* from, const char* to) {
    if (consoleRename) {
        if (FILE* existing = __real_fopen(to, "rb")) {
            std::fclose(existing);
            errno = EEXIST;
            return -1;
        }
    }
    return __real_rename(from, to);
}
namespace vgx {
bool LogEnabled() { return false; }
void SubmitToBackend(CmdBuf& b) { backend::Execute(b); }
}
int main(int argc, char** argv) {
    Require(argc == 3, "supply clean/duplicate and deferred flag");
    deferred = std::atoi(argv[2]) != 0;
    const auto expected = Recipe();
    auto canonical = expected, input = expected;
    const std::string mode = argv[1];
    if (mode == "bad-program" || mode == "bad-variant") {
        extraShader = true;
        vgx::FsKey incompatible{};
        incompatible.numTexGens = 1;
        Bytes(input, 2u); Bytes(input, incompatible);
        const size_t keyEnd = expected.size() - kPairs * (12u + 20u);
        canonical.insert(canonical.begin() + keyEnd, input.begin() + expected.size(), input.end());
        Bytes(input, mode == "bad-program" ? 3u : 4u);
        Bytes(input, 0u); Bytes(input, kFs);
        if (mode == "bad-variant") { Bytes(input, 0u); Bytes(input, 0xC01u); }
    } else if (mode == "no-truncate") {
        ignoreTruncate = true;
        input.push_back(4);
    } else if (mode == "duplicate" || mode == "console-rename") {
        consoleRename = mode == "console-rename";
        input.insert(input.end(), expected.end() - 20, expected.end());
    }
    else if (mode.rfind("tail-", 0) == 0) {
        const unsigned n = std::stoul(mode.substr(5));
        Require(n >= 1 && n <= 3, "tail fixture is an incomplete record type");
        input.insert(input.end(), n, 4);
    } else if (mode == "payload") {
        truncatedVariant = true;
        input.resize(input.size() - 7);
        canonical.resize(canonical.size() - 20);
    } else Require(mode == "clean", "known replay/recovery scenario");
    char path[] = "/tmp/vgx-recipe-XXXXXX";
    const int fd = mkstemp(path);
    Require(fd >= 0, "test owns a unique temporary recipe");
    FILE* f = fdopen(fd, "wb");
    Require(f && std::fwrite(input.data(), 1, input.size(), f) == input.size() && std::fclose(f) == 0,
            "synthetic recipe fixture is complete");
    if (ignoreTruncate) {
        const std::string stale = std::string(path) + ".tmp";
        FILE* old = __real_fopen(stale.c_str(), "wb");
        Require(old && std::fwrite(input.data(), 1, input.size(), old) == input.size() && std::fclose(old) == 0,
                "stale replacement from an interrupted rewrite exists before recovery");
    }
    vgx::SetShaderListFile(path);
    vgx::SetShaderWarmupDeferred(deferred);
    vgx::ResetState();
    vgx::SetFrameSink(Observe);
    vgx::FrontPresent(true, nullptr);
    vgx::FrontPresent(true, nullptr);
    f = std::fopen(path, "rb");
    std::vector<uint8_t> actual(canonical.size());
    Require(f && std::fread(actual.data(), 1, actual.size(), f) == actual.size() && std::fgetc(f) == EOF &&
            std::fclose(f) == 0 && actual == canonical,
            "replay/recovery retains the byte-identical canonical complete-record prefix");
    Require(frames == 2, "second frame does not replay known recipes again");
    std::remove(path);
    std::printf("PASS: real frontend, 3000 shader-ID variants, canonical recipe bytes and replay order (%s, deferred=%d, size_t=%zu)\n",
                argv[1], deferred, sizeof(size_t));
}
