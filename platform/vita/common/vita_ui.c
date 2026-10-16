/*
 * Vita UI layer: options & cheat menu, map/status overlay, touch handling.
 *
 * Runs on the game thread at frame boundaries (TmcVita_OnInputPolled), so
 * cheats and menu actions may touch game state directly. While the menu or
 * the overlay is open the game is frozen (TmcVita_ShouldFreeze) and audio is
 * paused by the shared VBlank loop.
 */
#include "tmc_platform.h"
#include "tmc_gfx.h"

#include "port_debug_actions.h"
#include "port_debug_query.h"
#include "port_dump_state.h"
#include "port_reborn.h"
#include "port_runtime_config.h"
#include "port_second_screen.h"
#include "port_second_screen_state.h"

#include "main.h"
#include "player.h"
#include "save.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- externs from the rest of the Vita layer ---- */
extern uint32_t TmcVita_RawKeysHeld(void);
extern int TmcVita_FrameWidth(void);
extern int TmcVita_FrameHeight(void);
extern double Port_PPU_3DS_CurrentFps(void);
extern bool Platform3DS_TurboHeld(void);
extern void Platform3DS_SetTurboMultiplier(unsigned m);
extern void TmcVita_RequestQuit(void);
extern void DoSoftReset(void);
extern u32 WriteSaveFile(u32 index, SaveFile* saveFile);
extern void UpdateGlobalProgress(void);
extern bool Port_DumpState3DS_Write(const char* dumpDirectory);
extern PortDumpStateResult Port_DumpState3DS_LoadLatest(void);
extern void Port_SecondScreen_OnTap(int x, int y, int longPress);
extern int Port_SecondScreenTheme_Ready(void);
extern void Port_Audio_SetMasterVolume(float volume);
extern void Port_PPU_SetColorCorrection(bool enabled);
extern bool Port_Config_VitaRearTouch(void);
extern void Port_Config_SetVitaRearTouch(bool on);
extern bool Port_Config_VitaSideIcons(void);
extern void Port_Config_SetVitaSideIcons(bool on);
extern void Port_Config_SetVitaDisplayStyle(int style);
extern bool Port_Config_VitaFullView(void);
extern void Port_Config_SetVitaFullView(bool on);
extern int Port_Config_Get3DSDisplayStyle(void);
extern int Port_DebugAction_AreaIsWarpable(unsigned char area);
extern bool Port_Config_AutosaveEnabled(void);
extern void Port_Config_SetAutosaveEnabled(bool enabled);

/* ======================================================================
 * State
 * ====================================================================== */
enum { UI_NONE, UI_MENU, UI_OVERLAY };
static int sMode = UI_NONE;
static bool sClosing; /* wait for all buttons released before resuming */

static char sToast[96];
static uint64_t sToastUntil;

static bool sCheatHearts, sCheatRupees, sCheatAmmo, sCheatNoclip;
static int sWarpChoice;

static uint32_t sPrevHeld;
static uint32_t sOverlay[544 * 960];
static uint32_t sOverlayTick;

/* touch tracking */
static bool sTouchWasActive;
static int sTouchStartX, sTouchStartY, sTouchX, sTouchY;
static uint64_t sTouchStartUs;
static bool sTouchMoved;

static void Toast(const char* text) {
    snprintf(sToast, sizeof(sToast), "%s", text);
    sToastUntil = TmcSys_TimeUs() + 2000000u;
}

static bool InGame(void) { return gMain.task == TASK_GAME; }

/* ======================================================================
 * Menu model
 * ====================================================================== */
typedef enum { IT_TOGGLE, IT_CHOICE, IT_ACTION, IT_SLIDER, IT_HEADER } ItemType;

typedef struct MenuItem {
    ItemType type;
    const char* label;
    const char* help;
    int (*get)(void);                       /* toggle/choice/slider value */
    void (*set)(int value);                 /* toggle/choice/slider */
    void (*action)(void);                   /* IT_ACTION */
    const char* const* choices;             /* IT_CHOICE labels */
    int min, max, step;                     /* IT_CHOICE count via max, IT_SLIDER range */
    bool needsGame;                         /* only in gameplay */
} MenuItem;

typedef struct MenuTab {
    const char* title;
    const MenuItem* items;
    int count;
} MenuTab;

