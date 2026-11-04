// Replacement for the Broadway write-gather pipe (0xCC008000).
//
// The decompiled GX library builds exactly the command stream the real GPU would
// receive.  On the port, every FIFO write lands in a host buffer (big-endian, like
// the hardware stream and like display lists in game data) which the port's GPU
// command processor consumes at synchronisation points.
#pragma once

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t* __PortGXFifoPtr;
extern uint8_t* __PortGXFifoLimit;  // write position that triggers a flush (leaves slack)
void __PortGXFifoOverflow(void);
#if defined(TARGET_PC)
// Returns nonzero only when the complete XF payload was written.
int __PortGXFifoXfBurst12(const float* source);
#endif

static inline void __PortGXFifoCheck(void) {
    if (__PortGXFifoPtr >= __PortGXFifoLimit) {
        __PortGXFifoOverflow();
    }
}

static inline void __PortGXFifo_u8(uint8_t v) {
    *__PortGXFifoPtr++ = v;
    __PortGXFifoCheck();
}
static inline void __PortGXFifo_s8(int8_t v) { __PortGXFifo_u8((uint8_t)v); }

static inline void __PortGXFifo_u16(uint16_t v) {
    uint8_t* p = __PortGXFifoPtr;
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
    __PortGXFifoPtr = p + 2;
    __PortGXFifoCheck();
}
static inline void __PortGXFifo_s16(int16_t v) { __PortGXFifo_u16((uint16_t)v); }

static inline void __PortGXFifo_u32(uint32_t v) {
    v = __builtin_bswap32(v);
    memcpy(__PortGXFifoPtr, &v, 4);
    __PortGXFifoPtr += 4;
    __PortGXFifoCheck();
}
static inline void __PortGXFifo_s32(int32_t v) { __PortGXFifo_u32((uint32_t)v); }

static inline void __PortGXFifo_f32(float f) {
    uint32_t v;
    memcpy(&v, &f, 4);
    __PortGXFifo_u32(v);
}

// A command or a vertex's fields written with one limit check: when all n bytes fit before the limit, the field-by-field
// writes above would not reach it before the last byte either, so one check after it calls the overflow at the same write.
// When they do not fit, the caller writes field by field. The bytes are the same either way.
static inline int __PortGXFifoFits(const uint8_t* p, unsigned n) {
    return (intptr_t)((uintptr_t)__PortGXFifoLimit - (uintptr_t)p) >= (intptr_t)n;
}
static inline void __PortGXFifoEnd(uint8_t* end) {
    __PortGXFifoPtr = end;
    __PortGXFifoCheck();
}
static inline void __PortGXFifoPut_u8(uint8_t* p, uint8_t v) { p[0] = v; }
static inline void __PortGXFifoPut_s8(uint8_t* p, int8_t v) { p[0] = (uint8_t)v; }
static inline void __PortGXFifoPut_u16(uint8_t* p, uint16_t v) {
    v = __builtin_bswap16(v);
    memcpy(p, &v, 2);
}
static inline void __PortGXFifoPut_s16(uint8_t* p, int16_t v) { __PortGXFifoPut_u16(p, (uint16_t)v); }
static inline void __PortGXFifoPut_u32(uint8_t* p, uint32_t v) {
    v = __builtin_bswap32(v);
    memcpy(p, &v, 4);
}
static inline void __PortGXFifoPut_s32(uint8_t* p, int32_t v) { __PortGXFifoPut_u32(p, (uint32_t)v); }
static inline void __PortGXFifoPut_f32(uint8_t* p, float f) {
    uint32_t v;
    memcpy(&v, &f, 4);
    __PortGXFifoPut_u32(p, v);
}

