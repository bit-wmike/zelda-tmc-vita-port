# CLAUDE.md – Übergabe für Claude Code (Stand v0.1.10, 04.11.2026)

Diese Datei liest Claude Code beim Start automatisch. Sie fasst alles zusammen, was
für die Weiterarbeit am **PS-Vita-Port von Zelda: The Minish Cap** nötig ist.

## Zusammenarbeit mit Mike
- Sprache: **Deutsch**. Kurz, ehrlich, ohne Schönfärberei; Widerspruch und eigene
  Einschätzung sind erwünscht. Sparsam mit Tokens; bei Hintergrundarbeit erst melden,
  wenn sie fertig ist.
- Claude schreibt den Code (80–100 %). Mike testet auf seiner **PS Vita Slim PCH-2016
  (HENkaku)** und schickt `ux0:data/tmc/log.txt` (+ `log_prev.txt`), Screenshots,
  Spielstand (`tmc_eu.sav`) und Quick-States (`ux0:data/tmc/dumps/`).
- Mike nutzt Linux (Raspberry Pi 5), kein Windows.
- Wenn Mike „Breitbild“ sagt, meint er fast immer die **Weitsicht** (Full View, bis 400×240).
- Ziele: Weitsicht möglichst immer, konstanter Zoom (kein Hin und Her), konstante 60 FPS,
  sonst lieber stabile 30 FPS als Ruckeln/Verlangsamung. Viele Komfortfunktionen,
  Cheat-Menü, Touch/Rückseite, Karten-/Status-Overlay.
- Versionsnummer muss in der App sichtbar sein (LiveArea/Startbildschirm).

## Git-Regeln (wichtig)
- Repo: `https://github.com/bit-wmike/zelda-tmc-vita-port`, Branch **`main`**.
- **Nur in Mikes eigene Repos pushen** (origin). Nie zu `upstream` (EstebanPdN/zelda-tmc-3ds)
  oder anderen fremden Repos.
- Commit-Autor: `bit-wmike <295828+bit-wmike@users.noreply.github.com>` – keine private
  Mailadresse, keine Passwörter/Accounts/Adressen ins Repo.
- Commit-Nachricht: `Vita x.y.z: <kurze Zusammenfassung>` + Stichpunkte (Englisch).
- Keine ROM-Daten und kein Nintendo-Artwork committen (`sce_sys_local/` ist git-ignoriert).

## Projektaufbau
- Basis: zeldaret/tmc (Decomp) → Project Picori (native Engine, Breitbild, Reborn-Komfort)
  → EstebanPdN/zelda-tmc-3ds (32-Bit-ARM, EU-Erkennung, Karten-/Status-Bildschirm).
  Basis-Commit `5ebc387`, Vita-Port in `platform/vita/`. Lizenz GPL-3.0.
- Shared Code wird mit `TMC_3DS` **und** `TMC_VITA` gebaut (3DS-Hardware durch
  `compat/3ds.h` + `common/platform_core.c` ersetzt). USA-Basis mit Laufzeit-Erkennung der EU-ROM.
- `platform/vita/common/` (Host + Vita): Config `tmcvita.ini`, PPU-Frontend
  (`port_ppu_vita.c`, Software-PPU auf 3 Kernen), Audio, UI/Menü/Cheats/Overlay
  (`vita_ui.c`), Quick-State, Diagnose-Log (`tmc_diag.c`), Tempo (`platform_core.c`).
- `platform/vita/source/` (nur Vita): vita2d-Video (`vita_video.c`, Skalierung/KeepZoom),
  Eingabe, Audio, main.
- `platform/vita/host/`: headless Testbuilds und Test-Hooks (`host_boot.c`, `host_platform.c`).
- Weitsicht-Logik: `port/port_linked_stubs.c` (Kamera, Fallback, HUD-Breite),
  `port/port_3ds_full_view_policy.*` (Entscheidung + Fallback-Gründe), Renderer
  `port/ppu/src/mode1.c` (Schatten-Tilemaps, NEON, `-O3`).