/* ---- getters / setters ---- */
static const char* const kStyleNames[] = { "Weich", "Scharf", "Pixelgenau (3x)" };
static int GetStyle(void) { return Port_Config_Get3DSDisplayStyle(); }
static void SetStyle(int v) { Port_Config_SetVitaDisplayStyle(v); }
static int GetWide(void) { return Port_Config_WidescreenEnabled(); }
static void SetWide(int v) { Port_Config_SetWidescreenEnabled(v != 0); }
static int GetFullView(void) { return Port_Config_VitaFullView(); }
static void SetFullView(int v) { Port_Config_SetVitaFullView(v != 0); }
static int GetColor(void) { return Port_Config_GetColorCorrection(); }
static void SetColor(int v) {
    Port_Config_SetColorCorrection(v != 0);
    Port_PPU_SetColorCorrection(v != 0);
}
static int GetVolume(void) { return (int)(Port_Config_GetMasterVolume() * 100.0f + 0.5f); }
static void SetVolume(int v) {
    Port_Config_SetMasterVolume((float)v / 100.0f);
    Port_Audio_SetMasterVolume((float)v / 100.0f);
}
static int GetFps(void) { return Port_Config_GetShowFps(); }
static void SetFps(int v) { Port_Config_SetShowFps(v != 0); }
static int GetIcons(void) { return Port_Config_VitaSideIcons(); }
static void SetIcons(int v) { Port_Config_SetVitaSideIcons(v != 0); }
static int GetRear(void) { return Port_Config_VitaRearTouch(); }
static void SetRear(int v) { Port_Config_SetVitaRearTouch(v != 0); }
static int GetTurbo(void) { return (int)Port_Config_GetTurboMultiplier(); }
static void SetTurbo(int v) {
    Port_Config_SetTurboMultiplier((unsigned)v);
    Platform3DS_SetTurboMultiplier((unsigned)v);
}
static int GetHoldText(void) { return Port_Config_GetHoldToAdvanceText(); }
static void SetHoldText(int v) { Port_Config_SetHoldToAdvanceText(v != 0); }
static int GetAutosave(void) { return Port_Config_AutosaveEnabled(); }
static void SetAutosave(int v) { Port_Config_SetAutosaveEnabled(v != 0); }

#define REBORN_ACCESSORS(name, feat)                                                     \
    static int Get##name(void) { return Port_Reborn_IsEnabled(feat); }                  \
    static void Set##name(int v) {                                                     \
        Port_Reborn_SetEnabled(feat, v != 0);                                          \
        Port_Config_SetRebornMask(Port_Reborn_GetMask());                              \
    }
REBORN_ACCESSORS(Shells, REBORN_FEAT_SHELLS_9999)
REBORN_ACCESSORS(Boots, REBORN_FEAT_EQUIP_LR_BOOTS)
REBORN_ACCESSORS(Ocarina, REBORN_FEAT_EQUIP_LSELECT_OCARINA)
REBORN_ACCESSORS(NoEzloResume, REBORN_FEAT_NO_EZLO_ON_RESUME)
REBORN_ACCESSORS(Library, REBORN_FEAT_LIBRARY_REOPEN)
REBORN_ACCESSORS(Figurine, REBORN_FEAT_FIGURINE_MIN_RAISED)
REBORN_ACCESSORS(SkipTut, REBORN_FEAT_SKIP_EZLO_TUTORIALS)
REBORN_ACCESSORS(Hero, REBORN_FEAT_HERO_MODE)
REBORN_ACCESSORS(SecondLAB, REBORN_FEAT_SECONDARY_LAB)
REBORN_ACCESSORS(SelectEquip, REBORN_FEAT_SELECT_HOLD_EQUIP)
REBORN_ACCESSORS(Analog, REBORN_FEAT_ANALOG_360_MOVEMENT)

static int GetCHearts(void) { return sCheatHearts; }
static void SetCHearts(int v) { sCheatHearts = v != 0; }
static int GetCRupees(void) { return sCheatRupees; }
static void SetCRupees(int v) { sCheatRupees = v != 0; }
static int GetCAmmo(void) { return sCheatAmmo; }
static void SetCAmmo(int v) { sCheatAmmo = v != 0; }
static int GetCNoclip(void) { return sCheatNoclip; }
static void SetCNoclip(int v) {
    sCheatNoclip = v != 0;
    Port_DebugAction_SetNoclip(sCheatNoclip ? 1 : 0);
}

