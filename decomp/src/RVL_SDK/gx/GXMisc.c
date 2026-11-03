#include "revolution/gx/GXEnum.h"
#include "port/game_progress.h"
#include "revolution/os/OSInterrupt.h"
#include "revolution/os/OSThread.h"
#include "revolution/gx.h"
#include "revolution/gx/GXTypes.h"
#include "revolution/gx/GXRegs.h"
#include "revolution/gx/shortcut_pe_misc.h"
#include "private/flipper.h"
#include "revolution/os.h"

static GXDrawSyncCallback TokenCB;
static GXDrawDoneCallback DrawDoneCB;
static volatile GXBool DrawDone;
static OSThreadQueue FinishQueue;
#ifdef TARGET_PC
// (port) Finish tokens written and their interrupts taken, in order: GXWaitDrawDone waits for all of them, and a wait for one
// picture (GXWaitPictureRead) for its own, while a later one may be on its way already (PETARI_EARLY_DONE).
static volatile u32 sFinishSet, sFinishDone;
// (GXV_RACECHECK: the guest write epoch when each token was set, for the front end; port_gx_token_epoch)
static uint64_t sTokenEpoch[64];
static u32 sTokenPublished;
extern uint64_t port_vgx_guest_write_epoch(void);
int port_gx_token_epoch(u32 token, uint64_t* epoch) {
    const u32 published = __atomic_load_n(&sTokenPublished, __ATOMIC_ACQUIRE);
    if (token == 0 || (s32)(token - published) > 0 || published - token >= 64) {
        return 0;
    }
    *epoch = sTokenEpoch[token & 63];
    return __atomic_load_n(&sTokenPublished, __ATOMIC_ACQUIRE) - token < 64;
}
#endif

void GXSetMisc(GXMiscToken token, u32 val) {
    switch (token) {
        case GX_MT_NULL:
            break;
        case GX_MT_XF_FLUSH:
            gx->vNum = val;
            gx->vNumNot = !gx->vNum;
            gx->bpSentNot = GX_TRUE;
            if (gx->vNum > 0) {
                gx->dirtyState |= 8;
            }
            break;
        case GX_MT_DL_SAVE_CONTEXT:
            gx->dlSaveContext = (val != 0);
            break;
        case GX_MT_ABORT_WAIT_COPYOUT:
            gx->abtWaitPECopy = (val != 0);
            break;
    }
}

void GXFlush(void) {
    if (gx->dirtyState) {
        __GXSetDirtyState();
    }

    GX_WRITE_U32(0);
    GX_WRITE_U32(0);
    GX_WRITE_U32(0);
    GX_WRITE_U32(0);
    GX_WRITE_U32(0);
    GX_WRITE_U32(0);
    GX_WRITE_U32(0);
    GX_WRITE_U32(0);
    PPCSync();
#ifndef __MWERKS__
    {
        extern void __PortGXFifoFlush(void);
        __PortGXFifoFlush();
    }
#endif
}

void GXResetWriteGatherPipe(void) {
    while (PPCMfwpar() & 0x1) {

    }

#ifdef __MWERKS__
    PPCMtwpar(OSUncachedToPhysical((void*)(0xC008000)));
#endif
}

static void __GXAbortWait(u32 clocks) {
    OSTime time0, time1;
    time0 = OSGetTime();

    do {
        time1 = OSGetTime();
    }
    while (time1 - time0 <= clocks / 4);
}

static void __GXAbortWaitPECopyDone(void) {
    u32 peCnt0, peCnt1;
    
    peCnt0 = GX_MEM_COUNTER_READ_U32(MEM_PE_REQCOUNT);
    do {
        peCnt1 = peCnt0;
        __GXAbortWait(32);
    
        peCnt0 = GX_MEM_COUNTER_READ_U32(MEM_PE_REQCOUNT);
    } while ( peCnt0 != peCnt1 );
} 

void __GXAbort(void) {
    if (gx->abtWaitPECopy && __GXIsGPFifoReady()) {
        __GXAbortWaitPECopyDone();
    }

    __PIRegs[0x18/4] = 1;
    __GXAbortWait(200);
    __PIRegs[0x18/4] = 0;
    __GXAbortWait(20);
}

void GXAbortFrame(void) {
    __GXAbort();

    if (__GXIsGPFifoReady()) {
        __GXCleanGPFifo(); 
        __GXInitRevisionBits();
        gx->dirtyState = 0;
        GXFlush();
    }
}

