/*
 * Minimal libctru compatibility shim for the PS Vita (and the 32-bit host
 * test build). The shared Minish Cap 3DS sources only use a handful of
 * libctru primitives in the multi-threaded scanline renderer (mode1.c):
 * one-shot events, worker threads, a system tick and the thread priority
 * query. They map 1:1 onto pthreads, which VitaSDK ships (pthread-embedded).
 */
#ifndef TMC_VITA_COMPAT_3DS_H
#define TMC_VITA_COMPAT_3DS_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int s32; /* matches port_types.h under TMC_3DS */
typedef uint32_t Handle;
typedef int32_t Result;

#define CUR_THREAD_HANDLE 0xFFFF8000u
#define RESET_ONESHOT 0

/* One-shot event: Signal wakes exactly one Wait, then the event resets. */
typedef struct LightEvent {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int signalled;
} LightEvent;

static inline void LightEvent_Init(LightEvent* ev, int resetType) {
    (void)resetType;
    pthread_mutex_init(&ev->mutex, NULL);
    pthread_cond_init(&ev->cond, NULL);
    ev->signalled = 0;
}

static inline void LightEvent_Signal(LightEvent* ev) {
    pthread_mutex_lock(&ev->mutex);
    ev->signalled = 1;
    pthread_cond_signal(&ev->cond);
    pthread_mutex_unlock(&ev->mutex);
}

static inline void LightEvent_Wait(LightEvent* ev) {
    pthread_mutex_lock(&ev->mutex);
    while (!ev->signalled)
        pthread_cond_wait(&ev->cond, &ev->mutex);
    ev->signalled = 0;
    pthread_mutex_unlock(&ev->mutex);
}

typedef void (*ThreadFunc)(void*);

typedef struct TmcCompatThread {
    pthread_t handle;
    ThreadFunc entry;
    void* arg;
    int core;
} TmcCompatThread;
typedef TmcCompatThread* Thread;

void TmcPlatform_PinCurrentThread(int core);

static void* TmcCompat_ThreadTrampoline(void* p) {
    TmcCompatThread* t = (TmcCompatThread*)p;
    TmcPlatform_PinCurrentThread(t->core);
    t->entry(t->arg);
    return NULL;
}


static inline Thread threadCreate(ThreadFunc entry, void* arg, size_t stackSize, int prio, int core,
                                  bool detached) {
    (void)prio;
    (void)detached;
    TmcCompatThread* t = (TmcCompatThread*)calloc(1, sizeof(*t));
    if (!t)
        return NULL;
    t->entry = entry;
    t->arg = arg;
    t->core = core;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (stackSize < 64 * 1024)
        stackSize = 64 * 1024;
    pthread_attr_setstacksize(&attr, stackSize);
    int rc = pthread_create(&t->handle, &attr, TmcCompat_ThreadTrampoline, t);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        fprintf(stderr, "[thread] pthread_create failed rc=%d\n", rc);
        free(t);
        return NULL;
    }
    return t;
}

static inline Result threadJoin(Thread t, uint64_t timeoutNs) {
    (void)timeoutNs;
    if (t)
        pthread_join(t->handle, NULL);
    return 0;
}

static inline void threadFree(Thread t) {
    free(t);
}

static inline Result svcGetThreadPriority(s32* out, Handle h) {
    (void)h;
    if (out)
        *out = 0x30;
    return 0;
}

/* Monotonic tick, implemented by the platform layer (microseconds). */
unsigned long long svcGetSystemTick(void);

#ifdef __cplusplus
}
#endif

#endif
