/*
 * Vita presentation path: renders each game frame with the shared software
 * PPU (VirtuaPPU, multi-threaded over three cores) into an ABGR8888 buffer
 * and hands it to the platform video backend (GXM on the Vita).
 *
 * Frame geometry follows the 3DS port: native 240x160, the established wide
 * view (up to TMC_VITA_WIDE_MAX px), or the optional 400x240 "Full View"
 * camera for outdoor areas.
 */
#include "port_ppu.h"

#include "port_gba_mem.h"
#include "port_hdma.h"
#include "port_runtime_config.h"
#include "port_second_screen.h"
#include "port_widescreen.h"
#include "platform_3ds.h"
#include "tmc_platform.h"
#include "top_view_3ds.h"

#include "virtuappu.h"
#include "cpu/mode1.h"
#include "main.h"
#include "area.h"
#include "room.h"
#include "screen.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define GBA_NATIVE_W 240
#define GBA_NATIVE_H 160
/* Widest wide-mode frame. The 3DS port ships 266 (its 400x240 panel); the
 * renderer's wide shadow holds 32 extra px, so 272 is its hard limit. */
#ifndef TMC_VITA_WIDE_MAX
#define TMC_VITA_WIDE_MAX 266
#endif
#define FRAME_PITCH MODE1_GBA_WIDTH

static uint32_t* sFrame;
static bool sInitialized;
static bool sColorCorrection;
static int sFrameWidth = GBA_NATIVE_W;
static int sFrameHeight = GBA_NATIVE_H;
static uint32_t sFrameCount;

/* Implemented by the Vita UI layer (overlay, menus): may draw over / replace
 * the frame before it is shown. */
extern void TmcVita_BeforePresent(uint32_t* pixels, int* width, int* height, int pitch);

static int WideFrameWidth(void) {
    Port_Widescreen_SetWindowPixels(960, 544);
    if (Port_Widescreen_IsActive() && Port_Widescreen_ShadowsLive()) {
        int width = Port_Widescreen_EffectiveViewWidth();
        if (width > MODE1_GBA_WIDTH) width = MODE1_GBA_WIDTH;
        if (width > TMC_VITA_WIDE_MAX) width = TMC_VITA_WIDE_MAX;
        if (width > GBA_NATIVE_W) return width;
    }
    return GBA_NATIVE_W;
}

static void SelectFrame(int* width, int* height) {
    const Port3DSFullViewMode requested = Port_Widescreen_3DSViewMode();
    Port3DSFullViewMode mode = requested;
    const TopView3DSPpuCoherence coherence =
        TopView3DS_ResolvePpuCoherence(requested, virtuappu_mode1_ws_full_view != 0);
    if (coherence.forceNativeFrame) {
        if (coherence.clearFullViewProducer) virtuappu_mode1_ws_full_view = 0;
        *width = GBA_NATIVE_W;
        *height = GBA_NATIVE_H;
        return;
    }
    if (mode == PORT_3DS_FULL_VIEW_OUTDOOR_1X && MODE1_GBA_WIDTH >= 400 && MODE1_GBA_HEIGHT >= 240 &&
        Port_Widescreen_ShadowsLive()) {
        *width = 400;
        *height = 240;
        return;
    }
    if (mode == PORT_3DS_FULL_VIEW_INTERIOR_2X && Port_Widescreen_ShadowsLive()) {
        *width = 200;
        *height = 120;
        return;
    }
    *width = WideFrameWidth();
    *height = GBA_NATIVE_H;
}

void Port_PPU_Init(SDL_Window* window) {
    (void)window;
    if (!sFrame) {
        sFrame = (uint32_t*)aligned_alloc(64, (size_t)FRAME_PITCH * MODE1_GBA_HEIGHT * sizeof(uint32_t));
        if (!sFrame) return;
        memset(sFrame, 0x11, (size_t)FRAME_PITCH * MODE1_GBA_HEIGHT * sizeof(uint32_t));
    }
    VirtuaPPUMode1GbaMemory memory = { gIoMem, gVram, gBgPltt, gObjPltt, gOamMem };
    virtuappu_mode1_bind_gba_memory(&memory);
    virtuappu_mode1_set_old3ds_profile(false);
    sColorCorrection = Port_Config_GetColorCorrection();
    virtuappu_mode1_set_color_correction(sColorCorrection);
    Port_Widescreen_SetWindowPixels(960, 544);
    virtuappu_registers.frame_width = GBA_NATIVE_W;
    virtuappu_registers.frame_height = GBA_NATIVE_H;
    virtuappu_registers.frame_pitch = FRAME_PITCH;
    virtuappu_registers.mode = 1;
    virtuappu_mode1_set_output_buffer(sFrame, FRAME_PITCH);
    sInitialized = true;
}