void GXSetDrawSync(u16 token) {
    BOOL en;
    u32 reg = 0;
    en = OSDisableInterrupts();
    reg = PE_TOKEN_INT(token, 0x48);
    GX_WRITE_RA_REG(reg);

    SC_PE_TOKEN_SET_TOKEN(reg, token);
    SC_PE_TOKEN_SET_RID(reg, 0x47);
    GX_WRITE_RA_REG(reg);
    GXFlush();
    OSRestoreInterrupts(en);
    gx->bpSentNot = GX_FALSE;
}

void GXSetDrawDone(void) {
    u32 reg;
    BOOL en = OSDisableInterrupts();
#if defined(__vita__)
    static unsigned reported = 0;
    const unsigned trace = reported < 4 ? ++reported : 0;
    if (trace) port_log("native-gx finish=%u flush-begin", trace);
#endif
    reg = PE_FINISH(2, 0x45);
    GX_WRITE_RA_REG(reg);
#ifdef TARGET_PC
    ++sFinishSet;
    sTokenEpoch[sFinishSet & 63] = port_vgx_guest_write_epoch();
    __atomic_store_n(&sTokenPublished, sFinishSet, __ATOMIC_RELEASE);
#endif
    PORT_GAME_PROGRESS(PORT_GAME_PHASE_GX_FINISH_FLUSH);
    GXFlush();
    DrawDone = GX_FALSE;
#if defined(__vita__)
    if (trace) port_log("native-gx finish=%u flush-complete", trace);
#endif
    PORT_GAME_PROGRESS(PORT_GAME_PHASE_GX_FINISH_RESTORE);
    OSRestoreInterrupts(en);
#if defined(__vita__)
    if (trace) port_log("native-gx finish=%u restore-complete done=%u", trace, (unsigned)DrawDone);
#endif
}