static void ActAllItems(void) { Port_DebugAction_GiveAllItems(); Toast("Alle Items erhalten"); }
static void ActMaxHearts(void) { Port_DebugAction_MaxHearts(); Port_DebugAction_HealFull(); Toast("Herzen maximal"); }
static void ActKinstones(void) { Port_DebugAction_AllKinstones(); Toast("Alle Kinsteine verschmolzen"); }
static void ActShells(void) { Port_DebugAction_MaxShells(); Toast("Muscheln maximal"); }
static void ActFigurines(void) { Port_DebugAction_AllFigurines130(); Toast("Alle Figuren"); }
static void ActMinish(void) { Port_DebugAction_ToggleMinish(); Toast("Gr\xc3\xb6\xc3\x9f" "e gewechselt"); }
static void ActRupees(void) { Port_DebugAction_MaxRupees(); Toast("Rubine maximal"); }

/* Teleport targets: every area the debug warp accepts. */
static unsigned char sWarpAreas[256];
static const char* sWarpNames[256];
static int sWarpCount;
static void BuildWarpList(void) {
    sWarpCount = 0;
    for (int a = 0; a < 256; ++a) {
        if (Port_DebugAction_AreaIsWarpable((unsigned char)a)) {
            const char* n = Port_DebugQuery_AreaName((unsigned char)a);
            if (!n) continue;
            sWarpAreas[sWarpCount] = (unsigned char)a;
            sWarpNames[sWarpCount] = n;
            ++sWarpCount;
        }
    }
}
static int GetWarp(void) { return sWarpChoice; }
static void SetWarp(int v) { sWarpChoice = v; }
static void ActWarp(void) {
    if (sWarpCount == 0) return;
    const unsigned char area = sWarpAreas[sWarpChoice];
    unsigned short x = 0x80, y = 0x80;
    unsigned char layer = 1;
    Port_DebugAction_WarpSpawnOverride(area, 0, &x, &y, &layer);
    if (Port_DebugAction_Warp(area, 0, x, y, layer)) {
        Port_DebugAction_ArmWarpNudge();
        Toast("Teleport...");
    } else {
        Toast("Teleport gerade nicht m\xc3\xb6glich");
    }
}

static void ActSaveGame(void) {
    if (!InGame() || gSaveHeader->saveFileId >= NUM_SAVE_SLOTS) {
        Toast("Nur im laufenden Spiel");
        return;
    }
    UpdateGlobalProgress();
    Toast(WriteSaveFile(gSaveHeader->saveFileId, &gSave) ? "Spielstand gespeichert" : "Speichern fehlgeschlagen");
}
static void ActQuickSave(void) {
    char dir[160];
    if (!InGame() || !Port_DumpState_CreateDirectory("dumps", dir, sizeof(dir))) {
        Toast("Nur im laufenden Spiel");
        return;
    }
    Toast(Port_DumpState3DS_Write(dir) ? "Position gemerkt" : "Gerade nicht m\xc3\xb6glich");
}
static bool sPendingQuickLoad, sPendingReset, sPendingQuit;
static void ActQuickLoad(void) { sPendingQuickLoad = true; }
static void ActReset(void) { sPendingReset = true; }
static void ActQuit(void) { sPendingQuit = true; }

#define TOGGLE(label, help, name) { IT_TOGGLE, label, help, Get##name, Set##name, NULL, NULL, 0, 1, 1, false }
#define TOGGLE_G(label, help, name) { IT_TOGGLE, label, help, Get##name, Set##name, NULL, NULL, 0, 1, 1, true }
#define ACTION(label, help, fn, game) { IT_ACTION, label, help, NULL, NULL, fn, NULL, 0, 0, 0, game }
#define HEADER(label) { IT_HEADER, label, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, false }

static const MenuItem kDisplayItems[] = {
    { IT_CHOICE, "Bildstil", "Scharf: knackige Pixel ohne Flimmern. Pixelgenau: exakt 3-fach.", GetStyle, SetStyle,
      NULL, kStyleNames, 0, 2, 1, false },
    TOGGLE("Breitbild", "Mehr Spielwelt links und rechts sichtbar (Originalverh\xc3\xa4ltnis).", Wide),
    TOGGLE("Weitsicht (experimentell)", "Drau\xc3\x9f" "en 400x240: deutlich mehr Welt sichtbar, kleinere Figuren.",
           FullView),
    TOGGLE("GBA-Farbkorrektur", "Farben wie auf dem GBA-Bildschirm (etwas blasser).", Color),
    { IT_SLIDER, "Lautst\xc3\xa4rke", NULL, GetVolume, SetVolume, NULL, NULL, 0, 100, 10, false },
    TOGGLE("FPS anzeigen", NULL, Fps),
    TOGGLE("Symbole am Rand", "Kleine Karten- und Men\xc3\xbc-Symbole in den schwarzen R\xc3\xa4ndern (antippbar).",
           Icons),
};

