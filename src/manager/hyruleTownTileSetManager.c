/**
 * @file hyruleTownTileSetManager.c
 * @ingroup Managers
 *
 * @brief Swap tileSet data in hyrule town depending on the position.
 */
#include "manager/hyruleTownTileSetManager.h"
#include "area.h"
#include "asm.h"
#include "flags.h"
#include "main.h"
#include "room.h"
#include "tiles.h"
#include "game.h"
#include "assets/gfx_offsets.h"
#include "port_offset_remap.h"
#include "player.h"
#ifdef TMC_VITA
#include "port_ws_tileset.h"
#include <string.h>
#endif

void HyruleTownTileSetManager_UpdateLoadGfxGroups(HyruleTownTileSetManager*);
void HyruleTownTileSetManager_OnEnterRoom(HyruleTownTileSetManager*);

// clang-format off
static const u16 gHyruleTownTileSetManager_regions0[] = {
    0, 0x000, 0x000, 0x3f0, 0x200,
    1, 0x000, 0x280, 0x3f0, 0x140,
    0xff
};
static const u16 gHyruleTownTileSetManager_regions1[] = {
    2, 0x000, 0x000, 0x180, 0x3c0,
    3, 0x280, 0x000, 0x170, 0x3c0,
    0xff
};
static const u16 gHyruleTownTileSetManager_regions2[] = {
    5, 0x130, 0x1b0, 0x190, 0x140,
    4, 0x000, 0x000, 0x3f0, 0x3c0,
    0xff
};
static const u16 gHyruleTownTileSetManager_festivalRegions0[] = {
    0, 0x000, 0x000, 0x190, 0x1d0,
    1, 0x000, 0x2a0, 0x190, 0x120,
    0xff
};
static const u16 gHyruleTownTileSetManager_festivalRegions1[] = {
    0xff
};
static const u16 gHyruleTownTileSetManager_festivalRegions2[] = {
    5, 0x000, 0x1b0, 0x190, 0x140,
    4, 0x000, 0x000, 0x190, 0x3c0,
    0xff
};
// clang-format on

void HyruleTownTileSetManager_LoadGfxGroup(u32, u32);
void HyruleTownTileSetManager_BuildSecondOracleHouse(void);

bool32 HyruleTownTileSetManager_UpdateRoomGfxGroup(HyruleTownTileSetManager*, u32, u8*, const u16*);

extern const u8 gUnk_086E8460[];

typedef struct {
    u32 gfx1;
    void* dest1;
    u32 gfx2;
    void* dest2;
} HyruleTownTileSetManagerGfxInfo;

static const HyruleTownTileSetManagerGfxInfo gHyruleTownTileSetManagerGfxInfos[] = {
    { offset_gUnk_086D4460 + 0x8000, BG_SCREEN_ADDR(0), offset_gUnk_086D4460 + 0xE000, BG_SCREEN_ADDR(16) },
    { offset_gUnk_086D4460 + 0xB000, BG_SCREEN_ADDR(0), offset_gUnk_086D4460 + 0x11000, BG_SCREEN_ADDR(16) },
    { offset_gUnk_086D4460 + 0x9000, BG_SCREEN_ADDR(2), offset_gUnk_086D4460 + 0xF000, BG_SCREEN_ADDR(18) },
    { offset_gUnk_086D4460 + 0xC000, BG_SCREEN_ADDR(2), offset_gUnk_086D4460 + 0x12000, BG_SCREEN_ADDR(18) },
    { offset_gUnk_086D4460 + 0xA000, BG_SCREEN_ADDR(4), offset_gUnk_086D4460 + 0x10000, BG_SCREEN_ADDR(20) },
    { offset_gUnk_086D4460 + 0xD000, BG_SCREEN_ADDR(4), offset_gUnk_086D4460 + 0x13000, BG_SCREEN_ADDR(20) }
};
static const HyruleTownTileSetManagerGfxInfo gHyruleTownTileSetManagerGfxInfosFestival[] = {
    { offset_gUnk_086E8460 + 0x800, BG_SCREEN_ADDR(0), offset_gUnk_086E8460 + 0x6800, BG_SCREEN_ADDR(16) },
    { offset_gUnk_086E8460 + 0x3800, BG_SCREEN_ADDR(0), offset_gUnk_086E8460 + 0x9800, BG_SCREEN_ADDR(16) },
    { offset_gUnk_086E8460 + 0x1800, BG_SCREEN_ADDR(2), offset_gUnk_086E8460 + 0x7800, BG_SCREEN_ADDR(18) },
    { offset_gUnk_086E8460 + 0x4800, BG_SCREEN_ADDR(2), offset_gUnk_086E8460 + 0xA800, BG_SCREEN_ADDR(18) },
    { offset_gUnk_086E8460 + 0x2800, BG_SCREEN_ADDR(4), offset_gUnk_086E8460 + 0x8800, BG_SCREEN_ADDR(20) },
    { offset_gUnk_086E8460 + 0x5800, BG_SCREEN_ADDR(4), offset_gUnk_086E8460 + 0xB800, BG_SCREEN_ADDR(20) },
};
#ifdef PC_PORT
extern const u8* gGlobalGfxAndPalettes;
#else
extern const u8 gGlobalGfxAndPalettes[];
#endif