void GXWaitDrawDone(void) {
    PORT_GAME_PROGRESS(PORT_GAME_PHASE_GX_FINISH_WAIT);
    BOOL en = OSDisableInterrupts();
#ifdef TARGET_PC
    while (!DrawDone || (s32)(sFinishDone - sFinishSet) < 0) {
#else
    while (!DrawDone) {
#endif
        OSSleepThread(&FinishQueue);
    }
    OSRestoreInterrupts(en);
}

void GXDrawDone(void) {
    GXSetDrawDone();
    GXWaitDrawDone();
}

#ifdef TARGET_PC
// (port) A drawn frame's wait for the renderer, put off (PETARI_EARLY_DONE, MainLoopFramework::endFrame): later turns run while
// the renderer still reads the picture from the game's memory, and a write of something a picture reads first waits until
// that picture has been read to its end (GXWaitPictureRead): a display-list buffer the picture drew from (per buffer,
// J3DDisplayListObj); the matrix buffers of the view pass, a deformed model's vertex arrays and fur's, for the last picture
// whose end is written (GXWaitOwedDrawDone: the one before the picture drawn in this turn); everything before a scene change
// or a fast-forwarded cutscene.
static u32 sPicture;            // pictures begun (GXPortPictureBegin, before each drawn frame)
static u32 sPictureEnded;       // the last of them whose end token is written
static u32 sPictureToken[16];   // the token count after each picture's end token (by picture & 15)
extern void port_early_done_paid(int site, int64_t ns);
extern int port_os_may_wait(void);
u32 GXPortPictureBegin(void) { return ++sPicture; }
u32 GXPortPicture(void) { return sPicture; }
void GXSetDrawDoneOwed(void) {
    GXSetDrawDone();
    sPictureToken[sPicture & 15] = sFinishSet;
    sPictureEnded = sPicture;
    port_early_done_paid(-1, 0);
}
// 1 once picture `picture` has been read to its end (0, and a write that could not wait counted, when it was not: the picture
// is the one still being drawn, or the caller may not sleep).
int GXWaitPictureRead(u32 picture, int site) {
    if (picture == 0 || sPictureEnded == 0 || (s32)(sPictureEnded - picture) >= 16) {
        return 1;  // (none, PETARI_EARLY_DONE off, or long done: the ring holds the last 16)
    }
    if ((s32)(picture - sPictureEnded) > 0) {
        port_early_done_paid(5, 0);
        return 0;
    }
    const u32 token = sPictureToken[picture & 15];
    if ((s32)(sFinishDone - token) >= 0) {
        return 1;
    }
    if (!port_os_may_wait()) {
        port_early_done_paid(5, 0);
        return 0;
    }
    const int64_t start = port_host_time_ns();
    BOOL en = OSDisableInterrupts();
    while ((s32)(sFinishDone - token) < 0) {
        OSSleepThread(&FinishQueue);
    }
    OSRestoreInterrupts(en);
    port_early_done_paid(site, port_host_time_ns() - start);
    return 1;
}
void GXWaitOwedDrawDone(int site) { GXWaitPictureRead(sPictureEnded, site); }
// The same without waiting: 1 once picture `picture` has been read to its end.
int GXPictureRead(u32 picture) {
    if (picture == 0 || sPictureEnded == 0 || (s32)(sPictureEnded - picture) >= 16) {
        return 1;
    }
    return (s32)(picture - sPictureEnded) <= 0 && (s32)(sFinishDone - sPictureToken[picture & 15]) >= 0;
}
// 1 while a picture is being drawn or one whose end is written is still being read (PETARI_EARLY_DONE on).
int GXPortPicturesUnread(void) { return sPictureEnded != 0 && (sPicture != sPictureEnded || !GXPictureRead(sPictureEnded)); }
#endif

void GXPixModeSync(void) {
    GX_WRITE_RA_REG(gx->peCtrl);
    gx->bpSentNot = GX_FALSE;
}

void GXTexModeSync(void) {
    u32 reg = TX_LOADBLOCK3(0, 0, 0x63);
    GX_WRITE_RA_REG(reg);
    gx->bpSentNot = GX_FALSE;
}

void GXPokeAlphaMode(GXCompare func, u8 threshold) {
    u32 reg = (u32)(func << 8 | threshold);
    GX_PE_REG_WRITE_U16(3, reg);
}

void GXPokeAlphaRead(GXAlphaReadMode mode) {
    u32 reg = 0;
    SC_PE_PI_CTL_SET_AFMT(reg, mode);
    SC_PE_PI_CTL_SET_ZFMT(reg, 1);
    GX_PE_REG_WRITE_U16(4, reg);
}

void GXPokeAlphaUpdate(GXBool update_en) {
    u32 reg = GX_PE_REG_READ_U16(1);
    SC_PE_CMODE0_SET_ALPHA_MASK(reg, update_en);
    GX_PE_REG_WRITE_U16(1, reg);
}

void GXPokeBlendMode(GXBlendMode type, GXBlendFactor src, GXBlendFactor dst, GXLogicOp op) {
    u32 reg = GX_PE_REG_READ_U16(1);
    SC_PE_CMODE0_SET_BLEND_ENABLE(reg, ((type == GX_BM_BLEND) || (type == GX_BM_SUBTRACT)));
    SC_PE_CMODE0_SET_BLENDOP(reg, (type == GX_BM_SUBTRACT));
    SC_PE_CMODE0_SET_LOGICOP_ENABLE(reg, (type == GX_BM_LOGIC));
    SC_PE_CMODE0_SET_LOGICOP(reg, op);
    SC_PE_CMODE0_SET_SFACTOR(reg, src);
    SC_PE_CMODE0_SET_DFACTOR(reg, dst);
    SC_PE_CMODE0_SET_RID(reg, 0x41);
    GX_PE_REG_WRITE_U16(1, reg);
}

void GXPokeColorUpdate(GXBool update_en) {
    u32 reg = GX_PE_REG_READ_U16(1);
    SC_PE_CMODE0_SET_COLOR_MASK(reg, update_en);
    GX_PE_REG_WRITE_U16(1, reg);
}

void GXPokeDstAlpha(GXBool enable, u8 alpha) {
    u32 reg = 0;
    SC_PE_CMODE1_SET_CONSTANT_ALPHA(reg, alpha);
    SC_PE_CMODE1_SET_CONSTANT_ALPHA_ENABLE(reg, enable);
    GX_PE_REG_WRITE_U16(2, reg);
}

void GXPokeDither(GXBool dither) {
    u32 reg = GX_PE_REG_READ_U16(1);
    SC_PE_CMODE0_SET_DITHER_ENABLE(reg, dither);
    GX_PE_REG_WRITE_U16(1, reg);
}

void GXPokeZMode(GXBool compare_en, GXCompare func, GXBool update_en) {
    u32 reg = 0;
    SC_PE_ZMODE_SET_ENABLE(reg, compare_en);
    SC_PE_ZMODE_SET_FUNC(reg, func);
    SC_PE_ZMODE_SET_MASK(reg, update_en);
    GX_PE_REG_WRITE_U16(0, reg);
}

#ifndef __MWERKS__
u32 __PortGXPeekARGB(u16 x, u16 y);
u32 __PortGXPeekZ(u16 x, u16 y);

void GXPeekARGB(u16 x, u16 y, u32* color) {
    *color = __PortGXPeekARGB(x, y);
}

void GXPeekZ(u16 x, u16 y, u32* z) {
    *z = __PortGXPeekZ(x, y);
}
#else
void GXPeekARGB(u16 x, u16 y, u32* color) {
    u32 addr = (u32) OSPhysicalToUncached(0x8000000);
    SC_PE_PI_EFB_ADDR_SET_X(addr, x);
    SC_PE_PI_EFB_ADDR_SET_Y(addr, y);
    SC_PE_PI_EFB_ADDR_SET_TYPE(addr,  0);
    *color = (*(u32 *)addr);
}

void GXPeekZ(u16 x, u16 y, u32* z)  {
    u32 addr = (u32) OSPhysicalToUncached(0x8000000);
    SC_PE_PI_EFB_ADDR_SET_X(addr, x);
    SC_PE_PI_EFB_ADDR_SET_Y(addr, y);
    SC_PE_PI_EFB_ADDR_SET_TYPE(addr,  1);
    *z = *(u32*)addr;
}
#endif

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb) { 
    GXDrawSyncCallback oldcb = TokenCB;
    BOOL en = OSDisableInterrupts();
    TokenCB = cb;
    OSRestoreInterrupts(en);
    return oldcb;
}

