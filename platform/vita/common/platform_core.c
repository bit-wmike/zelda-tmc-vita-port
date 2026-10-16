/*
 * Vita implementation of the platform API the shared 3DS-port sources call
 * (Platform3DS_*). The names stay as they are so the engine/port code can be
 * shared unmodified; everything here sits on top of tmc_platform.h.
 */
#include "platform_3ds.h"
#include "tmc_platform.h"
#include "tmc_diag.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static TmcInputState sInput;
static uint32_t sHeld;
static uint32_t sDown;
static uint32_t sPrevHeld;
static unsigned sTurboMultiplier = 3;
static unsigned sTurboPhase;
static bool sQuit;

extern void TmcVita_OnInputPolled(const TmcInputState* input, uint32_t down);
extern void TmcPlatform_FrameHook(void);

static void PollInput(void) {
    TmcSys_PollInput(&sInput);
    sPrevHeld = sHeld;
    sHeld = sInput.held;
    sDown = sHeld & ~sPrevHeld;
    static int sLoggedPresses;
    if (sDown && sLoggedPresses < 20) {
        ++sLoggedPresses;
        TmcDiag_Logf("[input] down=0x%08x stick=%.2f,%.2f touch=%d\n", (unsigned)sDown, (double)sInput.stickX,
                     (double)sInput.stickY, (int)sInput.touchActive);
    }
    if (TmcSys_ShouldQuit())
        sQuit = true;
    TmcVita_OnInputPolled(&sInput, sDown);
    TmcPlatform_FrameHook();
}

/* ---- lifecycle ---- */
int Platform3DS_Init(void) { return 1; }
void Platform3DS_Shutdown(void) {}
bool Platform3DS_IsRunning(void) { return !sQuit; }
void TmcVita_RequestQuit(void) { sQuit = true; }
extern bool Port_Config_VitaRenderThreads(void);
bool Platform3DS_IsNew3DS(void) { return getenv("TMC_NOTHREADS") == NULL && Port_Config_VitaRenderThreads(); }
bool Platform3DS_CanUseCore1(void) { return getenv("TMC_NOTHREADS") == NULL && Port_Config_VitaRenderThreads(); }
unsigned Platform3DS_Core1TimeLimit(void) { return 100; }
void Platform3DS_ShowSplash(void) {}
void Platform3DS_EnterGameplayDisplay(void) {}

/* ---- input ---- */
uint16_t TmcVita_GbaKeysFromMask(uint32_t keys);
uint32_t TmcVita_FilterGameKeys(uint32_t held);

uint16_t TmcVita_GbaKeysFromMask(uint32_t keys) {
    uint16_t input = 0x03ff;
    if (keys & TMC_KEY_A) input &= ~(1u << 0);
    if (keys & TMC_KEY_B) input &= ~(1u << 1);
    if (keys & TMC_KEY_SELECT) input &= ~(1u << 2);
    if (keys & TMC_KEY_START) input &= ~(1u << 3);
    if (keys & TMC_KEY_RIGHT) input &= ~(1u << 4);
    if (keys & TMC_KEY_LEFT) input &= ~(1u << 5);
    if (keys & TMC_KEY_UP) input &= ~(1u << 6);
    if (keys & TMC_KEY_DOWN) input &= ~(1u << 7);
    if (keys & TMC_KEY_R) input &= ~(1u << 8);
    if (keys & TMC_KEY_L) input &= ~(1u << 9);
    return input;
}

uint16_t Platform3DS_ReadKeyInput(void) {
    uint16_t v = TmcVita_GbaKeysFromMask(TmcVita_FilterGameKeys(sHeld));
    if (v != 0x3ff && getenv("TMC_INPUT_DEBUG")) {
        char line[64];
        snprintf(line, sizeof(line), "[input] keyinput=%03x\n", v);
        TmcSys_Log(line);
    }
    return v;
}
uint16_t Platform3DS_ReadKeyDownInput(void) { return TmcVita_GbaKeysFromMask(TmcVita_FilterGameKeys(sDown)); }
uint32_t Platform3DS_KeysHeld(void) { return TmcVita_FilterGameKeys(sHeld); }
uint32_t TmcVita_RawKeysHeld(void) { return sHeld; }
uint32_t TmcVita_RawKeysDown(void) { return sDown; }
const TmcInputState* TmcVita_Input(void) { return &sInput; }

void Platform3DS_ReadCircle(float* x, float* y) {
    /* The shared analog code expects y up-positive, like the 3DS circle pad. */
    if (x) *x = sInput.stickX;
    if (y) *y = -sInput.stickY;
}

/* ---- time ---- */
uint64_t Platform3DS_Milliseconds(void) { return TmcSys_TimeUs() / 1000u; }
uint64_t Platform3DS_SystemTick(void) { return TmcSys_TimeUs(); }
uint64_t Platform3DS_TicksPerSecond(void) { return 1000000u; }
unsigned long long svcGetSystemTick(void) { return TmcSys_TimeUs(); }

