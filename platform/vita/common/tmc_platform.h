/*
 * Thin hardware interface for the Vita port.
 *
 * Everything the game needs from the machine goes through these few hooks.
 * source/ implements them for the PS Vita, host/ for the headless 32-bit
 * Linux test harness. The rest of the port (common/ and the shared engine)
 * is identical on both, which is what makes the host tests meaningful.
 */
#ifndef TMC_PLATFORM_H
#define TMC_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Button bits. They deliberately use the 3DS HID bit positions, because the
 * shared config/soft-slot code reads Platform3DS_KeysHeld() with those masks
 * (X/Y/ZL/ZR = extra item slots 0..3). */
enum {
    TMC_KEY_A = 1u << 0,
    TMC_KEY_B = 1u << 1,
    TMC_KEY_SELECT = 1u << 2,
    TMC_KEY_START = 1u << 3,
    TMC_KEY_RIGHT = 1u << 4,
    TMC_KEY_LEFT = 1u << 5,
    TMC_KEY_UP = 1u << 6,
    TMC_KEY_DOWN = 1u << 7,
    TMC_KEY_R = 1u << 8,
    TMC_KEY_L = 1u << 9,
    TMC_KEY_SLOT0 = 1u << 10, /* 3DS X  -> extra item slot 1 */
    TMC_KEY_SLOT1 = 1u << 11, /* 3DS Y  -> extra item slot 2 */
    TMC_KEY_SLOT2 = 1u << 14, /* 3DS ZL -> extra item slot 3 */
    TMC_KEY_SLOT3 = 1u << 15, /* 3DS ZR -> extra item slot 4 */
    TMC_KEY_TURBO = 1u << 24,
    TMC_KEY_MENU = 1u << 25,    /* opens the Vita options/cheat menu */
    TMC_KEY_OVERLAY = 1u << 26, /* toggles the map/status overlay */
};

typedef struct TmcInputState {
    uint32_t held;
    float stickX; /* -1..1, right positive */
    float stickY; /* -1..1, down positive */
    /* Front touch: one active point in 960x544 screen space. */
    bool touchActive;
    int touchX;
    int touchY;
} TmcInputState;

/* --- system --- */
uint64_t TmcSys_TimeUs(void);
void TmcSys_PollInput(TmcInputState* out);
bool TmcSys_ShouldQuit(void);
void TmcSys_Log(const char* text);
void TmcSys_Fatal(const char* title, const char* message);
void TmcSys_PinThread(int core);
void TmcSys_BoostThreadPriority(void);

/* --- video --- */
/* Presents one finished game frame (ABGR8888, `pitch` pixels per row) and
 * waits for the next display refresh when `vsync` is set. */
void TmcVideo_Present(const uint32_t* pixels, int width, int height, int pitch, bool vsync);

/* --- audio --- */
bool TmcAudio_Open(int sampleRate, int frames);
/* Blocks until the hardware accepted one buffer of `frames` stereo samples. */
void TmcAudio_Output(const int16_t* stereo);
void TmcAudio_Close(void);

#ifdef __cplusplus
}
#endif

#endif
