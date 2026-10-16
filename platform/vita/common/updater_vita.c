/* The 3DS port updates itself over the network; the Vita build does not.
 * These stubs keep the shared settings panel linkable and tell the player
 * where new versions come from. */
#include "updater.h"
#include <stdio.h>
#include <string.h>

void Updater_Init(const char* launch_path) { (void)launch_path; }
void Updater_Check(void) {}
void Updater_SetChannel(bool prerelease) { (void)prerelease; }
void Updater_Download(void) {}
void Updater_Cancel(void) {}
void Updater_GetStatus(UpdateStatus* out) {
    memset(out, 0, sizeof(*out));
    out->state = UPDATE_CURRENT;
    snprintf(out->version, sizeof(out->version), "%s", TMC_PORT_VERSION);
    snprintf(out->message, sizeof(out->message), "Neue Versionen als VPK installieren");
}
bool Updater_Busy(void) { return false; }
bool Updater_ShouldClose(void) { return false; }
void Updater_Shutdown(void) {}
unsigned Updater_GetNotes(char* out, unsigned capacity) {
    if (capacity) out[0] = 0;
    return 0;
}
unsigned Update_FormatNotes(const char* markdown, char lines[][43], unsigned capacity) {
    (void)markdown; (void)lines; (void)capacity;
    return 0;
}
