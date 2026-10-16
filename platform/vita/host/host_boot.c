/*
 * Host-only test hook: TMC_BOOT="area,room,x,y" skips title/file select by
 * creating a synthetic save slot and warping there (same technique as the
 * port's repro harnesses). TMC_CHEAT_ALL=1 also grants all items.
 */
#include "main.h"
#include "save.h"
#include "fileselect.h"
#include "room.h"
#include "player.h"
#include "port_debug_actions.h"
#include "port_runtime_config.h"

#include "port_second_screen.h"
#include "port_second_screen_state.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

extern void SetActiveSave(u32 idx);
static unsigned sFrame;
unsigned gHostForcedKeys;

void TmcPlatform_FrameHook(void) {
    static int active = -1, booted, warped, cheated;
    static unsigned a, r, x, y;
    ++sFrame;
    gHostForcedKeys = 0;
    {
        extern void TmcVitaUi_TestQuickSave(void);
        extern void TmcVitaUi_TestQuickLoad(void);
        const char* qs = getenv("TMC_QSAVE_AT");
        const char* ql = getenv("TMC_QLOAD_AT");
        if (qs && sFrame == (unsigned)atoi(qs)) { fprintf(stderr, "[test] quick save @%u\n", sFrame); TmcVitaUi_TestQuickSave(); }
        if (ql && sFrame == (unsigned)atoi(ql)) { fprintf(stderr, "[test] quick load @%u\n", sFrame); TmcVitaUi_TestQuickLoad(); }
    }
    if (active < 0) {
        const char* e = getenv("TMC_BOOT");
        active = e && sscanf(e, "%x,%x,%x,%x", &a, &r, &x, &y) == 4;
    }
    if (getenv("TMC_OAM_DUMP") && sFrame == (unsigned)atoi(getenv("TMC_OAM_DUMP"))) {
        extern u16 gOamMem[0x200];
        extern u8 gIoMem[0x400];
        fprintf(stderr, "[oam] dispcnt=%04x bldcnt=%04x bldalpha=%04x win0h=%04x winin=%04x winout=%04x\n",
                gIoMem[0] | gIoMem[1] << 8, gIoMem[0x50] | gIoMem[0x51] << 8, gIoMem[0x52] | gIoMem[0x53] << 8,
                gIoMem[0x40] | gIoMem[0x41] << 8, gIoMem[0x48] | gIoMem[0x49] << 8, gIoMem[0x4a] | gIoMem[0x4b] << 8);
        for (int i = 0; i < 128; ++i) {
            u16 a0 = gOamMem[i * 4], a1 = gOamMem[i * 4 + 1], a2 = gOamMem[i * 4 + 2];
            if ((a0 & 0x300) == 0x200) continue; /* disabled */
            fprintf(stderr, "[oam] %3d y=%3d x=%3d shape=%d size=%d mode=%d aff=%d tile=%d prio=%d pal=%d\n", i,
                    a0 & 0xff, a1 & 0x1ff, a0 >> 14, a1 >> 14, (a0 >> 10) & 3, (a0 >> 8) & 1, a2 & 0x3ff,
                    (a2 >> 10) & 3, a2 >> 12);
        }
    }
    if (!active) {
        if (getenv("TMC_LANG_EARLY") && sFrame == 60) {
            gSaveHeader->language = (u8)atoi(getenv("TMC_LANG_EARLY"));
            fprintf(stderr, "[boot] language forced to %d\n", gSaveHeader->language);
        }
        return;
    }
    if (!booted && gMain.task == TASK_TITLE && sFrame >= 30 && (sFrame & 0xF) < 3)
        gHostForcedKeys = 8; /* START */
    if (!booted && gMain.task == TASK_FILE_SELECT && sFrame > 60) {
        SaveFile* sv = &gFileSelectState.saves[0];
        ResetSaveFile(0);
        sv->initialized = 1;
        sv->name[0] = 'M';
        sv->saved_status.area_next = (u8)a;
        sv->saved_status.room_next = (u8)r;
        sv->saved_status.start_pos_x = (s16)x;
        sv->saved_status.start_pos_y = (s16)y;
        gFileSelectState.saveStatus[0] = 1;
        if (getenv("TMC_LANG")) gSaveHeader->language = (u8)atoi(getenv("TMC_LANG"));
        SetActiveSave(0);
        SetTask(TASK_GAME);
        booted = 1;
        fprintf(stderr, "[boot] frame %u -> TASK_GAME\n", sFrame);
    }
    if (booted && !warped && gMain.task == TASK_GAME && sFrame % 20 == 0) {
        if ((unsigned)gRoomControls.area == a && (unsigned)gRoomControls.room == r) {
            warped = 1;
            fprintf(stderr, "[boot] frame %u in area %x room %x\n", sFrame, a, r);
        } else {
            Port_DebugAction_Warp((unsigned char)a, (unsigned char)r, (unsigned short)x, (unsigned short)y, 0);
        }
    }
    if (getenv("TMC_OVERLAY_DUMP") && sFrame == (unsigned)atoi(getenv("TMC_OVERLAY_DUMP"))) {
        static uint32_t px[544 * 960];
        SecondScreenSnapshot snap;
        Port_SecondScreenState_Read(&snap);
        int W = getenv("TMC_OVERLAY_W") ? atoi(getenv("TMC_OVERLAY_W")) : 960;
        int H = getenv("TMC_OVERLAY_H") ? atoi(getenv("TMC_OVERLAY_H")) : 544;
        for (int t = 0; t < 3; ++t) Port_SecondScreen_PaintInto(px, W, H, W, &snap, (uint32_t)(sFrame + t));
        FILE* f = fopen("overlay.ppm", "wb");
        fprintf(f, "P6\n%d %d\n255\n", W, H);
        for (int i = 0; i < W * H; ++i) { unsigned char c[3] = { px[i] & 255, (px[i] >> 8) & 255, (px[i] >> 16) & 255 }; fwrite(c, 1, 3, f); }
        fclose(f);
        fprintf(stderr, "[boot] overlay dumped (inGame=%d)\n", snap.inGame);
    }
    if (warped && !cheated && getenv("TMC_CHEAT_ALL")) {
        Port_DebugAction_GiveAllItems();
        Port_DebugAction_MaxHearts();
        cheated = 1;
    }
}