- Kachelsatz-Kontexte (Minish-Dorf): `port/port_ws_tileset.*` + `virtuappu_mode1_ws_alt_*` in `mode1.c`.
- Overlay/Karte: `port/port_second_screen.c`, `port/port_second_screen_worldmap.c`.
- Doku: `README.md` (Englisch), `platform/vita/README.md` (deutsche Anleitung, Steuerung,
  Test-Schalter), `COPYRIGHT.md`, `THIRD-PARTY-LICENSES.md`.

## Bauen
```sh
# VitaSDK (falls nicht vorhanden, ~1,5 h): sudo bash platform/vita/tools/bootstrap_vitasdk.sh
export VITASDK=/usr/local/vitasdk PATH=$VITASDK/bin:$PATH
cd platform/vita
make TARGET=vita vpk -j8   # -> build-vita/TheMinishCap_Vita_<VERSION>.vpk + tmc_vita.elf
make TARGET=host -j8       # x86 32-Bit headless (braucht gcc-multilib)
make TARGET=armhost        # ARMv7, gelinkt auf 0x81000000 wie die Vita, läuft unter qemu-arm
```
- Version steht in `platform/vita/VERSION`. Zu jeder VPK die passende ELF als
  `tmc_vita-<version>.elf` mitliefern (für Absturz-Analysen).
- **Host-GCC ist nachsichtiger als Vita-GCC 15** → vor jeder Version `make TARGET=vita` bauen.
- Seit 0.1.9 eigenes Linker-Skript `platform/vita/vita.ld` (Standard + ≥ 16 KiB Lücke vor
  dem Datensegment), sonst bricht vita-elf-create ab, wenn der Text knapp unter einer
  64-KiB-Grenze endet.
- Für Host-Tests braucht man Mikes eigene EU-ROM (SHA-1 `cff199b36ff173fb6faf152653d1bccf87c26fb7`);
  sie liegt **nicht** im Repo. Mike muss sie bereitstellen.

## Testen (Host)
`build-host/tmc_host rom.gba <frames> <ausgabeordner> [skript]` – Skriptzeilen
`<frame> <tastenmaske-hex>` (Bits: `TMC_KEY_*` in `common/tmc_platform.h`; z. B. A=1, B=2,
Select=4, Start=8, rechts=10, links=20, oben=40, unten=80, R=100, L=200).
Das Arbeitsverzeichnis enthält `tmcvita.ini` (Weitsicht: `widescreen=1` + `full_view=1`),
optional `tmc_eu.sav` und `dumps/` (für Quick-Load). Das Titel-Intro dauert ~400 Frames,
der Boot-Warp passiert erst danach.

Wichtige Schalter (Umgebungsvariablen):
- `TMC_DUMP=500,600` Einzelbilder (PPM, Frame-Größe = gerendertes Bild, z. B. 400×240/266×160)
- `TMC_BOOT=area,room,x,y` (hex) direkt ins Spiel; `TMC_WARP_AT="frame:area,room,x,y"`
  zweiter Warp (z. B. nach Quick-Load)
- `TMC_QSAVE_AT` / `TMC_QLOAD_AT=frame` (lädt den neuesten State aus `dumps/` im Arbeitsordner)
- `TMC_VIEW_LOG=1` pro Frame Bildgröße, KeepZoom, Substate, UI-State, Scroll, Kamera
- `TMC_PPU_DEBUG=1` Weitsicht-Fallback-Grund; `TMC_FV_DEBUG`, `TMC_LIST_ROOMS`
- `TMC_OVERLAY_DUMP=frame`, `TMC_OVERLAY_SEQ="t:x,y;d:dx,dy;p:n"`, `TMC_DETAIL_DUMP=1`
- `TMC_OAM_DUMP`, `TMC_BG0_DUMP`, `TMC_MSG`/`TMC_MSG_AT`, `TMC_TEXT_DUMP`, `TMC_GLOBAL_FLAG`
- `TMC_LANG=4` (Deutsch), `TMC_CHEAT_ALL=1`, `TMC_AUDIO_SYNC=1` (bit-genau), `TMC_FAST=1`
- `TMC_OLD3DS_PROFILE=1` + `TMC_PPU_TIMING=frame` + `TMC_NOTHREADS=1` Renderzeit messen
  (callgrind: `--toggle-collect=TmcHost_TimedRender`)