#ifdef TMC_VITA
/* Wide views can show parts of the town that need different data in the
 * same slot (port_ws_tileset.h). A group writes two 4 KiB blocks
 * (HyruleTownTileSetManager_LoadGfxGroup); group 2 also gets the finished
 * second oracle house (HyruleTownTileSetManager_BuildSecondOracleHouse). */
static u16 HyruleTown_WsWrite(u32 group, u8* vram, bool32 festival) {
    const HyruleTownTileSetManagerGfxInfo* info;
    u32 base;
    if (group > 5)
        return 0;
    info = festival ? &gHyruleTownTileSetManagerGfxInfosFestival[group] : &gHyruleTownTileSetManagerGfxInfos[group];
    base = festival ? offset_gUnk_086E8460 : offset_gUnk_086D4460;
    const u32 adj = Port_RemapGfxOffset(base) - base;
    const u32 off1 = (u32)(uintptr_t)info->dest1 - 0x06000000u;
    const u32 off2 = (u32)(uintptr_t)info->dest2 - 0x06000000u;
    u16 pages = 0;
    if (off1 + BG_SCREEN_SIZE * 2 <= 0x10000u) {
        memcpy(vram + off1, &gGlobalGfxAndPalettes[info->gfx1 + adj], BG_SCREEN_SIZE * 2);
        pages |= (u16)(1u << (off1 >> 12));
    }
    if (off2 + BG_SCREEN_SIZE * 2 <= 0x10000u) {
        memcpy(vram + off2, &gGlobalGfxAndPalettes[info->gfx2 + adj], BG_SCREEN_SIZE * 2);
        pages |= (u16)(1u << (off2 >> 12));
    }
    if (!festival && group == 2 && CheckGlobalFlag(TATEKAKE_HOUSE))
        memcpy(vram + ((u32)(uintptr_t)BG_SCREEN_ADDR(3) - 0x06000000u), gUnk_086E8460, BG_SCREEN_SIZE);
    return pages;
}
static u16 HyruleTown_WsWriteGroup(u32 group, u8* vram) {
    return HyruleTown_WsWrite(group, vram, FALSE);
}
static u16 HyruleTown_WsWriteGroupFestival(u32 group, u8* vram) {
    return HyruleTown_WsWrite(group, vram, TRUE);
}
static void HyruleTownTileSetManager_ReportWideView(void) {
    static const u16 sNoRegions[] = { 0xff };
    static const PortWsTileSet sSet = {
        3,
        { gHyruleTownTileSetManager_regions0, gHyruleTownTileSetManager_regions1,
          gHyruleTownTileSetManager_regions2 },
        HyruleTown_WsWriteGroup,
        NULL,
    };
    static const PortWsTileSet sSetFestival = {
        3,
        { gHyruleTownTileSetManager_festivalRegions0, sNoRegions, gHyruleTownTileSetManager_festivalRegions2 },
        HyruleTown_WsWriteGroupFestival,
        NULL,
    };
    const u8 loaded[3] = { gRoomVars.graphicsGroups[0], gRoomVars.graphicsGroups[1], gRoomVars.graphicsGroups[2] };
    Port_WsTileSet_Update(gRoomControls.area == AREA_FESTIVAL_TOWN ? &sSetFestival : &sSet, loaded);
}
#endif

