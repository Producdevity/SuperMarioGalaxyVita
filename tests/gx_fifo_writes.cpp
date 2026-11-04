// The FIFO writers that write a command or a vertex's fields with one limit check (port/gx_fifo.h: __PortGXFifo_u8u32 and
// the others, GXVert.h's GXPosition3f32 and the others) against the same fields written one at a time: same bytes, same end
// pointer, the overflow called at the same writes with the same pointer, at every distance from the limit and whatever the
// overflow does next (tools/test_gx_fifo_writes.sh).
#ifdef NDEBUG
#error "tests/gx_fifo_writes.cpp needs assert(): build it without NDEBUG"
#endif
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "revolution/types.h"
#include "port/gx_fifo.h"
#include "revolution/gx/GXVert.h"

uint8_t* __PortGXFifoPtr;
uint8_t* __PortGXFifoLimit;

// The buffer the writes start in, the one an overflow moves them to (a publish or a flush: the writes go on there with a new
// limit) and a sink (a display list that overflowed: further writes are dropped there).
alignas(64) static uint8_t sFirst[128], sSecond[128], sSink[64];
static unsigned sMode;
static std::vector<long> sCalls;  // where the pointer stood at each overflow call (offset into the buffer it pointed to)
static long offsetOf(const uint8_t* p) {
    if (p >= sFirst && p <= sFirst + sizeof sFirst) return p - sFirst;
    if (p >= sSecond && p <= sSecond + sizeof sSecond) return 1000 + (p - sSecond);
    if (p >= sSink && p <= sSink + sizeof sSink) return 2000 + (p - sSink);
    return -1;
}
extern "C" void __PortGXFifoOverflow(void) {
    sCalls.push_back(offsetOf(__PortGXFifoPtr));
    switch (sMode) {
    case 0:  // goes on in the other buffer
        __PortGXFifoPtr = sSecond + 3;
        __PortGXFifoLimit = sSecond + 96;
        break;
    case 1:  // drops what follows
        __PortGXFifoPtr = sSink;
        __PortGXFifoLimit = sSink + 40;
        break;
    default:  // a soft limit moved on in the same buffer
        __PortGXFifoLimit = __PortGXFifoPtr + 1 + sMode;
        break;
    }
}

struct Result {
    uint8_t first[sizeof sFirst], second[sizeof sSecond], sink[sizeof sSink];
    long end;
    std::vector<long> calls;
};
static Result run(void (*write)(const uint32_t*), const uint32_t* v, int room, unsigned mode) {
    memset(sFirst, 0xAA, sizeof sFirst);
    memset(sSecond, 0xBB, sizeof sSecond);
    memset(sSink, 0xCC, sizeof sSink);
    __PortGXFifoPtr = sFirst + 40;
    __PortGXFifoLimit = sFirst + 40 + room;
    sMode = mode;
    sCalls.clear();
    write(v);
    Result r;
    memcpy(r.first, sFirst, sizeof sFirst);
    memcpy(r.second, sSecond, sizeof sSecond);
    memcpy(r.sink, sSink, sizeof sSink);
    r.end = offsetOf(__PortGXFifoPtr);
    r.calls = sCalls;
    return r;
}

static float asFloat(uint32_t u) {
    float f;
    memcpy(&f, &u, 4);
    return f;
}