static void GXTokenInterruptHandler(__OSInterrupt, OSContext* context)
{
    u16 token;
    OSContext exceptionContext;
    u32 reg;
    
    token = (u16)GX_PE_REG_READ_U16(7);
    if (TokenCB) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        (*TokenCB)(token);
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);        
    }

    reg = GX_PE_REG_READ_U16(5);
    SC_PE_INTRCTL_SET_INT0CLR(reg, 1);
    GX_PE_REG_WRITE_U16(5, reg);
}

GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback cb) {
    GXDrawDoneCallback oldcb = DrawDoneCB;
    BOOL en = OSDisableInterrupts();
    DrawDoneCB = cb;
    OSRestoreInterrupts(en);
    return oldcb;
}

static void
GXFinishInterruptHandler(__OSInterrupt, OSContext* context) {
    OSContext exceptionContext;
    u32 reg = 0;
    
    reg = GX_PE_REG_READ_U16(5);
    SC_PE_INTRCTL_SET_INT1CLR(reg, 1);
    GX_PE_REG_WRITE_U16(5, reg);

    DrawDone = GX_TRUE;
#ifdef TARGET_PC
    ++sFinishDone;
#endif
#if defined(__vita__)
    static unsigned reported = 0;
    if (reported < 4) port_log("native-gx finish-interrupt=%u", ++reported);
#endif

    if (DrawDoneCB)
    {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);
        (*DrawDoneCB)();
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);        
    }

    OSWakeupThread(&FinishQueue);
}

void __GXPEInit (void) {
    u32 reg;
    __OSSetInterruptHandler(__OS_INTERRUPT_PI_PE_TOKEN,  GXTokenInterruptHandler);
    __OSSetInterruptHandler(__OS_INTERRUPT_PI_PE_FINISH, GXFinishInterruptHandler);

    OSInitThreadQueue(&FinishQueue);

    __OSUnmaskInterrupts(OS_INTERRUPTMASK_PI_PE_TOKEN);
    __OSUnmaskInterrupts(OS_INTERRUPTMASK_PI_PE_FINISH);    

    reg = GX_PE_REG_READ_U16(5);
    SC_PE_INTRCTL_SET_INT0CLR(reg, 1);
    SC_PE_INTRCTL_SET_INT1CLR(reg, 1);
    SC_PE_INTRCTL_SET_INT0EN(reg, 1);
    SC_PE_INTRCTL_SET_INT1EN(reg, 1);
    GX_PE_REG_WRITE_U16(5, reg);
}