void HyruleTownTileSetManager_Main(HyruleTownTileSetManager* this) {
    if (super->action == 0) {
        super->action = 1;
        this->gfxGroup2 = 0xff;
        this->gfxGroup1 = 0xff;
        this->gfxGroup0 = 0xff;
        RegisterTransitionHandler(this, HyruleTownTileSetManager_OnEnterRoom, NULL);
        SetEntityPriority((Entity*)this, PRIO_PLAYER_EVENT);
    }
    HyruleTownTileSetManager_UpdateLoadGfxGroups(this);
#ifdef TMC_VITA
    HyruleTownTileSetManager_ReportWideView();
#endif
}

void HyruleTownTileSetManager_OnEnterRoom(HyruleTownTileSetManager* this) {
    gRoomVars.graphicsGroups[2] = 0xff;
    gRoomVars.graphicsGroups[1] = 0xff;
    gRoomVars.graphicsGroups[0] = 0xff;
    this->gfxGroup2 = 0xff;
    this->gfxGroup1 = 0xff;
    this->gfxGroup0 = 0xff;
    HyruleTownTileSetManager_UpdateLoadGfxGroups(this);
}

void HyruleTownTileSetManager_UpdateLoadGfxGroups(HyruleTownTileSetManager* this) {
    if (gRoomControls.area != AREA_FESTIVAL_TOWN) {
        if (HyruleTownTileSetManager_UpdateRoomGfxGroup(this, 0, &this->gfxGroup0,
                                                        gHyruleTownTileSetManager_regions0) != 0) {
            HyruleTownTileSetManager_LoadGfxGroup(0, this->gfxGroup0);
        }
        if (HyruleTownTileSetManager_UpdateRoomGfxGroup(this, 1, &this->gfxGroup1,
                                                        gHyruleTownTileSetManager_regions1) != 0) {
            HyruleTownTileSetManager_LoadGfxGroup(1, this->gfxGroup1);
            if (this->gfxGroup1 == 2) {
                HyruleTownTileSetManager_BuildSecondOracleHouse();
            }
        }
        if (HyruleTownTileSetManager_UpdateRoomGfxGroup(this, 2, &this->gfxGroup2,
                                                        gHyruleTownTileSetManager_regions2) != 0) {
            HyruleTownTileSetManager_LoadGfxGroup(2, this->gfxGroup2);
        }
    } else {
        if (HyruleTownTileSetManager_UpdateRoomGfxGroup(this, 0, &this->gfxGroup0,
                                                        gHyruleTownTileSetManager_festivalRegions0) != 0) {
            HyruleTownTileSetManager_LoadGfxGroup(0, this->gfxGroup0);
        }
        if (HyruleTownTileSetManager_UpdateRoomGfxGroup(this, 2, &this->gfxGroup2,
                                                        gHyruleTownTileSetManager_festivalRegions2) != 0) {
            HyruleTownTileSetManager_LoadGfxGroup(2, this->gfxGroup2);
        }
    }
}

void HyruleTownTileSetManager_BuildSecondOracleHouse(void) {
    u32 loopVar;
    u32 innerLoopVar;

    if (CheckGlobalFlag(TATEKAKE_HOUSE) != 0) {
        for (loopVar = 0; loopVar < 4; ++loopVar) {
            for (innerLoopVar = 0; innerLoopVar < 4; ++innerLoopVar) {
                SetTileByIndex(loopVar * 0x10 + TILE_TYPE_1195 + innerLoopVar,
                               TILE_LOCAL(0x28 + innerLoopVar * 0x10, 0x188 + loopVar * 0x10), 1);
            }
        }

        for (loopVar = 0; loopVar < 3; ++loopVar) {
            for (innerLoopVar = 0; innerLoopVar < 4; ++innerLoopVar) {
                SetTileByIndex(loopVar * 0x10 + TILE_TYPE_1088 + innerLoopVar,
                               TILE_LOCAL(0x28 + innerLoopVar * 0x10, 0x188 + loopVar * 0x10), 2);
            }
        }
        SetTileByIndex(TILE_TYPE_214, TILE_POS(2, 23), LAYER_TOP);
        SetTileByIndex(TILE_TYPE_215, TILE_POS(3, 23), LAYER_TOP);
        LoadResourceAsync(gUnk_086E8460, BG_SCREEN_ADDR(3), BG_SCREEN_SIZE);
    } else {
        if (CheckGlobalFlag(TATEKAKE_TOCHU) != 0) {
            for (loopVar = 0; loopVar < 5; ++loopVar) {
                for (innerLoopVar = 0; innerLoopVar < 4; ++innerLoopVar) {
                    SetTileByIndex(loopVar * 0x10 + TILE_TYPE_1190 + innerLoopVar,
                                   TILE_LOCAL(0x28 + innerLoopVar * 0x10, 0x188 + loopVar * 0x10), 1);
                }
            }
            SetTileByIndex(TILE_TYPE_1092, TILE_POS(2, 24), LAYER_TOP);
            SetTileByIndex(TILE_TYPE_1093, TILE_POS(5, 24), LAYER_TOP);
            SetTileByIndex(TILE_TYPE_1108, TILE_POS(2, 25), LAYER_TOP);
            SetTileByIndex(TILE_TYPE_1109, TILE_POS(5, 25), LAYER_TOP);
        }
    }
}

