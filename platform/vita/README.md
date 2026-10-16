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
| Dreieck / Viereck | Zusatz-Item 1 / 2 (im Pausemenü **Select halten** + Kreuz/Kreis zum Belegen) |
| Rechter Stick ↑ (halten) | Turbo / Vorspulen |
| Rechter Stick ↓ | Karte & Status (Overlay, Touch-bedienbar) |
| Rechter Stick ← / → | Zusatz-Item 3 / 4 |
| Start + Select | Vita-Menü (Optionen, Komfort, Cheats, Speichern) |
| Touch oben links / oben rechts | Karte / Vita-Menü |
| Rückseite oben links / rechts | Zusatz-Item 3 / 4 (muss im Menü eingeschaltet werden) |
| PS TV: L2/R2, L3, R3 | Zusatz-Item 3/4, Karte, Menü |

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