/* ---- memory ---- */
int Platform3DS_IsNativeAddress(uintptr_t value) {
    /* Vita user memory (and the 32-bit Linux test host) never maps below
     * 0x0A000000, so everything outside GBA space is a real pointer. */
    return value >= 0x0A000000u;
}

int Platform3DS_IsActiveStackAddress(uintptr_t value) {
    volatile char marker;
    const uintptr_t sp = (uintptr_t)&marker;
    const uintptr_t window = 64u * 1024u;
    return value >= sp - window && value <= sp + window;
}

void* linearAlloc(size_t size) { return malloc(size); }
void linearFree(void* p) { free(p); }

bool Platform3DS_CleanDataCache(const void* addr, size_t size) {
    (void)addr;
    (void)size;
    return true;
}
const char* Platform3DS_CacheCleanPath(void) { return "none"; }

/* ---- frame pacing ----
 * Normal play presents every logic frame and waits for VSync. While turbo is
 * held, only every Nth frame is presented and the rest run unthrottled. */
bool Platform3DS_TurboHeld(void) { return (sHeld & TMC_KEY_TURBO) != 0; }
unsigned Platform3DS_TurboMultiplier(void) { return sTurboMultiplier; }
void Platform3DS_SetTurboMultiplier(unsigned m) { sTurboMultiplier = m < 2 ? 2 : (m > 8 ? 8 : m); }

extern bool TmcVita_GamePaused(void);

extern bool Port_Config_VitaHalfRate(void);
static unsigned sHalfRatePhase;

bool Platform3DS_BeginFrameBoundary(void) {
    if (!Platform3DS_TurboHeld()) {
        sTurboPhase = 0;
        /* 30 fps mode: the game still runs 60 logic frames per second; only
         * every second one is rendered and shown. The presenter waits two
         * display refreshes per shown frame, so the speed stays exact. */
        if (Port_Config_VitaHalfRate()) {
            sHalfRatePhase ^= 1u;
            return sHalfRatePhase == 0;
        }
        return true;
    }
    sTurboPhase = (sTurboPhase + 1) % sTurboMultiplier;
    return sTurboPhase == 0;
}

void Platform3DS_EndFrameBoundary(void) {}

void Platform3DS_WaitForVBlank(void) {
    /* The video backend already synchronised to the display while presenting;
     * this is where the next frame's input is sampled. */
    PollInput();
}

void Platform3DS_PumpWithoutVBlank(void) { PollInput(); }
void Platform3DS_MarkFrameDiscontinuity(Old3DSFramePacerDiscontinuity reason) { (void)reason; }

/* ---- diagnostics ---- */
void Platform3DS_SetStage(uint32_t stage) { (void)stage; }
void Platform3DS_Heartbeat(void) {}
void Platform3DS_WatchdogPoll(void) {}
void Platform3DS_RequestQuickDump(void) {}
uint64_t Platform3DS_PumpTicks(void) { return 0; }
uint64_t Platform3DS_PumpMaxTicks(void) { return 0; }
uint64_t Platform3DS_PostWaitTicks(void) { return 0; }
uint64_t Platform3DS_PostWaitMaxTicks(void) { return 0; }
void Platform3DS_GetRuntimeStats(Platform3DSRuntimeStats* stats) {
    if (stats) memset(stats, 0, sizeof(*stats));
}
bool Platform3DS_SubmitBottomWorker(void) { return false; }
bool Platform3DS_TryFinishBottomWorker(void) { return false; }
void Platform3DS_ShutdownBottomWorker(void) {}
uint16_t* Platform3DS_GetFramebuffer(int top, uint16_t* width, uint16_t* height) {
    (void)top;
    if (width) *width = 0;
    if (height) *height = 0;
    return NULL;
}

void Platform3DS_Debug(const char* message) { TmcSys_Log(message); }
void Platform3DS_ShowFatal(const char* title, const char* message) { TmcSys_Fatal(title, message); }

void TmcPlatform_PinCurrentThread(int core) {
    TmcSys_PinThread(core);
    TmcDiag_Logf("[thread] renderer worker started on core %d\n", core);
}

/* ---- second-screen (overlay) synchronisation ----
 * The overlay is painted on the main thread on the Vita, but the shared code
 * still brackets its state with these locks. */
static pthread_mutex_t sUiLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t sSnapLock = PTHREAD_MUTEX_INITIALIZER;
void Port_SecondScreen_3DS_SyncInit(void) {}
void Port_SecondScreen_3DS_LockUI(void) { pthread_mutex_lock(&sUiLock); }
void Port_SecondScreen_3DS_UnlockUI(void) { pthread_mutex_unlock(&sUiLock); }
void Port_SecondScreen_3DS_LockSnapshot(void) { pthread_mutex_lock(&sSnapLock); }
void Port_SecondScreen_3DS_UnlockSnapshot(void) { pthread_mutex_unlock(&sSnapLock); }

#include <unistd.h>
int TmcVita_Fsync(int fd) {
    static int sLogged;
    const int rc = fsync(fd);
    if (rc != 0 && !sLogged) {
        sLogged = 1;
        TmcDiag_Logf("[io] fsync not supported here (rc=%d), continuing without it\n", rc);
    }
    return 0;
}
