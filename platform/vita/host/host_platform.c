/*
 * Headless 32-bit Linux harness implementing tmc_platform.h.
 *   tmc_host <rom.gba> <frames> <outdir> [script]
 * The script holds lines "<frame> <hexmask>" (TMC_KEY_* bits held from that
 * frame on). Frames listed in TMC_DUMP ("100,200,...") are saved as PPM.
 */
#include "tmc_platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static unsigned sPresented;
static unsigned sMaxFrames = 600;
static const char* sOutDir = ".";
static struct { unsigned frame; uint32_t mask; } sScript[512];
static int sScriptCount;
static int sAudioRate, sAudioFrames;
static FILE* sWav;

void Host_Configure(unsigned maxFrames, const char* outDir, const char* scriptPath) {
    sMaxFrames = maxFrames;
    sOutDir = outDir;
    if (scriptPath) {
        FILE* f = fopen(scriptPath, "r");
        if (f) {
            unsigned fr; unsigned mask;
            while (sScriptCount < 512 && fscanf(f, "%u %x", &fr, &mask) == 2) {
                sScript[sScriptCount].frame = fr;
                sScript[sScriptCount].mask = mask;
                ++sScriptCount;
            }
            fclose(f);
        }
    }
}

uint64_t TmcSys_TimeUs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

extern unsigned gHostForcedKeys;
void TmcSys_PollInput(TmcInputState* out) {
    memset(out, 0, sizeof(*out));
    out->held |= gHostForcedKeys;
    for (int i = 0; i < sScriptCount; ++i)
        if (sScript[i].frame <= sPresented) out->held = sScript[i].mask | gHostForcedKeys;
}

bool TmcSys_ShouldQuit(void) { return sPresented >= sMaxFrames; }
void TmcSys_Log(const char* text) { fputs(text, stderr); }
void TmcSys_Fatal(const char* title, const char* message) {
    fprintf(stderr, "FATAL: %s\n%s\n", title, message);
    exit(1);
}
void TmcSys_PinThread(int core) { (void)core; }
void TmcSys_BoostThreadPriority(void) {}

static int ShouldDump(unsigned frame) {
    const char* list = getenv("TMC_DUMP");
    if (!list) return 0;
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", list);
    for (char* t = strtok(buf, ","); t; t = strtok(NULL, ","))
        if ((unsigned)atoi(t) == frame) return 1;
    return 0;
}

void TmcVideo_Present(const uint32_t* pixels, int width, int height, int pitch, bool vsync) {
    static uint64_t next;
    if (vsync && !getenv("TMC_FAST")) {
        uint64_t now = TmcSys_TimeUs();
        if (next == 0 || now > next + 50000) next = now;
        if (now < next) usleep((useconds_t)(next - now));
        next += 16667;
    }
    ++sPresented;
    if (ShouldDump(sPresented)) {
        char path[512];
        snprintf(path, sizeof(path), "%s/frame_%05u.ppm", sOutDir, sPresented);
        FILE* f = fopen(path, "wb");
        if (f) {
            fprintf(f, "P6\n%d %d\n255\n", width, height);
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) {
                    uint32_t p = pixels[y * pitch + x];
                    unsigned char rgb[3] = { (unsigned char)(p & 0xff), (unsigned char)((p >> 8) & 0xff),
                                             (unsigned char)((p >> 16) & 0xff) };
                    fwrite(rgb, 1, 3, f);
                }
            fclose(f);
        }
    }
    if (sPresented % 300 == 0) fprintf(stderr, "[host] frame %u\n", sPresented);
}

bool TmcAudio_Open(int sampleRate, int frames) {
    sAudioRate = sampleRate;
    sAudioFrames = frames;
    char path[512];
    snprintf(path, sizeof(path), "%s/audio.raw", sOutDir);
    sWav = fopen(path, "wb");
    return true;
}
void TmcAudio_Output(const int16_t* stereo) {
    if (sWav) fwrite(stereo, sizeof(int16_t), (size_t)sAudioFrames * 2, sWav);
    usleep((useconds_t)((uint64_t)sAudioFrames * 1000000u / (unsigned)sAudioRate));
}
void TmcAudio_Close(void) {
    if (sWav) fclose(sWav);
    sWav = NULL;
}

void TmcDiag_LogSystemInfo(void) { fprintf(stderr, "[sys] host build\n"); }
const char* TmcDiag_MemoryText(void) { return "host"; }
void TmcDiag_LogSettings(void) {}
