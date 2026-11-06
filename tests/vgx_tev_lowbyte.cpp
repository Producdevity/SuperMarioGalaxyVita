// TevLowByteRegs (vgx_shadergen.h): which loaded TEV registers may be uploaded as their low 8 bits (tools/test_vgx_tev_lowbyte.sh).
#include "vgx_shadergen.h"
#include <cstdio>
#include <cstdlib>

namespace vgx { bool LogEnabled() { return false; } }
namespace {
void Require(bool ok, const char* why) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
    std::printf("ok: %s\n", why);
}
// Colour arguments 0..15 (CPREV, APREV, C0, A0, ... ZERO 15) and alpha arguments 0..7 (APREV, A0.. ZERO 7).
uint32_t Color(unsigned a, unsigned b, unsigned c, unsigned d, unsigned dest, bool clamp = true) {
    return (a << 12) | (b << 8) | (c << 4) | d | (dest << 22) | (unsigned(clamp) << 19);
}
uint32_t Alpha(unsigned a, unsigned b, unsigned c, unsigned d, unsigned dest, bool clamp = true) {
    return (a << 13) | (b << 10) | (c << 7) | (d << 4) | (dest << 22) | (unsigned(clamp) << 19);
}
constexpr uint8_t ColorBit(unsigned reg) { return static_cast<uint8_t>(1u << (reg * 2)); }
constexpr uint8_t AlphaBit(unsigned reg) { return static_cast<uint8_t>(1u << (reg * 2 + 1)); }
const uint32_t kNoColor = Color(15, 15, 15, 15, 0), kNoAlpha = Alpha(7, 7, 7, 7, 0);
} // namespace

int main() {
    {
        // Mario's eyeball: PREV.a = lerp(APREV, TEXA, A0); A0 is loaded as -122.
        vgx::FsKey k;
        k.stage[0].color = kNoColor;
        k.stage[0].alpha = Alpha(0, 4, 1, 7, 0);
        const uint8_t m = vgx::TevLowByteRegs(k);
        Require(m & AlphaBit(1), "an alpha read only as c gets its low byte");
        Require(m & AlphaBit(0), "the loaded PREV alpha read as a gets its low byte");
        Require(!(m & ColorBit(1)), "the colour half of the same register is not read");
    }
    {
        // A Luma's body (Body1_v): REG0 is the signed d of stage 0, REG2 only an a of stage 2.
        vgx::FsKey k;
        k.numStages = 3;
        k.stage[0].color = Color(4, 8, 10, 2, 0);
        k.stage[1].color = Color(1, 10, 1, 0, 0);
        k.stage[2].color = Color(6, 0, 0, 15, 0);
        for (int s = 0; s < 3; ++s) k.stage[s].alpha = kNoAlpha;
        const uint8_t m = vgx::TevLowByteRegs(k);
        Require(!(m & ColorBit(1)), "a register read as d keeps its signed value");
        Require(m & ColorBit(3), "a register read only as a gets its low byte");
        Require(m & ColorBit(2), "REG1 read as a in stage 0 gets its low byte");
        Require(!(m & AlphaBit(0)), "PREV alpha read after stage 0 wrote PREV is not the loaded value");
    }
    {
        // Dino Piranha (every material of DinoPackun.bdl): stage 2 adds C2 x the environment-mapped highlight; C2 is loaded
        // as 792..906 and read only as b, so GX uses its low byte (837 -> 69), not 3.3 times the highlight.
        vgx::FsKey k;
        k.numStages = 3;
        k.stage[0].color = Color(4, 8, 10, 2, 0);
        k.stage[1].color = Color(14, 10, 1, 0, 0);
        k.stage[2].color = Color(15, 6, 8, 0, 0);
        k.stage[0].alpha = Alpha(4, 6, 5, 1, 0);
        k.stage[1].alpha = Alpha(7, 6, 5, 1, 0);
        k.stage[2].alpha = Alpha(0, 7, 7, 7, 0);
        const uint8_t m = vgx::TevLowByteRegs(k);
        Require(m & ColorBit(3), "a highlight colour read only as b gets its low byte");
        Require(!(m & ColorBit(1)) && !(m & AlphaBit(1)), "C0 and A0, read as d, keep their signed values");
    }
    {
        // Overwritten before it is read: the register's loaded value never reaches an operand.
        vgx::FsKey k;
        k.numStages = 2;
        k.stage[0].color = Color(15, 15, 15, 12, 3);
        k.stage[1].color = Color(6, 15, 15, 15, 0);
        k.stage[0].alpha = k.stage[1].alpha = kNoAlpha;
        Require(!(vgx::TevLowByteRegs(k) & ColorBit(3)), "a register written before it is read is not the loaded value");
    }
    {
        // Read as c in one stage and as d in a later one, both before it is written: the signed value wins.
        vgx::FsKey k;
        k.numStages = 2;
        k.stage[0].color = Color(15, 15, 2, 15, 0);
        k.stage[1].color = Color(15, 15, 15, 2, 0);
        k.stage[0].alpha = k.stage[1].alpha = kNoAlpha;
        Require(!(vgx::TevLowByteRegs(k) & ColorBit(1)), "a register also read as d keeps its signed value");
    }
    {
        // A colour argument that reads a register's alpha (A0 as c0.aaa) marks the alpha half.
        vgx::FsKey k;
        k.stage[0].color = Color(15, 3, 8, 15, 0);
        k.stage[0].alpha = kNoAlpha;
        const uint8_t m = vgx::TevLowByteRegs(k);
        Require((m & AlphaBit(1)) && !(m & ColorBit(1)), "A0 as a colour operand marks the alpha half only");
    }
    {
        // A stage reads its operands before it writes: REG2 as d and its own destination.
        vgx::FsKey k;
        k.stage[0].color = Color(15, 15, 15, 6, 3);
        k.stage[0].alpha = kNoAlpha;
        Require(!(vgx::TevLowByteRegs(k) & ColorBit(3)), "a stage's own d is read before its destination is written");
    }
    std::printf("PASS: TEV low-byte registers\n");
}
