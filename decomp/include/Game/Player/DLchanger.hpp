#pragma once

#include <JSystem/JKernel/JKRHeap.hpp>
#include <revolution/types.h>

class J3DModelX;

#ifdef TARGET_PC
extern "C" int GXPortSwapTurn(u32* stamp, int site);  // (decomp/src/RVL_SDK/gx/GXMisc.c)
#endif

struct DLholder {
    /* 0x00 */ u8* mDL;
    /* 0x04 */ u16 mSize;
    /* 0x06 */ u16 _6;
};

class DLchanger {
public:
    DLchanger(u8 count, u32 size) {
        mBuffers = new DLholder[count];
        mBufferCount = count;
        mCurrentBuffer = 0;
        for (u32 i = 0; i < mBufferCount; i++) {
            mBuffers[i].mDL = new (32) u8[size];
        }
#ifdef TARGET_PC
        mPortPicture = 0;
        mPortSwap = true;
#endif
    }

#ifdef TARGET_PC
    // (PETARI_EARLY_DONE: whether the next swap moves on, decided before the caller's scheduler lock, where a wait could not
    // end: a list written while no picture has begun since the last one goes into the same buffer, as J3DModel::viewCalc)
    void portTurn() { mPortSwap = GXPortSwapTurn(&mPortPicture, 8); }
#endif

    void addDL(J3DModelX*);

    DLholder* swap() {
#ifdef TARGET_PC
        if (!mPortSwap) {
            return &mBuffers[mCurrentBuffer];
        }
#endif
        mCurrentBuffer = (mCurrentBuffer + 1) % mBufferCount;
        return &mBuffers[mCurrentBuffer];
    }

    /* 0x00 */ DLholder* mBuffers;
    /* 0x04 */ u8 mBufferCount;
    /* 0x05 */ u8 mCurrentBuffer;
#ifdef TARGET_PC
    u32 mPortPicture;
    bool mPortSwap;
#endif
};