static const MenuItem kControlItems[] = {
    TOGGLE("R\xc3\xbc" "ckseiten-Touch", "Oben links/rechts auf der R\xc3\xbc" "ckseite = Zusatz-Item 3/4.", Rear),
    { IT_SLIDER, "Turbo-Faktor", "Turbo: rechten Stick nach oben halten.", GetTurbo, SetTurbo, NULL, NULL, 2, 8, 1,
      false },
    TOGGLE("Text mit Halten vorspulen", "Kreuz gedr\xc3\xbc" "ckt halten spult Dialoge vor.", HoldText),
    TOGGLE("Auto-Speichern (5 Min.)", "Speichert alle 5 Minuten den aktiven Spielstand.", Autosave),
    TOGGLE("360\xc2\xb0 Analog-Bewegung", "Link l\xc3\xa4uft mit dem Stick in 32 Richtungen.", Analog),
    TOGGLE("L + R: Pegasusstiefel", NULL, Boots),
    TOGGLE("L + Select: Okarina", NULL, Ocarina),
    TOGGLE("L + Kreuz/Kreis: Zusatz-Items", "Zusatz-Items wie Dreieck/Viereck \xc3\xbc" "ber L ausl\xc3\xb6sen.", SecondLAB),
    TOGGLE("Select halten im Men\xc3\xbc = Zusatz-Slot", "Im Pausemen\xc3\xbc Select halten + Kreuz/Kreis legt auf Dreieck/Viereck.",
           SelectEquip),
    TOGGLE("Muscheln bis 9999", NULL, Shells),
    TOGGLE("Kein Ezlo-Hinweis nach dem Laden", NULL, NoEzloResume),
    TOGGLE("Ezlo-Tutorials \xc3\xbc" "berspringen", NULL, SkipTut),
    TOGGLE("Bibliothek bleibt offen", "Die Bibliothek bleibt nach dem Vierschwert zug\xc3\xa4nglich.", Library),
    TOGGLE("Bessere Figuren-Chance", NULL, Figurine),
    TOGGLE("Heldenmodus (doppelter Schaden)", NULL, Hero),
};

static const MenuItem kCheatItems[] = {
    TOGGLE_G("Unendlich Herzen", NULL, CHearts),
    TOGGLE_G("Unendlich Rubine", NULL, CRupees),
    TOGGLE_G("Unendlich Bomben & Pfeile", NULL, CAmmo),
    TOGGLE_G("Durch W\xc3\xa4nde gehen", "Vorsicht: Man kann in W\xc3\xa4nden h\xc3\xa4ngen bleiben.", CNoclip),
    ACTION("Alle Items geben", "Gibt alle Items und Ausr\xc3\xbcstung.", ActAllItems, true),
    ACTION("Herzen auf Maximum", NULL, ActMaxHearts, true),
    ACTION("Rubine auf Maximum", NULL, ActRupees, true),
    ACTION("Muscheln auf Maximum", NULL, ActShells, true),
    ACTION("Alle Kinsteine verschmelzen", "Vorsicht: \xc3\xbc" "berspringt viele Nebenaufgaben.", ActKinstones, true),
    ACTION("Alle Figuren", NULL, ActFigurines, true),
    ACTION("Minish-Gr\xc3\xb6\xc3\x9f" "e umschalten", NULL, ActMinish, true),
    { IT_CHOICE, "Teleport-Ziel", "Mit links/rechts w\xc3\xa4hlen, darunter teleportieren.", GetWarp, SetWarp, NULL,
      NULL, 0, 0, 1, true },
    ACTION("Teleportieren", "Experimentell: Ziel ist der Raum-Anfang des Gebiets.", ActWarp, true),
};