void Port_PPU_PresentFrame(void) {
    if (!sInitialized) return;
    ++sFrameCount;
    extern void TmcVita_FpsTick(void);
    TmcVita_FpsTick();
    const uint16_t dispcnt = (uint16_t)(gIoMem[0] | (gIoMem[1] << 8));
    const uint8_t mode = (uint8_t)(dispcnt & 7);
    SelectFrame(&sFrameWidth, &sFrameHeight);

    virtuappu_registers.mode = (mode == 1 || mode == 2) ? 2 : 1;
    virtuappu_registers.frame_width = sFrameWidth;
    virtuappu_registers.frame_height = sFrameHeight;
    virtuappu_registers.frame_pitch = FRAME_PITCH;
    virtuappu_mode1_pre_line_callback = port_hdma_has_active_channels() ? port_hdma_step_line : NULL;
    virtuappu_mode1_bg2x_hdma_strobe = port_hdma_dest_overlaps(gIoMem + 0x28, gIoMem + 0x2c) != 0;
    virtuappu_mode1_bg2y_hdma_strobe = port_hdma_dest_overlaps(gIoMem + 0x2c, gIoMem + 0x30) != 0;
    /* Hyrule Field clouds are a seamless 256px repeating tilemap. */
    virtuappu_mode1_bg3_repeat = gMain.task == TASK_GAME && gRoomControls.area == AREA_HYRULE_FIELD &&
                                 gScreen.bg3.control == 0x1e05;
    virtuappu_mode1_bg3_hdma_native_bounds =
        port_hdma_dest_overlaps(gIoMem + MODE1_IO_BG3HOFS, gIoMem + MODE1_IO_BG3VOFS + 2) != 0;

    virtuappu_render_frame();
    if (getenv("TMC_PPU_DEBUG") && (sFrameCount % 60) == 0) {
        char line[200];
        snprintf(line, sizeof(line), "[ppu] fv=%d reason=%d w=%d f=%u dispcnt=%04x bldy=%02x pal0=%04x pal1=%04x px=%08x task=%d\n",
                 (int)Port_Config_3DSFullViewComboEnabled(), (int)Port_Widescreen_3DSFallbackReason(), sFrameWidth, sFrameCount, dispcnt, gIoMem[0x54], gBgPltt[0] | (gBgPltt[1] << 8),
                 gBgPltt[2] | (gBgPltt[3] << 8), sFrame[80 * FRAME_PITCH + 120], gMain.task);
        TmcSys_Log(line);
    }

    int w = sFrameWidth;
    int h = sFrameHeight;
    TmcVita_BeforePresent(sFrame, &w, &h, FRAME_PITCH);
    TmcVideo_Present(sFrame, w, h, FRAME_PITCH, !Platform3DS_TurboHeld());
}

int TmcVita_FrameWidth(void) { return sFrameWidth; }
int TmcVita_FrameHeight(void) { return sFrameHeight; }
const uint32_t* TmcVita_FramePixels(void) { return sFrame; }
uint32_t TmcVita_FrameCount(void) { return sFrameCount; }

void Port_PPU_SetPresentIsFirstOfTick(bool first) { (void)first; }
void Port_PPU_SetWindowTitle(const char* title) { (void)title; }
void Port_PPU_ToggleFullscreen(void) {}
bool Port_PPU_IsFullscreen(void) { return true; }
void Port_PPU_CycleWindowScale(int direction) { (void)direction; }
unsigned char Port_PPU_WindowScale(void) { return 1; }
void Port_PPU_ApplyWindowScale(void) {}
void Port_PPU_ToggleSmoothing(void) {}
void Port_PPU_CyclePresentationMode(int direction) { (void)direction; }
const char* Port_PPU_PresentationModeName(void) { return "Vita widescreen"; }
void Port_PPU_CycleFilter(int direction) { (void)direction; }
const char* Port_PPU_FilterName(void) { return "Off"; }
void Port_PPU_SetVSync(bool enabled) { (void)enabled; }
bool Port_PPU_VSyncEnabled(void) { return true; }
unsigned Port_PPU_DisplayRefreshRate(void) { return 60; }
void Port_PPU_SetColorCorrection(bool enabled) {
    sColorCorrection = enabled;
    virtuappu_mode1_set_color_correction(enabled);
}
bool Port_PPU_ColorCorrectionEnabled(void) { return sColorCorrection; }
void Port_PPU_SetPersistence(bool enabled, float rho) {
    (void)enabled;
    (void)rho;
}
void Port_PPU_Shutdown(void) {
    sInitialized = false;
    virtuappu_mode1_set_output_buffer(NULL, 0);
}
void Port_OpenInGameSettingsModal(void) {}
bool Port_InGameSettingsModalIsOpen(void) { return false; }

/* ---- quick dump / FPS hooks used by the overlay's settings page ---- */
#include "port_dump_state.h"
extern bool Port_DumpState3DS_Write(const char* dumpDirectory);
static char sLastDump[160];
static uint64_t sFpsWindowStart;
static uint32_t sFpsWindowFrames;
static double sFpsCurrent;

void TmcVita_FpsTick(void) {
    const uint64_t now = TmcSys_TimeUs();
    if (sFpsWindowStart == 0) sFpsWindowStart = now;
    ++sFpsWindowFrames;
    if (now - sFpsWindowStart >= 1000000u) {
        sFpsCurrent = (double)sFpsWindowFrames * 1000000.0 / (double)(now - sFpsWindowStart);
        sFpsWindowStart = now;
        sFpsWindowFrames = 0;
    }
}
double Port_PPU_3DS_CurrentFps(void) { return sFpsCurrent; }
double Port_PPU_3DS_AverageFps(void) { return sFpsCurrent; }
const char* Port_PPU_3DS_LastDumpDirectory(void) { return sLastDump; }
void Port_PPU_3DS_WriteQuickDump(void) {
    if (Port_DumpState_CreateDirectory("dumps", sLastDump, sizeof(sLastDump)))
        Port_DumpState3DS_Write(sLastDump);
}
