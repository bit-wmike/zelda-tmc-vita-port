/*
 * Audio: the shared MP2K (agbplay) mixer renders into a small stereo buffer
 * on a dedicated thread, which blocks on the platform output (sceAudioOut on
 * the Vita). Blocking output paces the mixer exactly to the hardware clock.
 */
#include "port_audio.h"
#include "port_m4a_backend.h"
#include "tmc_platform.h"

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef TMC_AUDIO_RATE
#define TMC_AUDIO_RATE 32000
#endif
#define BUFFER_FRAMES 512

static bool sInitialized;
static volatile bool sRunning;
static volatile bool sPaused;
static pthread_t sThread;
static int16_t sBuffer[2][BUFFER_FRAMES * 2];
static _Atomic float sVolume = 1.0f;

extern float Port_Config_GetMasterVolume(void);

static void* AudioThread(void* arg) {
    (void)arg;
    TmcSys_PinThread(-1);
    TmcSys_BoostThreadPriority();
    int index = 0;
    while (sRunning) {
        int16_t* buf = sBuffer[index];
        if (sPaused) {
            memset(buf, 0, sizeof(sBuffer[0]));
        } else {
            Port_M4A_Backend_Render(buf, BUFFER_FRAMES, false);
        }
        TmcAudio_Output(buf);
        index ^= 1;
    }
    return NULL;
}

bool Port_Audio_Init(void) {
    if (sInitialized) return true;
    if (!Port_M4A_Backend_Init(TMC_AUDIO_RATE)) return false;
    if (!TmcAudio_Open(TMC_AUDIO_RATE, BUFFER_FRAMES)) {
        Port_M4A_Backend_Shutdown();
        return false;
    }
    sVolume = fmaxf(0.0f, fminf(1.0f, Port_Config_GetMasterVolume()));
    Port_M4A_Backend_SetMasterVolume(sVolume);
    sRunning = true;
    sPaused = false;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 256 * 1024);
    if (pthread_create(&sThread, &attr, AudioThread, NULL) != 0) {
        pthread_attr_destroy(&attr);
        sRunning = false;
        TmcAudio_Close();
        Port_M4A_Backend_Shutdown();
        return false;
    }
    pthread_attr_destroy(&attr);
    sInitialized = true;
    return true;
}

void Port_Audio_Shutdown(void) {
    if (!sInitialized) return;
    sRunning = false;
    pthread_join(sThread, NULL);
    TmcAudio_Close();
    Port_M4A_Backend_Shutdown();
    sInitialized = false;
}

void Port_Audio_3DSSetPaused(bool paused) { sPaused = paused; }
void Port_Audio_Reset(void) { Port_M4A_Backend_Reset(); }
void Port_Audio_SetGbaAccurate(bool accurate) { Port_M4A_Backend_SetGbaAccurate(accurate); }
bool Port_Audio_IsGbaAccurate(void) { return Port_M4A_Backend_GetGbaAccurate(); }
void Port_Audio_SetWidth(float width) { (void)width; }
float Port_Audio_GetWidth(void) { return 1.0f; }
void Port_Audio_SetReverbLevel(int level) { Port_M4A_Backend_SetReverbLevel(level); }
int Port_Audio_GetReverbLevel(void) { return Port_M4A_Backend_GetReverbLevel(); }
void Port_Audio_SetMasterVolume(float volume) {
    sVolume = fmaxf(0.0f, fminf(1.0f, volume));
    Port_M4A_Backend_SetMasterVolume(sVolume);
}
float Port_Audio_GetMasterVolume(void) { return sVolume; }

/* ---- DSP offload hooks used by agbplay on the 3DS: never available here,
 * so every voice is mixed in software. ---- */
bool NdspPcm_Available(void) { return false; }
void NdspPcm_Init(void) {}
void NdspPcm_Shutdown(void) {}
bool NdspPcm_Play(int* slot, const int8_t* samplePtr, uint32_t endPos, uint32_t loopPos, bool loopEnabled,
                  uint32_t startPos, float rate, float leftVol, float rightVol, bool* finished) {
    (void)slot; (void)samplePtr; (void)endPos; (void)loopPos; (void)loopEnabled;
    (void)startPos; (void)rate; (void)leftVol; (void)rightVol;
    if (finished) *finished = false;
    return false;
}
void NdspPcm_Stop(int* slot) { (void)slot; }
void NdspPcm_SetMasterLevel(float level) { (void)level; }
void NdspPcm_SetPaused(bool paused) { (void)paused; }
unsigned long long NdspPcm_VoicesOffloaded(void) { return 0; }
unsigned long long NdspPcm_VoicesDeclined(void) { return 0; }
unsigned long long NdspPcm_VoicesUnsupported(void) { return 0; }
void NdspPcm_NoteUnsupported(void) {}

bool NdspPsg_Available(void) { return false; }
void NdspPsg_SetMasterLevel(float level) { (void)level; }
void NdspPsg_SetPaused(bool paused) { (void)paused; }
void NdspPsg_SetReverbLevel(int level) { (void)level; }
bool NdspPsg_PlaySquare(int* slot, float fetchRateHz, int dutyIndex, float volumeLeft, float volumeRight) {
    (void)slot; (void)fetchRateHz; (void)dutyIndex; (void)volumeLeft; (void)volumeRight;
    return false;
}
bool NdspPsg_PlayWave(int* slot, float fetchRateHz, const unsigned char* wave16, float volumeLeft,
                      float volumeRight) {
    (void)slot; (void)fetchRateHz; (void)wave16; (void)volumeLeft; (void)volumeRight;
    return false;
}
bool NdspPsg_PlayNoise(int* slot, float lfsrClockHz, bool shortPeriod, float volumeLeft, float volumeRight) {
    (void)slot; (void)lfsrClockHz; (void)shortPeriod; (void)volumeLeft; (void)volumeRight;
    return false;
}
void NdspPsg_Stop(int* slot) { (void)slot; }
unsigned long long NdspPsg_VoicesOffloaded(void) { return 0; }
unsigned long long NdspPsg_VoicesRateDeclined(void) { return 0; }
int NdspPsg_CurrentReverbLevel(void) { return 0; }
void NdspPsg_Init(void) {}
void NdspPsg_Shutdown(void) {}
