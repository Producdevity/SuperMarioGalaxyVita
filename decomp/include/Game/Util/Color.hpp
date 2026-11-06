#pragma once

#include <revolution/gx.h>
#include <revolution/types.h>

class Color8 {
public:
    Color8() : mColor(-1) {
    }

    Color8(GXColor color) {
        set(color);
    }

    Color8(u8 r, u8 g, u8 b, u8 a) {
        set(r, g, b, a);
    }

    // (a u32 colour is 0xRRGGBBAA, as JUtility::TColor's)
    Color8(u32 color) {
#if defined(__MWERKS__) || defined(__BIG_ENDIAN__)
        mColor = color;
#else
        set(u8(color >> 24), u8(color >> 16), u8(color >> 8), u8(color));
#endif
    }

    inline operator GXColor() const {
        return mGXColor;
    }

    inline operator u32() const {
#if defined(__MWERKS__) || defined(__BIG_ENDIAN__)
        return mColor;
#else
        return (u32(r) << 24) | (u32(g) << 16) | (u32(b) << 8) | u32(a);
#endif
    }

    inline const Color8& operator=(const GXColor& rOther) {
        mGXColor = rOther;
        return *this;
    }

    inline const Color8& operator=(const Color8& rOther) {
        r = rOther.r;
        g = rOther.g;
        b = rOther.b;
        a = rOther.a;
        return *this;
    }

    void set(u8 _r, u8 _g, u8 _b, u8 _a) {
        r = _r;
        g = _g;
        b = _b;
        a = _a;
    }

    void set(GXColor color) {
        GXColor* const pColor = &mGXColor;
        *pColor = color;
    }

    union {
        struct {
            u8 r;
            u8 g;
            u8 b;
            u8 a;
        };
        u32 mColor;
        GXColor mGXColor;
    };
};

class Color10 {
public:
    Color10() : r(0x3FF), g(0x3FF), b(0x3FF), a(0x3FF) {
    }

    operator GXColorS10() const {
        return mColor;
    }

    union {
        struct {
            u16 r;
            u16 g;
            u16 b;
            u16 a;
        };
        GXColorS10 mColor;
    };
};