// An opcode and one word (a BP register), an opcode, a byte and a word (a CP register), an opcode and two words (an XF
// register, a display-list call), an opcode and a count (a primitive). The field-by-field form is out of line: it is the
// rare case, and the one-check form stays small enough to inline everywhere.
#define __PORT_GX_FIFO_COLD __attribute__((noinline, cold))
#define __PORT_GX_FIFO_HOT __attribute__((always_inline))
static __PORT_GX_FIFO_COLD void __PortGXFifoFields_u8u32(uint8_t a, uint32_t b) {
    __PortGXFifo_u8(a);
    __PortGXFifo_u32(b);
}
static inline __PORT_GX_FIFO_HOT void __PortGXFifo_u8u32(uint8_t a, uint32_t b) {
    uint8_t* p = __PortGXFifoPtr;
    if (__builtin_expect(__PortGXFifoFits(p, 5), 1)) {
        __PortGXFifoPut_u8(p, a);
        __PortGXFifoPut_u32(p + 1, b);
        __PortGXFifoEnd(p + 5);
    } else {
        __PortGXFifoFields_u8u32(a, b);
    }
}
static __PORT_GX_FIFO_COLD void __PortGXFifoFields_u8u8u32(uint8_t a, uint8_t b, uint32_t c) {
    __PortGXFifo_u8(a);
    __PortGXFifo_u8(b);
    __PortGXFifo_u32(c);
}
static inline __PORT_GX_FIFO_HOT void __PortGXFifo_u8u8u32(uint8_t a, uint8_t b, uint32_t c) {
    uint8_t* p = __PortGXFifoPtr;
    if (__builtin_expect(__PortGXFifoFits(p, 6), 1)) {
        __PortGXFifoPut_u8(p, a);
        __PortGXFifoPut_u8(p + 1, b);
        __PortGXFifoPut_u32(p + 2, c);
        __PortGXFifoEnd(p + 6);
    } else {
        __PortGXFifoFields_u8u8u32(a, b, c);
    }
}
static __PORT_GX_FIFO_COLD void __PortGXFifoFields_u8u32u32(uint8_t a, uint32_t b, uint32_t c) {
    __PortGXFifo_u8(a);
    __PortGXFifo_u32(b);
    __PortGXFifo_u32(c);
}
static inline __PORT_GX_FIFO_HOT void __PortGXFifo_u8u32u32(uint8_t a, uint32_t b, uint32_t c) {
    uint8_t* p = __PortGXFifoPtr;
    if (__builtin_expect(__PortGXFifoFits(p, 9), 1)) {
        __PortGXFifoPut_u8(p, a);
        __PortGXFifoPut_u32(p + 1, b);
        __PortGXFifoPut_u32(p + 5, c);
        __PortGXFifoEnd(p + 9);
    } else {
        __PortGXFifoFields_u8u32u32(a, b, c);
    }
}
static __PORT_GX_FIFO_COLD void __PortGXFifoFields_u8u16(uint8_t a, uint16_t b) {
    __PortGXFifo_u8(a);
    __PortGXFifo_u16(b);
}
static inline __PORT_GX_FIFO_HOT void __PortGXFifo_u8u16(uint8_t a, uint16_t b) {
    uint8_t* p = __PortGXFifoPtr;
    if (__builtin_expect(__PortGXFifoFits(p, 3), 1)) {
        __PortGXFifoPut_u8(p, a);
        __PortGXFifoPut_u16(p + 1, b);
        __PortGXFifoEnd(p + 3);
    } else {
        __PortGXFifoFields_u8u16(a, b);
    }
}

static inline void __PortGXFifo_u64(uint64_t v) {
    __PortGXFifo_u32((uint32_t)(v >> 32));
    __PortGXFifo_u32((uint32_t)v);
}
static inline void __PortGXFifo_s64(int64_t v) { __PortGXFifo_u64((uint64_t)v); }
static inline void __PortGXFifo_f64(double d) {
    uint64_t v;
    memcpy(&v, &d, 8);
    __PortGXFifo_u64(v);
}

#ifdef __cplusplus
}
#endif