Nützliche Boot-Ziele: Steppe `3,1,1f8,158`; Minish-Wald `0,0,268,1a0`; Deepwood `48,0,80,e0`;
Innenraum `22,11,70,5c`. Portal-Test (erstes Schrumpfen): Mikes Quick-State laden, dann
`TMC_WARP_AT=480:0,0,148,370` und Skript `540 20 / 600 0 / 620 100 / 628 0`
(von rechts auf den Stumpf, R drücken).

Prüfregeln vor jeder Version:
1. Renderer-Änderungen müssen **Bild für Bild identisch** sein: Host neu gegen alte Version
   (gleiche Boot-Ziele/Skripte, PPMs byteweise vergleichen); Abweichungen nur dort, wo gewollt.
2. Parity-Fuzzer (x86 und ARM):
   ```sh
   gcc -m32 -O2 -DVIRTUAPPU_TESTING -DMODE1_GBA_WIDTH=400 -DMODE1_GBA_HEIGHT=240 -DTMC_VITA \
     -Iport/ppu/include -Iport/ppu/src port/ppu/tests/mode1_native_fast_path_test.c port/ppu/src/mode1.c -o fz && ./fz
   arm-linux-gnueabihf-gcc -O3 -static -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard <gleiche Flags> -o fz_arm && qemu-arm fz_arm
   ```
   (auch `mode1_draw_order_profile_test.c`). Erwartet: `PASS (4096 total scenes …)`.
3. `tools/difftest.sh` (armhost vs. armlow) bei Änderungen an Speicher/Zeigern.
4. Vita-Build (GCC 15) muss ohne Fehler durchlaufen.
- Leistung abschätzen: ARM-Instruktionen per qemu-Plugin zählen; Kalibrierung aus Mikes Logs
  ≈ 0,8 ms pro Mio. Instruktionen (Weitsicht, 3 Threads). Ziel < 16 ms pro Frame.

## Stand der Technik (was schon gelöst ist)
- 0.1.4: Entity-„gelöscht“-Prüfung per Vorzeichen war auf der Vita falsch (Speicher > 0x80000000);
  alle gthread-Funktionen erzwungen. (0.1.3 nie installieren.)
- 0.1.5: Zusatz-Item-Slots (Soft-Slots) laufen auf der Vita; EU-Text lokalisiert.
- 0.1.6: Weitsicht bleibt bei Dialogen/Bannern.
- 0.1.7: raumgroße Weitsicht überall (Raum gedeckelt auf 400×240, Räume ≤ 240×160 normal),
  konstanter Zoom (`TmcVita_KeepZoom`: kleineres Bild mit Rand statt Zoomsprung, Menüs
  behalten ihren Maßstab), „Bildrate 30 (konstant)“.
- 0.1.8: Renderer in der Weitsicht 2–3,5× schneller (Old-3DS-Profil, Kachelpfade, zweistufiger
  Compositor, NEON, `-O3`), Tempo-Schutz (`PaceAllowsPresent`: liegt das Spiel ≥ 2 Takte
  zurück, wird ein Bild nur berechnet, nicht gezeichnet; Log `catchup=`). Mikes Log:
  überall 59,9 FPS, ppu max ~15 ms.
- 0.1.9 (letzte Version, gepusht als `254fe7e`):
  - Nebel/Lichtstrahlen (BG3 per HDMA) über das ganze Weitsicht-Bild
    (`virtuappu_mode1_bg3_hdma_extend`, HDMA-Tabelle auf Bildhöhe gestreckt).
  - Doppelte Textbox („Gegenstand erhalten“) behoben – Band folgt `gTextRender.message`.
  - Minish-Portal: Drehung bleibt in der Weitsicht (PL_USE_PORTAL erlaubt camera_target = NULL);
    Nahaufnahme (`GAMEMAIN_MINISHPORTAL`) fällt im ersten Frame auf 240×160 zurück
    (`Port_WidescreenNativeScene`), BG-Puffer werden in `src/game.c` neu gefüllt;
    die Fall-Szene (Subtask 6) behält den Zoom. Bei Ansichtswechsel mit gehaltener
    Kamera bleibt die Bildmitte erhalten (`Port_WidescreenSnapGameplayCamera`).
  - Breitbild: Port rechnet mit den 266 Spalten, die die Vita zeigt (vorher 282 → R-Prompt
    abgeschnitten, Link versetzt), `PublishWideWindow` in `port_ppu_vita.c`.
  - Overlay-Detailkarte: 17 Gebietskarten zu einer Karte zusammengesetzt (Nähte gefüllt),
    Zoom 2×/3× (R/L, „ZOOM“), Verschieben per Touch/Steuerkreuz/Stick, Tipp auf Gebiet springt hin.
  - Vita-Linker-Skript (s. o.), neue Host-Hooks (TMC_WARP_AT, TMC_VIEW_LOG, TMC_DETAIL_DUMP, TMC_BG0_DUMP).