struct Case {
    const char* name;
    void (*real)(const uint32_t*);
    void (*ref)(const uint32_t*);
};
static const Case kCases[] = {
    {"BP register", [](const uint32_t* v) { __PortGXFifo_u8u32(v[0], v[1]); },
     [](const uint32_t* v) { __PortGXFifo_u8(v[0]); __PortGXFifo_u32(v[1]); }},
    {"CP register", [](const uint32_t* v) { __PortGXFifo_u8u8u32(v[0], v[1], v[2]); },
     [](const uint32_t* v) { __PortGXFifo_u8(v[0]); __PortGXFifo_u8(v[1]); __PortGXFifo_u32(v[2]); }},
    {"XF register", [](const uint32_t* v) { __PortGXFifo_u8u32u32(v[0], v[1], v[2]); },
     [](const uint32_t* v) { __PortGXFifo_u8(v[0]); __PortGXFifo_u32(v[1]); __PortGXFifo_u32(v[2]); }},
    {"begin", [](const uint32_t* v) { __PortGXFifo_u8u16(v[0], v[1]); },
     [](const uint32_t* v) { __PortGXFifo_u8(v[0]); __PortGXFifo_u16(v[1]); }},
    {"GXPosition3f32", [](const uint32_t* v) { GXPosition3f32(asFloat(v[0]), asFloat(v[1]), asFloat(v[2])); },
     [](const uint32_t* v) { __PortGXFifo_f32(asFloat(v[0])); __PortGXFifo_f32(asFloat(v[1])); __PortGXFifo_f32(asFloat(v[2])); }},
    {"GXPosition2f32", [](const uint32_t* v) { GXPosition2f32(asFloat(v[0]), asFloat(v[1])); },
     [](const uint32_t* v) { __PortGXFifo_f32(asFloat(v[0])); __PortGXFifo_f32(asFloat(v[1])); }},
    {"GXPosition3s16", [](const uint32_t* v) { GXPosition3s16(v[0], v[1], v[2]); },
     [](const uint32_t* v) { __PortGXFifo_s16(v[0]); __PortGXFifo_s16(v[1]); __PortGXFifo_s16(v[2]); }},
    {"GXPosition3u8", [](const uint32_t* v) { GXPosition3u8(v[0], v[1], v[2]); },
     [](const uint32_t* v) { __PortGXFifo_u8(v[0]); __PortGXFifo_u8(v[1]); __PortGXFifo_u8(v[2]); }},
    {"GXPosition3s8", [](const uint32_t* v) { GXPosition3s8(v[0], v[1], v[2]); },
     [](const uint32_t* v) { __PortGXFifo_s8(v[0]); __PortGXFifo_s8(v[1]); __PortGXFifo_s8(v[2]); }},
    {"GXNormal3f32", [](const uint32_t* v) { GXNormal3f32(asFloat(v[0]), asFloat(v[1]), asFloat(v[2])); },
     [](const uint32_t* v) { __PortGXFifo_f32(asFloat(v[0])); __PortGXFifo_f32(asFloat(v[1])); __PortGXFifo_f32(asFloat(v[2])); }},
    {"GXColor4u8", [](const uint32_t* v) { GXColor4u8(v[0], v[1], v[2], v[3]); },
     [](const uint32_t* v) { __PortGXFifo_u8(v[0]); __PortGXFifo_u8(v[1]); __PortGXFifo_u8(v[2]); __PortGXFifo_u8(v[3]); }},
    {"GXTexCoord2s16", [](const uint32_t* v) { GXTexCoord2s16(v[0], v[1]); },
     [](const uint32_t* v) { __PortGXFifo_s16(v[0]); __PortGXFifo_s16(v[1]); }},
    {"GXTexCoord2u16", [](const uint32_t* v) { GXTexCoord2u16(v[0], v[1]); },
     [](const uint32_t* v) { __PortGXFifo_u16(v[0]); __PortGXFifo_u16(v[1]); }},
    {"GXTexCoord2f32", [](const uint32_t* v) { GXTexCoord2f32(asFloat(v[0]), asFloat(v[1])); },
     [](const uint32_t* v) { __PortGXFifo_f32(asFloat(v[0])); __PortGXFifo_f32(asFloat(v[1])); }},
    {"GXTexCoord2u8", [](const uint32_t* v) { GXTexCoord2u8(v[0], v[1]); },
     [](const uint32_t* v) { __PortGXFifo_u8(v[0]); __PortGXFifo_u8(v[1]); }},
};

static uint32_t sState = 0x13579bdf;
static uint32_t next() { return sState = sState * 1664525 + 1013904223; }

int main() {
    unsigned compared = 0, fast = 0;
    for (const Case& c : kCases) {
        for (int room = -6; room <= 24; ++room) {
            for (unsigned mode = 0; mode < 5; ++mode) {
                for (int k = 0; k < 40; ++k) {
                    uint32_t v[4];
                    for (uint32_t& x : v) x = next();
                    if (k == 0) v[0] = v[1] = v[2] = v[3] = 0;
                    if (k == 1) v[0] = v[1] = v[2] = v[3] = 0xFFFFFFFFu;
                    if (k == 2) v[0] = v[1] = v[2] = v[3] = 0x80000000u;  // -0.0f
                    if (k == 3) v[0] = v[1] = v[2] = v[3] = 0x7FC00001u;  // a NaN payload
                    const Result a = run(c.real, v, room, mode), b = run(c.ref, v, room, mode);
                    if (memcmp(a.first, b.first, sizeof a.first) || memcmp(a.second, b.second, sizeof a.second) ||
                        memcmp(a.sink, b.sink, sizeof a.sink) || a.end != b.end || a.calls != b.calls) {
                        fprintf(stderr, "%s: room %d, overflow mode %u, values %08x %08x %08x %08x: different (end %ld / %ld, %zu / %zu calls)\n",
                                c.name, room, mode, v[0], v[1], v[2], v[3], a.end, b.end, a.calls.size(), b.calls.size());
                        return 1;
                    }
                    ++compared;
                    if (b.calls.empty()) ++fast;
                }
            }
        }
    }
    printf("gx fifo writes: %u writes compared (%u without an overflow), 0 different\n", compared, fast);
    return 0;
}
