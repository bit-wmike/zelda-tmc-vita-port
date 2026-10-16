/*
 * PS Vita implementation of tmc_platform.h: time, input, audio, threads.
 */
#include "tmc_platform.h"

#include <psp2/audioout.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/touch.h>
#include <psp2/rtc.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Heap for newlib (ROM 16 MB + engine + audio + textures stay well below). */
int _newlib_heap_size_user = 192 * 1024 * 1024;

static int sAudioPort = -1;
static int sAudioFrames;
static FILE* sLog;

uint64_t TmcSys_TimeUs(void) { return sceKernelGetProcessTimeWide(); }

void TmcVitaSys_Init(void) {
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);
    sceTouchEnableTouchForce(SCE_TOUCH_PORT_FRONT);
}

void TmcVitaSys_OpenLog(const char* path) {
    /* Route the shared port's stderr diagnostics into the log file too. */
    sLog = freopen(path, "w", stderr);
    if (sLog) setvbuf(sLog, NULL, _IOLBF, 0);
}

/* Rear touchpad: the left/right halves are two extra item buttons. To avoid
 * accidental presses from the hands holding the Vita, a half only counts
 * while a finger stays inside its outer third (the grip area is excluded)
 * and it is opt-in from the options menu. */
extern bool TmcVita_RearTouchEnabled(void);

static float StickAxis(int raw) {
    float v = ((float)raw - 128.0f) / 127.0f;
    if (fabsf(v) < 0.20f) return 0.0f;
    if (v > 1.0f) v = 1.0f;
    if (v < -1.0f) v = -1.0f;
    return v;
}

void TmcSys_PollInput(TmcInputState* out) {
    memset(out, 0, sizeof(*out));
    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));
    sceCtrlPeekBufferPositive(0, &pad, 1);
    const unsigned b = pad.buttons;
    uint32_t h = 0;
    if (b & SCE_CTRL_CROSS) h |= TMC_KEY_A;
    if (b & SCE_CTRL_CIRCLE) h |= TMC_KEY_B;
    if (b & SCE_CTRL_TRIANGLE) h |= TMC_KEY_SLOT0;
    if (b & SCE_CTRL_SQUARE) h |= TMC_KEY_SLOT1;
    if (b & SCE_CTRL_LTRIGGER) h |= TMC_KEY_L;
    if (b & SCE_CTRL_RTRIGGER) h |= TMC_KEY_R;
    if (b & SCE_CTRL_START) h |= TMC_KEY_START;
    if (b & SCE_CTRL_SELECT) h |= TMC_KEY_SELECT;
    if (b & SCE_CTRL_UP) h |= TMC_KEY_UP;
    if (b & SCE_CTRL_DOWN) h |= TMC_KEY_DOWN;
    if (b & SCE_CTRL_LEFT) h |= TMC_KEY_LEFT;
    if (b & SCE_CTRL_RIGHT) h |= TMC_KEY_RIGHT;
    /* PS TV / DS3-DS4: the shoulder buttons L2/R2 and L3/R3 as extras. */
    if (b & SCE_CTRL_L2) h |= TMC_KEY_SLOT2;
    if (b & SCE_CTRL_R2) h |= TMC_KEY_SLOT3;
    if (b & SCE_CTRL_L3) h |= TMC_KEY_OVERLAY;
    if (b & SCE_CTRL_R3) h |= TMC_KEY_MENU;

    out->stickX = StickAxis(pad.lx);
    out->stickY = StickAxis(pad.ly);
    /* The left stick also drives the D-pad bits so menus work with it. */
    if (out->stickX > 0.5f) h |= TMC_KEY_RIGHT;
    if (out->stickX < -0.5f) h |= TMC_KEY_LEFT;
    if (out->stickY > 0.5f) h |= TMC_KEY_DOWN;
    if (out->stickY < -0.5f) h |= TMC_KEY_UP;

    /* Right stick: up = fast-forward (hold), down = map overlay,
     * left/right = extra item slots 3/4. */
    const float rx = StickAxis(pad.rx);
    const float ry = StickAxis(pad.ry);
    if (ry < -0.6f) h |= TMC_KEY_TURBO;
    if (ry > 0.6f) h |= TMC_KEY_OVERLAY;
    if (rx < -0.6f) h |= TMC_KEY_SLOT2;
    if (rx > 0.6f) h |= TMC_KEY_SLOT3;

    /* START + SELECT together opens the Vita menu (works on PS TV too). */
    if ((b & SCE_CTRL_START) && (b & SCE_CTRL_SELECT)) {
        h |= TMC_KEY_MENU;
        h &= ~(TMC_KEY_START | TMC_KEY_SELECT);
    }

    SceTouchData touch;
    if (sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1) > 0 && touch.reportNum > 0) {
        out->touchActive = true;
        out->touchX = touch.report[0].x / 2; /* 1920x1088 -> 960x544 */
        out->touchY = touch.report[0].y / 2;
    }
    if (TmcVita_RearTouchEnabled() && sceTouchPeek(SCE_TOUCH_PORT_BACK, &touch, 1) > 0) {
        for (int i = 0; i < touch.reportNum; ++i) {
            const int x = touch.report[i].x; /* 0..1919 */
            const int y = touch.report[i].y; /* 108..997 */
            /* Upper 60% only: the lower part is where fingers rest. */
            if (y > 600) continue;
            if (x < 640) h |= TMC_KEY_SLOT2;
            else if (x > 1280) h |= TMC_KEY_SLOT3;
        }
    }
    out->held = h;
}