static const MenuItem kGameItems[] = {
    ACTION("Spielstand speichern", "Speichert den aktiven Spielstand (wie im Spiel).", ActSaveGame, true),
    ACTION("Position merken", "Merkt Spielstand und aktuelle Position.", ActQuickSave, true),
    ACTION("Gemerkte Position laden", "L\xc3\xa4" "dt die zuletzt gemerkte Position.", ActQuickLoad, false),
    ACTION("Zur\xc3\xbc" "ck zum Titelbild", "Nicht gespeicherter Fortschritt geht verloren.", ActReset, false),
    ACTION("Beenden", "Nicht gespeicherter Fortschritt geht verloren.", ActQuit, false),
};

static const MenuTab kTabs[] = {
    { "Anzeige", kDisplayItems, (int)(sizeof(kDisplayItems) / sizeof(kDisplayItems[0])) },
    { "Steuerung", kControlItems, (int)(sizeof(kControlItems) / sizeof(kControlItems[0])) },
    { "Cheats", kCheatItems, (int)(sizeof(kCheatItems) / sizeof(kCheatItems[0])) },
    { "Spiel", kGameItems, (int)(sizeof(kGameItems) / sizeof(kGameItems[0])) },
};
#define TAB_COUNT ((int)(sizeof(kTabs) / sizeof(kTabs[0])))

static int sTab, sRow, sScroll;

static int ItemChoiceCount(const MenuItem* it) {
    if (it->choices == NULL && it->type == IT_CHOICE) return sWarpCount; /* teleport list */
    return it->max - it->min + 1;
}

static const char* ItemValueText(const MenuItem* it, char* buf, size_t n) {
    switch (it->type) {
        case IT_TOGGLE: return it->get() ? "An" : "Aus";
        case IT_CHOICE:
            if (!it->choices) return sWarpCount ? sWarpNames[it->get() % sWarpCount] : "-";
            return it->choices[it->get()];
        case IT_SLIDER: snprintf(buf, n, "%d", it->get()); return buf;
        default: return "";
    }
}

static void ItemAdjust(const MenuItem* it, int dir) {
    if (it->needsGame && !InGame()) return;
    switch (it->type) {
        case IT_TOGGLE: it->set(!it->get()); break;
        case IT_CHOICE: {
            const int n = ItemChoiceCount(it);
            if (n <= 0) break;
            const int base = it->choices ? it->min : 0;
            int v = it->get() - base + dir;
            v = ((v % n) + n) % n;
            it->set(v + base);
            break;
        }
        case IT_SLIDER: {
            int v = it->get() + dir * it->step;
            if (v < it->min) v = it->min;
            if (v > it->max) v = it->max;
            it->set(v);
            break;
        }
        default: break;
    }
}

static void ItemActivate(const MenuItem* it) {
    if (it->needsGame && !InGame()) {
        Toast("Nur im laufenden Spiel");
        return;
    }
    if (it->type == IT_ACTION) it->action();
    else ItemAdjust(it, +1);
}

/* ======================================================================
 * Layout
 * ====================================================================== */
#define PANEL_X 90
#define PANEL_Y 34
#define PANEL_W 780
#define PANEL_H 476
#define TAB_H 44
#define ROW_H 34
#define ROWS_VISIBLE 10
#define LIST_Y (PANEL_Y + TAB_H + 14)

static void OpenMenu(void) {
    sMode = UI_MENU;
    if (sWarpCount == 0) BuildWarpList();
}

static void CloseUi(void) {
    sMode = UI_NONE;
    sClosing = true;
}

/* ======================================================================
 * Input
 * ====================================================================== */
static void MenuInput(uint32_t down, uint32_t held) {
    const MenuTab* tab = &kTabs[sTab];
    if (down & (TMC_KEY_B | TMC_KEY_MENU)) {
        CloseUi();
        return;
    }
    if (down & TMC_KEY_L) { sTab = (sTab + TAB_COUNT - 1) % TAB_COUNT; sRow = sScroll = 0; }
    if (down & TMC_KEY_R) { sTab = (sTab + 1) % TAB_COUNT; sRow = sScroll = 0; }
    tab = &kTabs[sTab];
    /* auto-repeat for up/down/left/right */
    static int repeat;
    uint32_t nav = down;
    if (held & (TMC_KEY_UP | TMC_KEY_DOWN | TMC_KEY_LEFT | TMC_KEY_RIGHT)) {
        if (++repeat > 18 && (repeat % 4) == 0) nav |= held;
    } else {
        repeat = 0;
    }
    if (nav & TMC_KEY_UP) sRow = (sRow + tab->count - 1) % tab->count;
    if (nav & TMC_KEY_DOWN) sRow = (sRow + 1) % tab->count;
    if (sRow < sScroll) sScroll = sRow;
    if (sRow >= sScroll + ROWS_VISIBLE) sScroll = sRow - ROWS_VISIBLE + 1;
    const MenuItem* it = &tab->items[sRow];
    if (nav & TMC_KEY_LEFT) ItemAdjust(it, -1);
    if (nav & TMC_KEY_RIGHT) ItemAdjust(it, +1);
    if (down & TMC_KEY_A) ItemActivate(it);
}

