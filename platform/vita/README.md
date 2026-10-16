# The Minish Cap – PS Vita Port

Native PS-Vita-Fassung von *The Legend of Zelda: The Minish Cap* (GBA, 2004).
Kein Emulator: Das Spiel läuft als ARM-Code direkt auf der Vita.

Aufgebaut auf:
- [zeldaret/tmc](https://github.com/zeldaret/tmc) – die vollständige Decompilation
- [Project Picori](https://github.com/999sian/tmc) – native Engine, Breitbild, Reborn-Komfortfunktionen
- [The Minish Cap 3DS](https://github.com/EstebanPdN/zelda-tmc-3ds) – 32-Bit-ARM-Basis, EU-Unterstützung, Karten-/Status-Bildschirm

Lizenz: GPL-3.0 wie die Vorlagen. **Kein ROM enthalten.**

## Installation

1. `TheMinishCap_Vita_x.y.z.vpk` mit VitaShell installieren.
2. Dein eigenes ROM nach `ux0:data/tmc/` kopieren. Der Dateiname ist egal (Endung `.gba`).
   - Europa (DE/EN/FR/ES/IT): SHA-1 `cff199b36ff173fb6faf152653d1bccf87c26fb7` (empfohlen)
   - USA: SHA-1 `b4bd50e4131b027c334547b4524e2dbbd4227130`
3. Starten. Die Sprache stellst du im Dateiauswahl-Bildschirm unter „Language“ ein.

Im selben Ordner liegen danach `tmcvita.ini` (Einstellungen), der Spielstand
(`tmc_eu.sav`, kompatibel mit mGBA/VBA) und `log.txt` (bei Problemen bitte mitschicken).

## Steuerung

| Vita | Funktion |
|---|---|
| Kreuz / Kreis | A / B |
| L / R, Start, Select | wie GBA |
| Steuerkreuz / linker Stick | Bewegen (Stick: 360° möglich) |
| Dreieck / Viereck | Zusatz-Item 1 / 2 |
| Rechter Stick ↑ (halten) | Turbo / Vorspulen |
| Rechter Stick ↓ | Karte & Status (Overlay, Touch-bedienbar) |
| Rechter Stick ← / → | Zusatz-Item 3 / 4 |
| Start + Select | Vita-Menü (Optionen, Komfort, Cheats, Speichern) |
| Touch oben links / oben rechts | Karte / Vita-Menü |
| Rückseite oben links / rechts | Zusatz-Item 3 / 4 (muss im Menü eingeschaltet werden) |
| PS TV: L2/R2, L3, R3 | Zusatz-Item 3/4, Karte, Menü |

**Zusatz-Items belegen:** Pausemenü öffnen, ein Item markieren und die gewünschte
Taste drücken (Dreieck, Viereck, rechter Stick ← / →, ein Rückseiten-Feld oder
L2/R2 am PS TV). Alternativ Select halten + Kreuz/Kreis für Dreieck/Viereck.
Die Belegung wird in `ux0:data/tmc/tmc.softslots` gespeichert.

## Vita-Menü

- **Anzeige:** Bildstil (Weich / Scharf / Pixelgenau), Breitbild, Weitsicht (400×240, experimentell),
  GBA-Farbkorrektur, Lautstärke, FPS-Anzeige, Rand-Symbole
- **Steuerung:** Rückseiten-Touch, Turbo-Faktor, Text vorspulen, Auto-Speichern und alle
  „Minish Cap Reborn“-Komfortfunktionen (Pegasusstiefel auf L+R, Okarina auf L+Select,
  360°-Bewegung, Muscheln bis 9999, Heldenmodus …)
- **Cheats:** Unendlich Herzen/Rubine/Munition, durch Wände gehen, alle Items, Herzen/Rubine/Muscheln
  maximal, alle Kinsteine, alle Figuren, Minish-Größe, Teleport
- **Spiel:** Spielstand speichern, Position merken/laden, zurück zum Titel, beenden

Cheats gelten nur für die laufende Sitzung und werden nicht gespeichert.

## Bauen

```sh
# einmalig: VitaSDK (falls nicht vorhanden)
sudo bash platform/vita/tools/bootstrap_vitasdk.sh
export VITASDK=/usr/local/vitasdk
cd platform/vita
make TARGET=vita vpk        # -> build-vita/TheMinishCap_Vita_<version>.vpk
make TARGET=host            # 32-Bit-Linux-Testbuild (headless, für automatische Tests)
```

Der Host-Build läuft mit `build-host/tmc_host rom.gba <frames> <ausgabeordner> [skript]`
und speichert Einzelbilder (`TMC_DUMP=100,200`). `TMC_BOOT=area,room,x,y` springt direkt ins Spiel.
Das Skript enthält Zeilen `<frame> <tastenmaske-hex>` (Bits wie `TMC_KEY_*` in `common/tmc_platform.h`).

Weitere Test-Schalter: `TMC_LANG=4` (Deutsch), `TMC_CHEAT_ALL=1`, `TMC_AUDIO_SYNC=1`
(bit-genaues Audio), `TMC_QSAVE_AT` / `TMC_QLOAD_AT` (Quick-State), `TMC_OVERLAY_DUMP=<frame>`,
`TMC_TEXT_DUMP=<textblock>` (Originaltexte aller Sprachen ausgeben),
`TMC_MSG=0x2c25 TMC_MSG_AT=<frame>` (Textbox anzeigen).

### Differentialtest (Vita-Adressen)

Auf der Vita liegt der ganze Speicher über `0x80000000`. Code aus der GBA-Zeit, der
Zeiger als Vorzeichenzahl prüft, verhält sich dort anders. `TARGET=armhost` (gelinkt
auf `0x81000000` wie die Vita) und `TARGET=armlow` (normale Adressen) laufen unter
`qemu-arm`; `tools/difftest.sh` spielt dieselben Eingaben in beiden ab und vergleicht
jedes Bild, den Ton und den Spielstand:

```sh
make TARGET=armhost && make TARGET=armlow
ROM=/pfad/rom.gba tools/difftest.sh walk 900 skript.txt TMC_AUDIO_SYNC=1 TMC_DUMP=300,400,500
```

Wichtig: Der Host-Compiler ist nachsichtiger als die Vita-Toolchain (GCC 15). Vor
jeder Version deshalb immer auch `make TARGET=vita` bauen.

## Eigene LiveArea-Bilder

Bilder für Icon, LiveArea-Hintergrund und Startkachel kommen aus `sce_sys_local/`
(`icon0.png` 128×128, `livearea/contents/bg.png` 840×500, `livearea/contents/startup.png`
280×158, jeweils PNG mit 8-Bit-Palette). Der Ordner ist von Git ausgenommen, damit
kein fremdes Artwork im Repo landet. Fehlt er, nimmt der Build die Platzhalter aus `sce_sys/`.

## Log

`ux0:data/tmc/log.txt` (vorheriger Start: `log_prev.txt`) enthält Version, Firmware,
Modell, Speicher, Einstellungen, alle 5 Sekunden `[perf]` (FPS, Ruckler, Zeiten für
Logik/Bildaufbau/Ausgabe, Audio) sowie Szenen- und Menü-Ereignisse. Bei Problemen
bitte beide Dateien mitschicken.

## Bekannte Einschränkungen

- Teleport-Cheat nach Hyrule-Stadt während des Prologs ergibt einen weißen Bildschirm
  (auch in den anderen Ports so).
- Der Build nutzt die USA-Basis mit Laufzeit-Erkennung der EU-ROM (wie der 3DS-Port).
  Einige kleine Details folgen noch der US-Fassung (z. B. eine Kachel in der
  Portal-Nahaufnahme, 4 Pixel Startposition im Wald-Labyrinth des Königstals).
