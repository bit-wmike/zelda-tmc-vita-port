# The Minish Cap – PS Vita

Native PlayStation Vita port of *The Legend of Zelda: The Minish Cap* (GBA, 2004).
It is not an emulator: the game is compiled from the open-source decompilation
and runs as native ARM code on the Vita.

**No ROM is included.** You need your own legally obtained Game Boy Advance ROM
(Europe or USA). No extracted Nintendo graphics, text or audio are distributed here.

> **Status: early development (0.1.x).** Tested on a PS Vita Slim (PCH-2000) with
> HENkaku. Expect bugs; please report them with your `log.txt` (see below).

*Deutsche Anleitung: [platform/vita/README.md](platform/vita/README.md)*

## Features

- Native port at 60 fps; the software renderer uses all three application cores
  (NEON where it helps). The game speed never drops: a frame that takes too long is
  skipped instead of slowing the game down.
- Widescreen at the original pixel aspect ratio, plus Full View: the whole room up to
  400×240 at one constant zoom level, with an optional steady 30 fps mode.
- Display styles: Smooth, Sharp (crisp pixels without shimmer) and Pixel Perfect.
- Map and status overlay (the 3DS bottom screen), usable with the front touch screen.
  The world map zooms into one stitched map of Hyrule (L/R or the ZOOM chip) that you
  move with a finger, the D-pad or the left stick.
- Extra item slots on △ / □ and the right stick; optional rear touch.
- Turbo / fast-forward, text fast-forward, autosave.
- *Minish Cap Reborn* comfort options (Pegasus Boots on L+R, Ocarina shortcut,
  360° movement, Hero Mode …).
- Cheat menu with toggles (infinite hearts / Rupees / ammo, walk through walls,
  all items, all Kinstones and figurines, teleport …).
- Quick position save/load.
- Europe and USA ROMs with automatic detection, all five European languages,
  and the [European gameplay backports](docs/eu-backport.md).
- Ordinary `.sav` files, compatible with mGBA and VBA.

## Installation

1. Install `TheMinishCap_Vita_<version>.vpk` with VitaShell.
2. Copy your ROM to `ux0:data/tmc/` (any file name ending in `.gba`):
   - Europe (DE/EN/FR/ES/IT), recommended: SHA-1 `cff199b36ff173fb6faf152653d1bccf87c26fb7`
   - USA: SHA-1 `b4bd50e4131b027c334547b4524e2dbbd4227130`
3. Start the game. Choose the language on the file select screen ("Language").

`ux0:data/tmc/` also holds your settings (`tmcvita.ini`), your save (`tmc_eu.sav`
or `tmc.sav`) and the diagnostic logs `log.txt` / `log_prev.txt` (previous run).

## Controls

| Vita | Action |
|---|---|
| ✕ / ○ | A / B |
| L / R, START, SELECT | as on the GBA |
| D-pad / left stick | move (stick: full 360° with the Reborn option) |
| △ / □ | extra item 1 / 2 |
| Right stick ↑ (hold) | turbo / fast-forward |
| Right stick ↓ | map & status overlay |
| Right stick ← / → | extra item 3 / 4 |
| START + SELECT | Vita menu (display, controls, cheats, game) |
| Touch top-left / top-right | map overlay / Vita menu |
| Rear touch top-left / top-right | extra item 3 / 4 (enable in the menu) |
| PS TV: L2 / R2, L3, R3 | extra item 3 / 4, map, menu |

**Extra items:** open the pause menu, highlight an item and press the button you
want it on (△, □, right stick ← / →, a rear touch zone, or L2 / R2 on PS TV).
Holding SELECT + ✕ / ○ also assigns to △ / □.

## Building

See [platform/vita/README.md](platform/vita/README.md). In short, with
[VitaSDK](https://vitasdk.org) installed:

```sh
export VITASDK=/usr/local/vitasdk
cd platform/vita
make TARGET=vita vpk   # -> build-vita/TheMinishCap_Vita_<version>.vpk
```

## Credits

This port stands on the work of:

- [zeldaret/tmc](https://github.com/zeldaret/tmc) — the decompilation of the game
- [Project Picori](https://github.com/999sian/tmc) — native engine, widescreen, port infrastructure
- [samyost1/tmc-android](https://github.com/samyost1/tmc-android) — dual-screen Android port
- [EstebanPdN/zelda-tmc-3ds](https://github.com/EstebanPdN/zelda-tmc-3ds) — the Nintendo 3DS port
  this Vita port is forked from (its README: [docs/README-3ds.md](docs/README-3ds.md))
- [Admentus64/The-Minish-Cap-Reborn](https://github.com/Admentus64/The-Minish-Cap-Reborn) — comfort features
- [vitasdk](https://github.com/vitasdk) and [vita2d](https://github.com/xerpi/libvita2d)

PS Vita port by [bit-wmike](https://github.com/bit-wmike), written with the help of Claude (Anthropic).

## License and legal notice

Distributed under the **GNU GPL v3.0 or later**, like the projects it is based on.
See [LICENSE](LICENSE), [COPYRIGHT.md](COPYRIGHT.md) and
[THIRD-PARTY-LICENSES.md](THIRD-PARTY-LICENSES.md).

Nintendo owns *The Legend of Zelda*, *The Minish Cap* and all associated game content.
This is an unofficial fan project and is not affiliated with or endorsed by Nintendo or Sony.