static void MenuTap(int x, int y) {
    if (x < PANEL_X || x > PANEL_X + PANEL_W || y < PANEL_Y || y > PANEL_Y + PANEL_H) {
        CloseUi();
        return;
    }
    if (y < PANEL_Y + TAB_H) {
        const int t = (x - PANEL_X) * TAB_COUNT / PANEL_W;
        if (t >= 0 && t < TAB_COUNT) { sTab = t; sRow = sScroll = 0; }
        return;
    }
    const int r = (y - LIST_Y) / ROW_H + sScroll;
    const MenuTab* tab = &kTabs[sTab];
    if (y < LIST_Y || r < 0 || r >= tab->count || r >= sScroll + ROWS_VISIBLE) return;
    sRow = r;
    const MenuItem* it = &tab->items[r];
    /* Tapping the value area of a choice/slider steps it; left half goes back. */
    if ((it->type == IT_CHOICE || it->type == IT_SLIDER) && x > PANEL_X + PANEL_W / 2) {
        ItemAdjust(it, x < PANEL_X + PANEL_W * 3 / 4 ? -1 : +1);
        return;
    }
    ItemActivate(it);
}

static bool InIcon(int x, int y, bool right) {
    return y < 70 && (right ? x > 960 - 56 : x < 56);
}

static void HandleTouch(const TmcInputState* in) {
    if (in->touchActive && !sTouchWasActive) {
        sTouchStartX = sTouchX = in->touchX;
        sTouchStartY = sTouchY = in->touchY;
        sTouchStartUs = TmcSys_TimeUs();
        sTouchMoved = false;
    } else if (in->touchActive) {
        sTouchX = in->touchX;
        sTouchY = in->touchY;
        if (abs(sTouchX - sTouchStartX) > 20 || abs(sTouchY - sTouchStartY) > 20) sTouchMoved = true;
    }
    if (!in->touchActive && sTouchWasActive && !sTouchMoved) {
        const bool longPress = TmcSys_TimeUs() - sTouchStartUs > 500000u;
        const int x = sTouchStartX, y = sTouchStartY;
        if (sMode == UI_MENU) {
            MenuTap(x, y);
        } else if (sMode == UI_OVERLAY) {
            /* The compositor's own settings button opens the Vita menu. */
            if (x > 900 && y > 468) OpenMenu();
            else if (InIcon(x, y, false)) CloseUi();
            else Port_SecondScreen_OnTap(x, y, longPress ? 1 : 0);
        } else {
            if (InIcon(x, y, true)) OpenMenu();
            else if (InIcon(x, y, false) && InGame()) sMode = UI_OVERLAY;
        }
    }
    sTouchWasActive = in->touchActive;
}

static void ApplyCheats(void) {
    if (!InGame()) return;
    if (sCheatHearts) Port_DebugAction_HealFull();
    if (sCheatRupees) Port_DebugAction_MaxRupees();
    if (sCheatAmmo) {
        Port_DebugAction_SetStat(5 /* bombs */, 9999);
        Port_DebugAction_SetStat(6 /* arrows */, 9999);
    }
}