bool32 HyruleTownTileSetManager_UpdateRoomGfxGroup(HyruleTownTileSetManager* this, u32 gfxIndex, u8* pGfxGroup,
                                                   const u16* regions) {
    *pGfxGroup = CheckRegionsOnScreen(regions);
    if ((*pGfxGroup != 0xff) && (gRoomVars.graphicsGroups[gfxIndex] != *pGfxGroup)) {
        gRoomVars.graphicsGroups[gfxIndex] = *pGfxGroup;
        return TRUE;
    }
    return FALSE;
}

void HyruleTownTileSetManager_LoadGfxGroup(u32 gfxIndex, u32 gfxGroup) {
    const HyruleTownTileSetManagerGfxInfo* gfxInfo;
    u32 base;
    u32 adj;

    gRoomVars.graphicsGroups[gfxIndex] = gfxGroup;
    if (gRoomControls.area != AREA_FESTIVAL_TOWN) {
        gfxInfo = &gHyruleTownTileSetManagerGfxInfos[gfxGroup];
        base = offset_gUnk_086D4460;
    } else {
        gfxInfo = &gHyruleTownTileSetManagerGfxInfosFestival[gfxGroup];
        base = offset_gUnk_086E8460;
    }
    /* gfx1/gfx2 are compiled `base + delta` offsets into gGlobalGfxAndPalettes;
     * the base symbol shifts on EU (assets/gfx_offsets.h is USA-baseline in
     * the fat binary) — wrong Hyrule Town tiles without the remap. */
    adj = Port_RemapGfxOffset(base) - base;
    LoadResourceAsync(&gGlobalGfxAndPalettes[gfxInfo->gfx1 + adj], gfxInfo->dest1, BG_SCREEN_SIZE * 2);
    LoadResourceAsync(&gGlobalGfxAndPalettes[gfxInfo->gfx2 + adj], gfxInfo->dest2, BG_SCREEN_SIZE * 2);
}

void TryLoadPrologueHyruleTown(void) {
    u32 gfxGroup;

    if (gRoomControls.area != AREA_FESTIVAL_TOWN) {
        gfxGroup = CheckRegionsOnScreen(gHyruleTownTileSetManager_regions0);
        if (gfxGroup != 0xff) {
            HyruleTownTileSetManager_LoadGfxGroup(0, gfxGroup);
        }
        gfxGroup = CheckRegionsOnScreen(gHyruleTownTileSetManager_regions1);
        if (gfxGroup != 0xff) {
            HyruleTownTileSetManager_LoadGfxGroup(1, gfxGroup);
            if (gfxGroup == 2) {
                HyruleTownTileSetManager_BuildSecondOracleHouse();
            }
        }
        gfxGroup = CheckRegionsOnScreen(gHyruleTownTileSetManager_regions2);
        if (gfxGroup != 0xff) {
            HyruleTownTileSetManager_LoadGfxGroup(2, gfxGroup);
        }
    } else {
        gfxGroup = CheckRegionsOnScreen(gHyruleTownTileSetManager_festivalRegions0);
        if (gfxGroup != 0xff) {
            HyruleTownTileSetManager_LoadGfxGroup(0, gfxGroup);
        }
        gfxGroup = CheckRegionsOnScreen(gHyruleTownTileSetManager_festivalRegions2);
        if (gfxGroup != 0xff) {
            HyruleTownTileSetManager_LoadGfxGroup(2, gfxGroup);
        }
    }
}
