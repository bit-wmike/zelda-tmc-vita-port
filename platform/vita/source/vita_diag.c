/* Vita start-up information for log.txt. */
#include "tmc_diag.h"
#include "tmc_platform.h"
#include "port_runtime_config.h"

#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/power.h>

#include <stdio.h>
#include <string.h>

extern bool Port_Config_VitaFullView(void);
extern int Port_Config_Get3DSDisplayStyle(void);
extern bool Port_Config_VitaRearTouch(void);

const char* TmcDiag_MemoryText(void) {
    static char buf[64];
    SceKernelFreeMemorySizeInfo info;
    memset(&info, 0, sizeof(info));
    info.size = sizeof(info);
    if (sceKernelGetFreeMemorySize(&info) < 0) return "?";
    snprintf(buf, sizeof(buf), "user %dK cdram %dK", info.size_user / 1024, info.size_cdram / 1024);
    return buf;
}

void TmcDiag_LogSystemInfo(void) {
    SceKernelSystemSwVersion sw;
    memset(&sw, 0, sizeof(sw));
    sw.size = sizeof(sw);
    sceKernelGetSystemSwVersion(&sw);
    const int model = sceKernelGetModel();
    TmcDiag_Logf("[sys] build=%s %s fw=%s model=0x%x (%s) cpu=%dMHz gpu=%dMHz battery=%d%%\n", __DATE__, __TIME__,
                 sw.versionString, model, model == 0x20000 ? "PS TV" : "Vita", scePowerGetArmClockFrequency(),
                 scePowerGetGpuClockFrequency(), scePowerGetBatteryLifePercent());
    TmcDiag_Logf("[sys] free memory: %s\n", TmcDiag_MemoryText());
}

void TmcDiag_LogSettings(void) {
    TmcDiag_Logf("[cfg] style=%d wide=%d fullview=%d colorfix=%d volume=%.2f rear_touch=%d turbo=%u reborn=0x%x\n",
                 Port_Config_Get3DSDisplayStyle(), (int)Port_Config_WidescreenEnabled(),
                 (int)Port_Config_VitaFullView(), (int)Port_Config_GetColorCorrection(),
                 (double)Port_Config_GetMasterVolume(), (int)Port_Config_VitaRearTouch(),
                 Port_Config_GetTurboMultiplier(), Port_Config_GetRebornMask());
}