void TmcVita_OnInputPolled(const TmcInputState* input, uint32_t down) {
    const uint32_t held = input->held;
    (void)sPrevHeld;
    sPrevHeld = held;

    if (sPendingQuit) {
        sPendingQuit = false;
        TmcVita_RequestQuit();
        return;
    }

    HandleTouch(input);

    if (sMode == UI_NONE) {
        if (down & TMC_KEY_MENU) OpenMenu();
        else if ((down & TMC_KEY_OVERLAY) && InGame()) sMode = UI_OVERLAY;
    } else if (sMode == UI_MENU) {
        MenuInput(down, held);
    } else if (sMode == UI_OVERLAY) {
        if (down & (TMC_KEY_B | TMC_KEY_OVERLAY | TMC_KEY_START)) CloseUi();
        else if (down & TMC_KEY_MENU) OpenMenu();
        else if (!InGame()) CloseUi();
    }
    if (sClosing && (held & ~(TMC_KEY_TURBO)) == 0) sClosing = false;

    if (sMode == UI_NONE && !sClosing) {
        if (sPendingReset) {
            sPendingReset = false;
            DoSoftReset();
        }
        if (sPendingQuickLoad) {
            sPendingQuickLoad = false;
            const PortDumpStateResult r = Port_DumpState3DS_LoadLatest();
            Toast(r == PORT_DUMP_STATE_OK || r == PORT_DUMP_STATE_OK_LEGACY ? "Position geladen"
                                                                           : "Keine gemerkte Position");
        }
    }
    ApplyCheats();
}

/* Keys the game itself sees: nothing while a UI is open or closing. */
uint32_t TmcVita_FilterGameKeys(uint32_t held) {
    if (sMode != UI_NONE || sClosing) return 0;
    return held & ~(TMC_KEY_MENU | TMC_KEY_OVERLAY);
}

bool TmcVita_GamePaused(void) { return sMode != UI_NONE; }
bool TmcVita_ShouldFreeze(void) { return sMode != UI_NONE; }
bool TmcVita_RearTouchEnabled(void) { return Port_Config_VitaRearTouch(); }
int TmcVita_DisplayStyle(void) { return Port_Config_Get3DSDisplayStyle(); }

void TmcVitaUi_Init(void) {
    if (Port_Config_HasRebornMask()) Port_Reborn_ApplyMask(Port_Config_GetRebornMask());
    Platform3DS_SetTurboMultiplier(Port_Config_GetTurboMultiplier());
    Port_SecondScreenTheme_Ready();
    Port_SecondScreen_Init();
}

void TmcVita_BeforePresent(uint32_t* pixels, int* width, int* height, int pitch) {
    (void)pixels;
    (void)width;
    (void)height;
    (void)pitch;
    if (sMode == UI_OVERLAY) {
        SecondScreenSnapshot snap;
        Port_SecondScreenState_Read(&snap);
        Port_SecondScreen_PaintInto(sOverlay, 960, 544, 960, &snap, ++sOverlayTick);
    }
}

/* ======================================================================
 * Drawing (inside the video backend's scene)
 * ====================================================================== */
#define COL_PANEL 0x10202CF0
#define COL_TAB 0x203848FF
#define COL_TAB_ON 0x3C8C50FF
#define COL_ROW_ON 0x2E6A40FF
#define COL_TEXT 0xF2EEDCFF
#define COL_DIM 0x9AA49AFF
#define COL_ACCENT 0xFFD866FF
#define COL_HEADER 0x88C8FFFF

