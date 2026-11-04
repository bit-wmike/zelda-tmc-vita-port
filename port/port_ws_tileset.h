#ifndef PORT_WS_TILESET_H
#define PORT_WS_TILESET_H

/*
 * Tile-set swap areas in the wide views (Vita port).
 *
 * Some areas swap BG tile data (and palettes) depending on which part of the
 * map touches the screen: the Minish village replaces its whole tile set
 * (minishVillageTileSetManager.c), Hyrule Town three independent 8 KiB slots
 * (hyruleTownTileSetManager.c). A 240x160 view never shows two parts that
 * need different data in the same slot; Breitbild and Full View can, and the
 * part whose data was not loaded showed foreign tiles.
 *
 * The manager describes its slots (one region table each, same format as
 * CheckRegionsOnScreen) and reports which group each slot holds. Each shadow
 * cell then gets a tile-set context for the renderer (cpu/mode1.h,
 * virtuappu_mode1_ws_alt_ctx): 0 = VRAM as loaded, otherwise an image of BG
 * VRAM with the cell's groups written over it (ROM data) plus their palettes.
 */

#include "global.h"

#define PORT_WS_TILESET_MAX_SLOTS 3
#define PORT_WS_TILESET_MAX_GROUPS 8

typedef struct PortWsTileSet {
    u8 slotCount;
    const u16* regions[PORT_WS_TILESET_MAX_SLOTS]; /* {group, x, y, w, h}..., 0xff */
    /* Writes group's tile data into a BG VRAM image (offsets from 0x06000000)
     * and returns the 4 KiB pages it fully replaces. */
    u16 (*writeGroup)(u32 group, u8* vram);
    /* Palette group loaded with the group, or -1 for none. */
    int (*paletteGroup)(u32 group);
} PortWsTileSet;

#ifdef TMC_VITA
/* Manager, once per update. loaded[slot] = group completely in VRAM, or 0xff. */
void Port_WsTileSet_Update(const PortWsTileSet* set, const u8* loaded);

/* Shadow producer (port_linked_stubs.c), once per VBlank. */
void Port_WsTileSet_BeginFrame(void);
u8* Port_WsTileSet_BeginShadow(int scrolling);
u8 Port_WsTileSet_Ctx(s32 worldCol, s32 worldRow);
void Port_WsTileSet_EndShadow(int bgIndex, u8* ctx);

/* Presenter, right before the frame is rendered. */
void Port_WsTileSet_PrepareRender(void);
#endif

#endif
