# Known Issues

Thanks for playing! If something here bothers you, or you find something new, open an [issue on GitHub](https://github.com/Producdevity/SuperMarioGalaxyVita/issues).

## Performance
- **Beach Bowl Galaxy runs below 30 fps without an overclock,** at about 25. The game keeps its speed, so it looks less smooth rather than slower. The Comet Observatory and Toy Time sit just above 30.
- **The biggest boss fights are heavy too.** The picture steps down a little there on its own.
- **PSVshell profiles don't carry over from the original build.** They're per title, so make one for this build if you use it.
- **A galaxy's first visit starts at standard sharpness.** It moves up at your first pause if the Vita has room, and remembers that afterwards.
- **The first start takes about 4 minutes** while the graphics are unpacked, with a progress bar on screen. It only happens once, and the next start reaches the graphics in under a second.

## Reported And Not Fixed Yet
- **Crashes around water.** People reported crashes getting in or out of water in Beach Bowl and Deep Dark, and while racing Guppy in Sea Slide ([#13](https://github.com/CBOC-Vita-Lab/galaxy-vita/issues/13), [#19](https://github.com/CBOC-Vita-Lab/galaxy-vita/issues/19), [#23](https://github.com/CBOC-Vita-Lab/galaxy-vita/issues/23)). This version doesn't fix them, and they weren't re-tested.
- **Music can stop after the console sleeps** or you leave the game with the PS button ([#7](https://github.com/CBOC-Vita-Lab/galaxy-vita/issues/7)).
- **Some visual glitches** from the original build are still open, such as black flashing ground in Matter Splatter ([#21](https://github.com/CBOC-Vita-Lab/galaxy-vita/issues/21)) and Bowser's model twisting in cutscenes ([#11](https://github.com/CBOC-Vita-Lab/galaxy-vita/issues/11)).

## Gameplay
- **Rear touch is off by default**, so fingers holding the Vita never move the pointer by accident. Turn it on in the pause menu with Triangle.
- **A brief pause the first time some effects appear.** The Vita prepares each new graphics effect once, which can take a fraction of a second. The package comes with the effects of the whole game prepared ahead, and anything new is kept, so it happens at most once.

## Install
- The game needs your own `RMGE01` (Wii, USA) disc image. Other regions aren't supported yet.
- `libshacccg.suprx` must be set up on the Vita once (the installer and [Install](MANUAL-INSTALL.md) explain how).
- **Preparing the game by hand needs Python 3** on your computer, and shrinking the movies by hand needs a C++ compiler; full-size movies play less smoothly, so follow step 3 of [Prepare Your Game](MANUAL-PREPARE-GAME.md). The installer does both for you.

## If Something Goes Wrong
- **Something looks or runs wrong:** the game keeps a short log in `ux0:data/smg-producdevity/log.txt` (and the session before it in `log.txt.prev`): the frame rate and any stutters every few seconds, picture changes, settings and errors. Attach it to your [issue](https://github.com/Producdevity/SuperMarioGalaxyVita/issues).
- **The picture keeps changing sharpness, or you want it fixed:** add `GXV_AUTOQ=0` to `ux0:data/smg-producdevity/env.txt` to keep the standard picture.
- **The HD character textures don't look right:** delete `ux0:data/smg/hdtex.pack`, or add `GXV_HDTEX=0` to the same file, to use the originals.
