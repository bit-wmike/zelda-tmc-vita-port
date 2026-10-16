/*
 * The Minish Cap Vita - entry point.
 *
 * Data folder: ux0:data/tmc/
 *   - put your own Minish Cap ROM there (any *.gba name; USA or Europe)
 *   - settings (tmcvita.ini), saves (tmc_eu.sav / tmc.sav) and log.txt live
 *     there too, so saves are ordinary mGBA-compatible .sav files.
 */
#include "platform_3ds.h"
#include "tmc_platform.h"

#include "port_audio.h"
#include "port_ppu.h"
#include "port_rom.h"
#include "port_runtime_config.h"

#include <psp2/apputil.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/power.h>
#include <psp2/sysmodule.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define APP_DIR "ux0:data/tmc"

/* Main thread stack: the decompiled engine keeps some large locals. */
unsigned int sceUserMainThreadStackSize = 1 * 1024 * 1024;

extern void AgbMain(void);
extern bool TmcVitaVideo_Init(void);
extern void TmcVitaSys_Init(void);
extern void TmcVitaSys_OpenLog(const char* path);
extern void TmcVitaUi_Init(void);

static int HasGbaExtension(const char* name) {
    const size_t n = strlen(name);
    return n >= 5 && name[n - 4] == '.' && tolower((unsigned char)name[n - 3]) == 'g' &&
           tolower((unsigned char)name[n - 2]) == 'b' && tolower((unsigned char)name[n - 1]) == 'a';
}

static int RomIsSupported(const char* path) {
    char code[4];
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    const int ok = fseek(f, 0xAC, SEEK_SET) == 0 && fread(code, 1, 4, f) == 4 &&
                   (memcmp(code, "BZME", 4) == 0 || memcmp(code, "BZMP", 4) == 0);
    fclose(f);
    return ok;
}

/* Returns 1 = found, 0 = no .gba at all, -1 = only unsupported .gba files. */
static int FindRom(char* out, size_t outSize) {
    DIR* dir = opendir(APP_DIR);
    if (!dir) return 0;
    int foundGba = 0;
    struct dirent* e;
    while ((e = readdir(dir)) != NULL) {
        if (e->d_name[0] == '.' || !HasGbaExtension(e->d_name)) continue;
        char path[512];
        snprintf(path, sizeof(path), "%s/%s", APP_DIR, e->d_name);
        foundGba = 1;
        if (!RomIsSupported(path)) continue;
        snprintf(out, outSize, "%s", path);
        closedir(dir);
        return 1;
    }
    closedir(dir);
    return foundGba ? -1 : 0;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    /* Full speed: the software PPU uses all three application cores. */
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);
    TmcSys_PinThread(0);

    sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);
    SceAppUtilInitParam init;
    SceAppUtilBootParam boot;
    memset(&init, 0, sizeof(init));
    memset(&boot, 0, sizeof(boot));
    sceAppUtilInit(&init, &boot);

    mkdir("ux0:data", 0777);
    mkdir(APP_DIR, 0777);
    chdir(APP_DIR);
    TmcVitaSys_OpenLog(APP_DIR "/log.txt");
    TmcVitaSys_Init();
    if (!TmcVitaVideo_Init()) {
        TmcSys_Log("video init failed\n");
        sceKernelExitProcess(0);
        return 1;
    }

    char romPath[512];
    const int found = FindRom(romPath, sizeof(romPath));
    if (found <= 0) {
        TmcSys_Fatal(found < 0 ? "ROM nicht unterstuetzt / Unsupported ROM" : "ROM nicht gefunden / ROM not found",
                     "Kopiere dein eigenes ROM von The Minish Cap (Europa oder USA)\n"
                     "in den Ordner  ux0:data/tmc/  (Dateiname egal, Endung .gba).\n\n"
                     "Copy your own Minish Cap ROM (Europe or USA) to ux0:data/tmc/\n\n"
                     "SHA-1 Europa: cff199b36ff173fb6faf152653d1bccf87c26fb7\n"
                     "SHA-1 USA:    b4bd50e4131b027c334547b4524e2dbbd4227130");
        return 1;
    }

    TmcSys_Log("The Minish Cap Vita " TMC_PORT_VERSION "\n");
    TmcSys_Log("ROM: ");
    TmcSys_Log(romPath);
    TmcSys_Log("\n");
    Port_Config_Load("tmcvita.ini");
    Port_LoadRom(romPath);
    TmcSys_Log("[vita] ROM loaded\n");
    TmcVitaUi_Init();
    Port_PPU_Init(NULL);
    TmcSys_Log("[vita] PPU ready\n");
    if (!Port_Audio_Init()) TmcSys_Log("[vita] audio unavailable\n");
    else TmcSys_Log("[vita] audio ready\n");
    TmcSys_Log("[vita] starting engine\n");
    AgbMain();
    TmcSys_Log("[vita] engine stopped\n");

    Port_Audio_Shutdown();
    Port_PPU_Shutdown();
    sceKernelExitProcess(0);
    return 0;
}
