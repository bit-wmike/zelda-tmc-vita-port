#ifndef TMC_DIAG_H
#define TMC_DIAG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum TmcDiagStage {
    TMC_DIAG_LOGIC, /* game code between two presents */
    TMC_DIAG_PPU,   /* software PPU render */
    TMC_DIAG_DRAW,  /* texture upload + GPU submit (+ UI) */
    TMC_DIAG_VSYNC, /* waiting for the display */
    TMC_DIAG_STAGE_COUNT
} TmcDiagStage;

void TmcDiag_Logf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void TmcDiag_Stage(TmcDiagStage stage, uint64_t us);
void TmcDiag_FrameDone(void);
void TmcDiag_AudioBuffer(uint64_t renderUs, bool underrun);
/* Platform: one-shot start-up information and a short free-memory string. */
void TmcDiag_LogSystemInfo(void);
const char* TmcDiag_MemoryText(void);

#ifdef __cplusplus
}
#endif

#endif