- 0.1.10: **Fremde Kacheln im Dorf der Minish behoben.** Ursache war nicht die
  Schatten-Tilemap: `minishVillageTileSetManager.c` tauscht je nach sichtbarer Region den
  ganzen BG-Kachelsatz (VRAM 0x0000–0x3FFF + 0x8000–0xBFFF) und die Paletten 4–14
  (5 Gruppen). 240×160 zeigt nie zwei Gruppen, 400×240 schon. Lösung: pro Schattenzelle
  ein Kontext (`virtuappu_mode1_ws_alt_ctx`, 0 = VRAM, 1..3 = Kachelsatz aus ROM +
  Palette, erweiterte Palettenbänke 16k..; Compositor-Codes: Layer jetzt ab Bit 12).
  Zelle→Gruppe: Region-Rechteck, sonst die Gruppe, die der GBA für 240×160-Ansichten mit
  dieser Zelle lädt (`BuildCellGroups`). Ohne Kontexte bitgleich (Regression + Fuzzer).
  Host-Repro: `TMC_BOOT=1,0,158,78` bzw. `1,0,270,1d0`, `1,0,1f0,a0` (vorher Müll/Balken).
  Bootstrap lädt libelf jetzt aus vitasdk/artifacts (deb.debian.org ist in Claude-Cloud
  gesperrt). Hinweis: Nach `TMC_BOOT` reagiert Link nicht auf Skript-Tasten (synthetischer
  Spielstand) – für Lauftests Mikes Quick-State nehmen.

## Offene Punkte (nächste Schritte)
1. **Hyrule-Stadt prüfen:** `hyruleTownTileSetManager.c` tauscht ebenfalls Kachelsätze je
   Region (Lücke zwischen Regionen nur 128 px) – in der Weitsicht vermutlich derselbe
   Fehler. Mit synthetischem Spielstand nicht testbar (weißer Bildschirm), braucht Mikes
   Quick-State. Code ist schon angebunden (`HyruleTownTileSetManager_ReportWideView`,
   3 Slots, Orakel-Haus-Sonderfall), aber noch nicht im Bild geprüft.
2. **Pausemenü-Status:** halb verdecktes Element (Symbol + Ziffern) unter „SPEICHERN“ unten
   rechts, halb unter der Beschreibungsleiste. Mit Mikes Spielstand auf dem Host nicht sichtbar.
3. ~~Ansicht-Auswahl~~ erledigt in 0.1.10: Menü „Ansicht: GBA / Breitbild / Weitsicht“
   (ini-Schlüssel unverändert `widescreen`/`full_view`), frische Installation = Weitsicht.
4. Kosmetik/Leistung: Iris- und andere GBA-Fenstereffekte erzwingen die normale Ansicht;
   Raum-Scroll-Übergänge zeigen kurz 266×160; generischer Compositor (Affin-BG-Zeilen,
   z. B. Deepwood 48/20) ist noch der langsame Pfad; Overlay-Hintergrund nicht gecacht
   (~17 M Instr. pro Malen).
5. Bekannt, nicht Vita-spezifisch: Teleport-Cheat nach Hyrule-Stadt im Prolog → weißer
   Bildschirm; `ResolveRomPtr … outside ROM` beim Übergang Hyrule-Stadt → Festival (harmlos);
   Räume 67/04 und 8a/02 stürzen nur per Debug-Warp ab.

## Weitere Doku
Ausführlichere Notizen liegen im claude.ai-Projekt „3ds/psp/vita modding“
(`minish-cap-vita/technik.md`). Diese Datei hier ist der maßgebliche Einstieg für Claude Code.
