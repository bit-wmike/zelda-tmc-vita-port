#include <malloc.h>
#include "port_audio.h"
#include "port_ppu.h"
#include "port_rom.h"
#include "port_runtime_config.h"

#include <stdio.h>
#include <stdlib.h>

extern void AgbMain(void);
extern void Host_Configure(unsigned maxFrames, const char* outDir, const char* scriptPath);

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: %s rom.gba frames outdir [script]\n", argv[0]);
        return 2;
    }
    Host_Configure((unsigned)atoi(argv[2]), argv[3], argc > 4 ? argv[4] : NULL);
    Port_Config_Load("tmcvita.ini");
    Port_LoadRom(argv[1]);
    extern void TmcVitaUi_Init(void);
    TmcVitaUi_Init();
    Port_PPU_Init(NULL);
    if (!Port_Audio_Init()) fprintf(stderr, "audio unavailable\n");
    AgbMain();
    fprintf(stderr, "[host] engine returned\n");
    Port_Audio_Shutdown();
    if (getenv("TMC_MALLOC_STATS")) malloc_stats();
    Port_PPU_Shutdown();
    fprintf(stderr, "[host] shutdown complete\n");
    return 0;
}
