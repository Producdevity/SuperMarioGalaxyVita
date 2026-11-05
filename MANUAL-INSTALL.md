# Manual Install: Windows, Mac Or Linux

> **Most people should use [CBOC Vita Installer](EASY-INSTALL.md) instead.** It does every step below for you, with nothing to compile or install: pick Super Mario Galaxy, choose your disc, press Install. Use this guide only if you can't or don't want to use it.

**You don't need CBOC Vita Installer to install or play this game.** The VPK
runs on the Vita by itself, and any computer can prepare and copy the files.
Once it's installed, no computer is needed. Prefer one click?
See [Easy Install](EASY-INSTALL.md).

Use `SuperMarioGalaxyVita-Producdevity.vpk`, title ID `PDEV00001`. It installs next to the
original build (`SMGV00001`) and reads the same game files.
[Start Here](START-HERE.txt) is the short version.

## You Need

- A homebrew-enabled PS Vita with VitaShell.
- Your own Super Mario Galaxy disc image: **Wii, USA (`RMGE01`)**, the original release (revision 0).
- `libshacccg.suprx` in `ur0:data/` (the shader compiler most vitaGL games need).
- About 2.5 GB free on the Vita (about 3.5 GB if you keep the movies full size; see
  [Prepare Your Game](MANUAL-PREPARE-GAME.md)).
- Python 3 on your computer, for the game preparation, and a C++ compiler with
  libjpeg for shrinking the movies (the guide shows how to get them on Mac,
  Windows and Linux).

## Installation

1. Download [the VPK](https://github.com/Producdevity/SuperMarioGalaxyVita/releases/latest/download/SuperMarioGalaxyVita-Producdevity.vpk),
   or the [complete install ZIP](https://github.com/Producdevity/SuperMarioGalaxyVita/releases/latest/download/SuperMarioGalaxyVita-Producdevity-install.zip)
   for the VPK plus guides, tools and source. **Don't unzip the VPK.**
   [Download checks](docs/CHECK-DOWNLOAD.md) are optional.
2. Prepare your game files on your computer with [Prepare Your Game](MANUAL-PREPARE-GAME.md):
   extract your disc, convert it, and **shrink the movies** so they play smoothly
   ([step 3](MANUAL-PREPARE-GAME.md#3-shrink-the-movies-recommended)). You end up with a
   `prepared-game` folder holding `sys/` and `files/`.
3. Use one of the transfer methods below. Copy `sys/` and `files/` into
   `ux0:data/smg/game/`, and copy `SuperMarioGalaxyVita-Producdevity.vpk` into `ux0:VPK/`. Create
   missing folders.
4. Check for `ur0:data/libshacccg.suprx` in VitaShell. If it's missing, copy
   your own file there. **USB and card readers only show `ux0:`**, so put the
   file in `ux0:VPK/` first, then in VitaShell use **Triangle > Copy**, open
   `ur0:data/` and use **Triangle > Paste**.
5. In VitaShell, open `ux0:VPK/SuperMarioGalaxyVita-Producdevity.vpk`, press Cross and confirm.
   Launch the Super Mario Galaxy (Producdevity) bubble. [Controls](CONTROLS.md) has the layout.

The first start takes about 4 minutes while the graphics are unpacked. It only happens once.

## Choose A Transfer Method

**USB:** In VitaShell, press START, set **SELECT button** to **USB**, and pick
the **USB device** that is your `ux0:` storage (for example SD2Vita). Close
the settings, connect a data USB cable and press SELECT. The drive that opens
on your computer is `ux0:`: use `data/smg/game/` and `VPK/` on it, not a new
folder called `ux0`. Eject the drive safely before you leave USB mode.

**Wi-Fi / FTP:** Put the Vita and the computer on the same network. In
VitaShell press START, set **SELECT button** to **FTP**, then press SELECT.
In an FTP client, connect to the address and port the Vita shows (FTP, not
SFTP), open `ux0:` and copy to the folders above. Keep VitaShell open until
everything has finished. USB is much faster for the 2.5 GB of game files.

**Memory card reader:** Only if your Vita's card (such as its SD2Vita microSD)
already works in your computer. Turn the Vita fully off before taking it out,
copy to `data/smg/game/` and `VPK/` at the card's root, eject it safely and put
it back. Never format the card for this.

## Folder Check

```text
ux0:data/smg/
  game/
    sys/
      fst.bin
      GalaxyID.bcsv       and the other prepared system files
    files/
      ObjectData/
        _bk00/ ... _bk57/ the bucket folders the preparation makes
      StageData/
      ...
  hdtex.pack              optional HD textures
ux0:data/smg-producdevity/
  saves/                  your saves (made by the game)
  settings.ini            your pause-menu settings (made by the game)
  shadercache/            compiled graphics (made by the game)
```

`game/sys/fst.bin` must not sit in `game/game/` or `game/DATA/`. The `_bkNN`
folders are meant to be there: don't flatten or rename them.

## Coming From The Original Build

Install the VPK. It lands next to the original, uses the same `game/` folder, and
copies your saves and settings on its first start. The original build and its
saves stay exactly as they were, so you can keep both.

## Updating

Install the new VPK over this build's old one. That's all: your game files, saves,
settings and compiled graphics stay where they are.

| Item | What to do |
| --- | --- |
| `SuperMarioGalaxyVita-Producdevity.vpk` | Install it in VitaShell over the old one |
| `game/` | Keep. Only copy it again for a first install or a repair |
| `saves/`, `settings.ini` | Keep. Back them up now and then |
| `shadercache/` | Keep. The game updates it by itself |
| `env.txt` | Optional switches. Only keep lines you added on purpose |

## If It Doesn't Start

- **Missing game files:** check that `ux0:data/smg/game/sys/fst.bin` exists and
  that you copied the *prepared* files, not the raw extraction.
- **Shader compiler error:** check `ur0:data/libshacccg.suprx`.
- **Stuck on the first loading screen:** give the first launch a minute. If it
  never finishes, see [Known Issues](KNOWN-ISSUES.md).

When you report a problem, say which build you used and what you saw. Never
upload your disc image, game files or saves.