static void DrawMenu(void) {
    TmcGfx_FillRect(0, 0, 960, 544, 0x00000090);
    TmcGfx_FillRect(PANEL_X, PANEL_Y, PANEL_W, PANEL_H, COL_PANEL);
    const int tw = PANEL_W / TAB_COUNT;
    for (int t = 0; t < TAB_COUNT; ++t) {
        TmcGfx_FillRect(PANEL_X + t * tw + 2, PANEL_Y + 2, tw - 4, TAB_H - 4, t == sTab ? COL_TAB_ON : COL_TAB);
        const int w = TmcGfx_TextWidth(1.0f, kTabs[t].title);
        TmcGfx_Text(PANEL_X + t * tw + (tw - w) / 2, PANEL_Y + 10, 1.0f, COL_TEXT, kTabs[t].title);
    }
    TmcGfx_Text(PANEL_X + 6, PANEL_Y - 28, 0.8f, COL_DIM, "L/R: Seite   Kreuz: w\xc3\xa4hlen   \xe2\x86\x90/\xe2\x86\x92: \xc3\xa4ndern   Kreis: zur\xc3\xbc" "ck");

    const MenuTab* tab = &kTabs[sTab];
    char buf[32];
    for (int i = 0; i < ROWS_VISIBLE && sScroll + i < tab->count; ++i) {
        const int idx = sScroll + i;
        const MenuItem* it = &tab->items[idx];
        const int y = LIST_Y + i * ROW_H;
        const bool sel = idx == sRow;
        if (sel) TmcGfx_FillRect(PANEL_X + 8, y, PANEL_W - 16, ROW_H - 2, COL_ROW_ON);
        const bool disabled = it->needsGame && !InGame();
        TmcGfx_Text(PANEL_X + 20, y + 6, 0.95f, disabled ? COL_DIM : COL_TEXT, it->label);
        const char* v = ItemValueText(it, buf, sizeof(buf));
        if (v[0]) {
            const int w = TmcGfx_TextWidth(0.95f, v);
            const uint32_t c = it->type == IT_TOGGLE && !it->get() ? COL_DIM : COL_ACCENT;
            if (it->type == IT_CHOICE || it->type == IT_SLIDER)
                TmcGfx_Text(PANEL_X + PANEL_W - 40 - w - 24, y + 6, 0.95f, c, "\xe2\x97\x80");
            TmcGfx_Text(PANEL_X + PANEL_W - 40 - w, y + 6, 0.95f, c, v);
            if (it->type == IT_CHOICE || it->type == IT_SLIDER)
                TmcGfx_Text(PANEL_X + PANEL_W - 34, y + 6, 0.95f, c, "\xe2\x96\xb6");
        } else if (it->type == IT_ACTION) {
            TmcGfx_Text(PANEL_X + PANEL_W - 60, y + 6, 0.95f, COL_DIM, "\xe2\x96\xb6");
        }
    }
    if (tab->count > ROWS_VISIBLE) {
        const int barH = (PANEL_H - TAB_H - 70) * ROWS_VISIBLE / tab->count;
        const int barY = LIST_Y + (PANEL_H - TAB_H - 70 - barH) * sScroll / (tab->count - ROWS_VISIBLE);
        TmcGfx_FillRect(PANEL_X + PANEL_W - 8, barY, 4, barH, COL_DIM);
    }
    const MenuItem* cur = &tab->items[sRow];
    const char* help = cur->needsGame && !InGame() ? "Nur im laufenden Spiel verf\xc3\xbc" "gbar." : cur->help;
    if (help) TmcGfx_Text(PANEL_X + 16, PANEL_Y + PANEL_H - 40, 0.85f, COL_HEADER, help);
    TmcGfx_Text(PANEL_X + PANEL_W - 150, PANEL_Y + PANEL_H + 6, 0.75f, COL_DIM, "Minish Cap Vita " TMC_PORT_VERSION);
}

static void DrawSideIcons(void) {
    if (!Port_Config_VitaSideIcons() || !InGame()) return;
    /* Map: a small folded parchment. */
    TmcGfx_FillRect(10, 12, 32, 26, 0xE8D8A0B0);
    TmcGfx_FillRect(20, 12, 2, 26, 0xA08850B0);
    TmcGfx_FillRect(30, 12, 2, 26, 0xA08850B0);
    TmcGfx_FillRect(14, 22, 6, 6, 0xC04030C0);
    /* Menu: three bars. */
    for (int i = 0; i < 3; ++i) TmcGfx_FillRect(960 - 42, 14 + i * 9, 30, 5, 0xE8E8E8B0);
}

void TmcVita_DrawUi(void) {
    if (sMode == UI_OVERLAY) {
        TmcGfx_DrawPixels(sOverlay, 960, 544, 960, 0, 0, 1.0f, false, 255);
        TmcGfx_Text(12, 6, 0.8f, 0x303030FF, "Kreis: zur\xc3\xbc" "ck");
    } else if (sMode == UI_NONE) {
        DrawSideIcons();
    }
    if (sMode == UI_MENU) DrawMenu();
    if (Port_Config_GetShowFps()) {
        char fps[24];
        snprintf(fps, sizeof(fps), "%.0f FPS", Port_PPU_3DS_CurrentFps());
        TmcGfx_Text(960 - 80, 520, 0.7f, 0xFFFF80FF, fps);
    }
    if (Platform3DS_TurboHeld()) TmcGfx_Text(450, 4, 1.0f, 0xFFFF80FF, "\xe2\x96\xb6\xe2\x96\xb6 Turbo");
    if (sToast[0] && TmcSys_TimeUs() < sToastUntil) {
        const int w = TmcGfx_TextWidth(1.0f, sToast);
        TmcGfx_FillRect((960 - w) / 2 - 16, 490, w + 32, 36, 0x000000C0);
        TmcGfx_Text((960 - w) / 2, 496, 1.0f, COL_TEXT, sToast);
    }
}
