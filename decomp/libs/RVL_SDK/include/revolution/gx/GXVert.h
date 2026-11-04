#ifndef GXVERT_H
#define GXVERT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "revolution/base/PPCWGPipe.h"

#ifdef __MWERKS__
volatile PPCWGPipe GXWGFifo : 0xCC008000;
#define __GXFIFO_WRITE(td, v) GXWGFifo.td = (td)(v)
#else
#include "port/gx_fifo.h"
#define __GXFIFO_WRITE(td, v) __PortGXFifo_##td((td)(v))
#endif

#define __GXCDEF(prfx, n, t) __GXCDEF##n(prfx##n##t, t, t)
#define __GXCDEFX(func, n, t) __GXCDEF##n(func, t, t)

#define __GXCDEF1(func, ts, td)                                                                                                                      \
    static void func(const ts x) {                                                                                                                   \
        __GXFIFO_WRITE(td, x);                                                                                                                         \
        return;                                                                                                                                      \
    }

#ifdef __MWERKS__
#define __GXCDEF2(func, ts, td)                                                                                                                      \
    static void func(const ts x, const ts y) {                                                                                                       \
        __GXFIFO_WRITE(td, x);                                                                                                                         \
        __GXFIFO_WRITE(td, y);                                                                                                                         \
        return;                                                                                                                                      \
    }

#define __GXCDEF3(func, ts, td)                                                                                                                      \
    static void func(const ts x, const ts y, const ts z) {                                                                                           \
        __GXFIFO_WRITE(td, x);                                                                                                                         \
        __GXFIFO_WRITE(td, y);                                                                                                                         \
        __GXFIFO_WRITE(td, z);                                                                                                                         \
        return;                                                                                                                                      \
    }

#define __GXCDEF4(func, ts, td)                                                                                                                      \
    static void func(const ts x, const ts y, const ts z, const ts w) {                                                                               \
        __GXFIFO_WRITE(td, x);                                                                                                                         \
        __GXFIFO_WRITE(td, y);                                                                                                                         \
        __GXFIFO_WRITE(td, z);                                                                                                                         \
        __GXFIFO_WRITE(td, w);                                                                                                                         \
        return;                                                                                                                                      \
    }
#else
// One limit check for the whole call when it fits (port/gx_fifo.h: __PortGXFifoFits); field by field out of line otherwise.
#define __GXCDEF2(func, ts, td)                                                                                                                     \
    static __PORT_GX_FIFO_COLD void func##Fields(const ts x, const ts y) {                                                                          \
        __GXFIFO_WRITE(td, x);                                                                                                                      \
        __GXFIFO_WRITE(td, y);                                                                                                                      \
    }                                                                                                                                               \
    static inline __PORT_GX_FIFO_HOT void func(const ts x, const ts y) {                                                                            \
        uint8_t* p = __PortGXFifoPtr;                                                                                                               \
        if (__builtin_expect(__PortGXFifoFits(p, 2 * sizeof(td)), 1)) {                                                                             \
            __PortGXFifoPut_##td(p, (td)(x));                                                                                                       \
            __PortGXFifoPut_##td(p + sizeof(td), (td)(y));                                                                                          \
            __PortGXFifoEnd(p + 2 * sizeof(td));                                                                                                    \
        } else {                                                                                                                                    \
            func##Fields(x, y);                                                                                                                     \
        }                                                                                                                                           \
    }

#define __GXCDEF3(func, ts, td)                                                                                                                     \
    static __PORT_GX_FIFO_COLD void func##Fields(const ts x, const ts y, const ts z) {                                                              \
        __GXFIFO_WRITE(td, x);                                                                                                                      \
        __GXFIFO_WRITE(td, y);                                                                                                                      \
        __GXFIFO_WRITE(td, z);                                                                                                                      \
    }                                                                                                                                               \
    static inline __PORT_GX_FIFO_HOT void func(const ts x, const ts y, const ts z) {                                                                \
        uint8_t* p = __PortGXFifoPtr;                                                                                                               \
        if (__builtin_expect(__PortGXFifoFits(p, 3 * sizeof(td)), 1)) {                                                                             \
            __PortGXFifoPut_##td(p, (td)(x));                                                                                                       \
            __PortGXFifoPut_##td(p + sizeof(td), (td)(y));                                                                                          \
            __PortGXFifoPut_##td(p + 2 * sizeof(td), (td)(z));                                                                                      \
            __PortGXFifoEnd(p + 3 * sizeof(td));                                                                                                    \
        } else {                                                                                                                                    \
            func##Fields(x, y, z);                                                                                                                  \
        }                                                                                                                                           \
    }

#define __GXCDEF4(func, ts, td)                                                                                                                     \
    static __PORT_GX_FIFO_COLD void func##Fields(const ts x, const ts y, const ts z, const ts w) {                                                  \
        __GXFIFO_WRITE(td, x);                                                                                                                      \
        __GXFIFO_WRITE(td, y);                                                                                                                      \
        __GXFIFO_WRITE(td, z);                                                                                                                      \
        __GXFIFO_WRITE(td, w);                                                                                                                      \
    }                                                                                                                                               \
    static inline __PORT_GX_FIFO_HOT void func(const ts x, const ts y, const ts z, const ts w) {                                                    \
        uint8_t* p = __PortGXFifoPtr;                                                                                                               \
        if (__builtin_expect(__PortGXFifoFits(p, 4 * sizeof(td)), 1)) {                                                                             \
            __PortGXFifoPut_##td(p, (td)(x));                                                                                                       \
            __PortGXFifoPut_##td(p + sizeof(td), (td)(y));                                                                                          \
            __PortGXFifoPut_##td(p + 2 * sizeof(td), (td)(z));                                                                                      \
            __PortGXFifoPut_##td(p + 3 * sizeof(td), (td)(w));                                                                                      \
            __PortGXFifoEnd(p + 4 * sizeof(td));                                                                                                    \
        } else {                                                                                                                                    \
            func##Fields(x, y, z, w);                                                                                                               \
        }                                                                                                                                           \
    }
#endif

__GXCDEF(GXCmd, 1, u8)
__GXCDEF(GXCmd, 1, u16)
__GXCDEF(GXCmd, 1, u32)
__GXCDEF(GXCmd, 1, f32)

__GXCDEF(GXPosition, 3, f32)
__GXCDEF(GXPosition, 3, u8)
__GXCDEF(GXPosition, 3, s8)
__GXCDEF(GXPosition, 3, u16)
__GXCDEF(GXPosition, 3, s16)

__GXCDEF(GXPosition, 2, f32)
__GXCDEF(GXPosition, 2, u8)
__GXCDEF(GXPosition, 2, s8)
__GXCDEF(GXPosition, 2, u16)
__GXCDEF(GXPosition, 2, s16)

__GXCDEF(GXNormal, 3, f32)

__GXCDEF(GXColor, 1, u32)
__GXCDEF(GXColor, 4, u8)

__GXCDEF(GXTexCoord, 2, u8)
__GXCDEF(GXTexCoord, 2, u16)
__GXCDEF(GXTexCoord, 2, s16)
__GXCDEF(GXTexCoord, 2, f32)

__GXCDEFX(GXPosition1x8, 1, u8)
__GXCDEFX(GXNormal1x8, 1, u8)
__GXCDEFX(GXTexCoord1x8, 1, u8)

#ifdef __cplusplus
}
#endif

#endif  // GXVERT_H