bool TmcSys_ShouldQuit(void) { return false; }

void TmcSys_Log(const char* text) {
    if (sLog) {
        fputs(text, sLog);
        fflush(sLog);
    }
}

extern void TmcVitaVideo_ShowMessage(const char* title, const char* message);

void TmcSys_Fatal(const char* title, const char* message) {
    TmcSys_Log(title);
    TmcSys_Log("\n");
    TmcSys_Log(message);
    TmcSys_Log("\n");
    TmcVitaVideo_ShowMessage(title, message);
    sceKernelExitProcess(0);
    for (;;) sceKernelDelayThread(1000000);
}

void TmcSys_BoostThreadPriority(void) {
    /* User thread priorities run 64 (highest) .. 191; the main thread sits at
     * 160. Audio must never starve behind the renderer. */
    sceKernelChangeThreadPriority(sceKernelGetThreadId(), 96);
}

void TmcSys_PinThread(int core) {
    int mask;
    switch (core) {
        case 0: mask = SCE_KERNEL_CPU_MASK_USER_0; break;
        case 1: mask = SCE_KERNEL_CPU_MASK_USER_1; break;
        case 2: mask = SCE_KERNEL_CPU_MASK_USER_2; break;
        default: mask = SCE_KERNEL_CPU_MASK_USER_ALL; break;
    }
    sceKernelChangeThreadCpuAffinityMask(sceKernelGetThreadId(), mask);
}

bool TmcAudio_Open(int sampleRate, int frames) {
    sAudioFrames = frames;
    sAudioPort = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, frames, sampleRate,
                                     SCE_AUDIO_OUT_MODE_STEREO);
    char line[64];
    snprintf(line, sizeof(line), "[audio] port=0x%08x\n", (unsigned)sAudioPort);
    TmcSys_Log(line);
    if (sAudioPort < 0) return false;
    int vol[2] = { SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB };
    sceAudioOutSetVolume(sAudioPort, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);
    return true;
}

void TmcAudio_Output(const int16_t* stereo) {
    if (sAudioPort >= 0) sceAudioOutOutput(sAudioPort, stereo);
    else sceKernelDelayThread(10000);
}

void TmcAudio_Close(void) {
    if (sAudioPort >= 0) sceAudioOutReleasePort(sAudioPort);
    sAudioPort = -1;
}

void TmcPlatform_FrameHook(void) {}

/* newlib-vita has no readlink(); the shared ROM loader only uses it to find
 * the executable's directory, which is irrelevant here (we chdir instead). */
long readlink(const char* path, char* buf, unsigned long bufsiz) {
    (void)path;
    (void)buf;
    (void)bufsiz;
    return -1;
}

/* libgcc/libstdc++ ("gthreads") reach pthreads only through *weak*
 * references. A weak reference never pulls an object out of a static
 * archive, so on the Vita every one of them resolved to NULL:
 *  - pthread_cancel == NULL made gthreads think the program is single
 *    threaded, so the emulated TLS behind the renderer's __thread scanline
 *    buffers (and std::mutex in the audio mixer) were shared/no-ops;
 *  - once that is fixed, pthread_once/getspecific/... == NULL would make the
 *    first TLS access jump to address 0.
 * Taking the address of every gthreads entry point forces them all in. */
#include <pthread.h>
#include <sched.h>
void* const gTmcGthreadsLink[] = {
    (void*)&pthread_attr_destroy,   (void*)&pthread_attr_init,      (void*)&pthread_attr_setdetachstate,
    (void*)&pthread_cancel,         (void*)&pthread_cond_broadcast, (void*)&pthread_cond_destroy,
    (void*)&pthread_cond_init,      (void*)&pthread_cond_signal,    (void*)&pthread_cond_timedwait,
    (void*)&pthread_cond_wait,      (void*)&pthread_create,         (void*)&pthread_detach,
    (void*)&pthread_equal,          (void*)&pthread_exit,           (void*)&pthread_getschedparam,
    (void*)&pthread_getspecific,    (void*)&pthread_join,           (void*)&pthread_key_create,
    (void*)&pthread_key_delete,     (void*)&pthread_mutex_destroy,  (void*)&pthread_mutex_init,
    (void*)&pthread_mutex_lock,     (void*)&pthread_mutex_timedlock, (void*)&pthread_mutex_trylock,
    (void*)&pthread_mutex_unlock,   (void*)&pthread_mutexattr_destroy, (void*)&pthread_mutexattr_init,
    (void*)&pthread_mutexattr_settype, (void*)&pthread_once,        (void*)&pthread_rwlock_rdlock,
    (void*)&pthread_rwlock_tryrdlock, (void*)&pthread_rwlock_trywrlock, (void*)&pthread_rwlock_unlock,
    (void*)&pthread_rwlock_wrlock,  (void*)&pthread_self,           (void*)&pthread_setschedparam,
    (void*)&pthread_setspecific,    (void*)&sched_get_priority_max, (void*)&sched_get_priority_min,
    (void*)&sched_yield,
};

/* Called once at start-up: logs whether the gthreads layer is live. */
void TmcVita_CheckGthreads(void) {
    int missing = 0;
    for (unsigned i = 0; i < sizeof(gTmcGthreadsLink) / sizeof(gTmcGthreadsLink[0]); ++i)
        if (gTmcGthreadsLink[i] == NULL) ++missing;
    char line[64];
    snprintf(line, sizeof(line), "[thread] gthreads entry points missing=%d\n", missing);
    TmcSys_Log(line);
}
