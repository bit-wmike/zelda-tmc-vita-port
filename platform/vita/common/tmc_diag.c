/*
 * Diagnostics written to log.txt so a test run on real hardware can be
 * evaluated afterwards without a debugger:
 *  - start-up info (version, firmware, model, memory, settings, ROM)
 *  - every 5 s: frame rate and per-stage frame timings, audio render cost and
 *    underruns, free memory, game task / area / room / player position
 *  - every area/room change and every menu/cheat action as it happens
 * The last lines before a crash therefore say where and in what state the
 * game was.
 */
#include "tmc_diag.h"
#include "tmc_platform.h"

#include "main.h"
#include "room.h"
#include "player.h"
#include "save.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define REPORT_INTERVAL_US 5000000u

typedef struct Stat {
    uint64_t sum;
    uint32_t max;
    uint32_t count;
} Stat;

static Stat sStage[TMC_DIAG_STAGE_COUNT];
static const char* const kStageNames[TMC_DIAG_STAGE_COUNT] = { "logic", "ppu", "draw", "vsync" };
static uint64_t sReportStart;
static uint32_t sFrames;
static uint32_t sSlowFrames; /* frame-to-frame interval > 20 ms */
static uint64_t sLastFrameUs;
static uint32_t sMaxInterval;

/* audio (written by the audio thread; plain counters, read approximately) */
static volatile uint32_t sAudioBuffers, sAudioUnderruns, sAudioRenderMaxUs;
static volatile uint64_t sAudioRenderSumUs;

static int sLastTask = -1, sLastArea = -1, sLastRoom = -1;
static uint32_t sTotalFrames;

void TmcDiag_Logf(const char* fmt, ...) {
    char line[320];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    TmcSys_Log(line);
}

void TmcDiag_Stage(TmcDiagStage stage, uint64_t us) {
    if (stage >= TMC_DIAG_STAGE_COUNT) return;
    Stat* s = &sStage[stage];
    s->sum += us;
    s->count++;
    if (us > s->max) s->max = (uint32_t)us;
}

void TmcDiag_AudioBuffer(uint64_t renderUs, bool underrun) {
    sAudioBuffers++;
    sAudioRenderSumUs += renderUs;
    if (renderUs > sAudioRenderMaxUs) sAudioRenderMaxUs = (uint32_t)renderUs;
    if (underrun) sAudioUnderruns++;
}

static void LogGameState(const char* tag) {
    TmcDiag_Logf("[%s] frame=%u task=%d area=%02x room=%02x pos=%d,%d hp=%d/%d\n", tag, sTotalFrames,
                 (int)gMain.task, (unsigned)gRoomControls.area, (unsigned)gRoomControls.room,
                 gMain.task == TASK_GAME ? (int)(gPlayerEntity.base.x.HALF.HI - gRoomControls.origin_x) : 0,
                 gMain.task == TASK_GAME ? (int)(gPlayerEntity.base.y.HALF.HI - gRoomControls.origin_y) : 0,
                 (int)gSave.stats.health, (int)gSave.stats.maxHealth);
}

void TmcDiag_FrameDone(void) {
    const uint64_t now = TmcSys_TimeUs();
    ++sTotalFrames;
    if (sReportStart == 0) sReportStart = now;
    if (sLastFrameUs) {
        const uint64_t dt = now - sLastFrameUs;
        extern bool Port_Config_VitaHalfRate(void);
        /* "slow" = longer than one display refresh plus margin (two in 30 fps mode). */
        if (dt > (Port_Config_VitaHalfRate() ? 37000u : 20000u)) ++sSlowFrames;
        if (dt > sMaxInterval) sMaxInterval = (uint32_t)dt;
    }
    sLastFrameUs = now;
    ++sFrames;

    /* Scene changes are logged immediately. */
    const int task = (int)gMain.task;
    const int area = gMain.task == TASK_GAME ? (int)gRoomControls.area : -1;
    const int room = gMain.task == TASK_GAME ? (int)gRoomControls.room : -1;
    if (task != sLastTask || area != sLastArea || room != sLastRoom) {
        sLastTask = task;
        sLastArea = area;
        sLastRoom = room;
        LogGameState("scene");
    }

    if (now - sReportStart < REPORT_INTERVAL_US) return;
    const double secs = (double)(now - sReportStart) / 1e6;
    char line[320];
    int n = snprintf(line, sizeof(line), "[perf] fps=%.1f slow=%u maxgap=%.1fms", sFrames / secs, sSlowFrames,
                     sMaxInterval / 1000.0);
    for (int i = 0; i < TMC_DIAG_STAGE_COUNT; ++i) {
        const Stat* s = &sStage[i];
        if (s->count && n < (int)sizeof(line))
            n += snprintf(line + n, sizeof(line) - n, " %s=%.2f/%.2fms", kStageNames[i],
                          (double)s->sum / s->count / 1000.0, s->max / 1000.0);
    }
    {
        extern uint32_t TmcVita_TakeCatchUpSkips(void);
        const uint32_t skips = TmcVita_TakeCatchUpSkips();
        if (skips && n < (int)sizeof(line)) n += snprintf(line + n, sizeof(line) - n, " catchup=%u", skips);
    }
    if (sAudioBuffers && n < (int)sizeof(line))
        n += snprintf(line + n, sizeof(line) - n, " audio=%.2f/%.2fms underruns=%u",
                      (double)sAudioRenderSumUs / sAudioBuffers / 1000.0, sAudioRenderMaxUs / 1000.0,
                      sAudioUnderruns);
    if (n < (int)sizeof(line)) n += snprintf(line + n, sizeof(line) - n, " mem=%s\n", TmcDiag_MemoryText());
    TmcSys_Log(line);
    LogGameState("state");

    memset(sStage, 0, sizeof(sStage));
    sReportStart = now;
    sFrames = sSlowFrames = sMaxInterval = 0;
    sAudioBuffers = sAudioUnderruns = sAudioRenderMaxUs = 0;
    sAudioRenderSumUs = 0;
}
