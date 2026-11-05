# Version 1.2 — November 5, 2026

The first release of Producdevity's build. It installs next to the original, the movies stop stuttering, and heavy scenes run a lot better.

### New
- **Installs next to the original.** Its own bubble, Super Mario Galaxy (Producdevity), title ID `PDEV00001`. It reads the same game files and HD pack, so there's nothing to prepare again, and keeps its saves, settings and compiled graphics in `ux0:data/smg-producdevity`. The first start copies your saves and settings from the original build. The original and its saves aren't changed.
- **Right stick pointer.** Press Square in the pause menu to switch the right stick between the camera and the pointer. The D-pad stays the camera.
- **More graphics effects compiled ahead of time:** 1,405, up from 1,131.

### Fixed
- **Movie sound.** The opening movies no longer stutter. All three picture decoders now get a CPU core, the movie file is read ahead, and the sound keeps frames in hand. Measured on a PS Vita with no overclock: 0 silent blocks of sound in 11,437 through both opening movies, where 559 of 7,211 were silent in the second one before.
- **Low CPU clock on some launches.** With a PSVshell profile, the game's clock request could race the plugin and leave the CPU at 444 MHz for the whole session. The request now waits until the plugin is done. No low launches in 92 test launches.

### Faster
- **The frame loop** no longer waits for the renderer to read each picture, and a turn without a picture skips work no picture would show.
- **Collision, shadows and gravity** repeat less work: a shadow's line finds its earlier hits again, line searches stop at the nearest hit, and gravity fields out of reach are rejected early.
- **Effects, matrices and the GX layer** do less per frame.
- **Audio mixing** has a faster resampler and AFC decoder.
- **Execution profile.** The game, platform and renderer code are compiled with a profile of real play.
- **A second skipped picture** that would only pay back a sliver of time is forgiven.
- **Automatic quality** steps down in play when pictures fall under 30 a second, and leaves movies alone.

### Measured on a PS Vita
This version, no overclocking plugin, Mario moving, last 30 seconds of a scripted run: Good Egg 47.9 fps, Honeyhive 37.0, Toy Time 31.9, Comet Observatory 31.4, Beach Bowl 24.7. The game kept 58.5 to 60.2 of its 60 updates a second.

Coming from the original build? Install the VPK. It lands next to the old one. The first start takes about 4 minutes while the graphics are unpacked.

# Version 1.1 — October 26, 2026

Mario's eyes, a Settings menu, and a steadier start.

### New
- **Settings menu.** Tap **Settings** on the game's LiveArea screen, before launching or while playing. It is drawn in the game's own fonts. Choose:
  - **Controls.** Remap every button for the Vita or for a PlayStation controller on a PS TV, with a picture of the controller.
  - **Picture rate.** Up to 60 FPS, a fixed 30, or **Steady 30**: an even 30 FPS that lowers the resolution and effects quality when needed. For players who prefer a steady picture to the highest frame rate.
  - **Dynamic quality** and **reduced motion.**
  - **Game language,** from the languages your game files include.
  
  Choices are saved in `ux0:data/smg/player-settings.cfg`. Some apply after a restart, which the menu offers. Your saves are never touched.
- **Title-screen loading picture,** with a loading bar that shows real progress.

### Fixed
- **Mario's eyes.** Players with the optional HD textures saw his eyes white, or his eyelids closed all the time. Eyes now keep the game's own pictures, so they show and blink normally. The rest of the HD pack still applies.
- **Blue eyes without the HD pack too.** A colour calculation that the Wii wraps around was being clipped, which could leave his irises white. It now matches the Wii.
- **Start-up crash guarded.** A rare crash in the first seconds of a launch came from a file seek on a handle that was not open. That seek now fails safely.

### Faster
- **Quicker sound mixing.** The game's audio is now mixed with the Vita's vector unit, which frees a little time on the game's busiest thread. The sound is bit-for-bit the same.

### Easier to help with
- **A slow start tells you.** If loading takes more than 90 seconds, the screen says so and points you to the game files folder.
- **A fuller log.** `log.txt` (in `ux0:data/smg`) records each loading stage and when the title was reached. Each 5-second line shows how many shaders were prepared.

Updating? Install the new VPK over the old one. Your saves are kept. The first start after updating takes a little longer while the game prepares its updated shaders.

# Version 1.0 — October 2026

Super Mario Galaxy, running natively on the PS Vita.

### Measured on a PS Vita
- **51 fps** on average in Good Egg Galaxy, **43 fps** in the Comet Observatory (the busiest place in the game), a full 60 in quieter moments.
- **0** stutters longer than a tenth of a second while playing, and **0** picture-size changes in the middle of play, in every test run.

### Touch controls and gameplay
- **Rear touch Star Bit aiming.** Slide on the rear pad to move the Star Bit pointer, like a trackpad, with your thumbs still on the sticks. Resting fingers are ignored. Turn it on in the pause menu with Triangle.
- **Settings panel in the pause menu,** drawn in the game's own fonts and window style: camera zoom with L and R (1.0× to 2.0×, 1.5× by default) and rear touch aiming with Triangle, with the game's own menu sounds.
- **Touch everywhere you used to point.** Tap to collect and choose, and hold still to grab Pull Stars. On menus, the left stick and Cross work too.
- **Tilt steering** for the Star Ball and the Manta ray, levelled to how you hold the Vita. Select resets it.
- **Vita button prompts** throughout the game's text and pictures.

### Smoother play
- **No resolution hitches.** The picture saves its sharpness changes for pauses and screen fades, and each galaxy remembers what works best for it.
- **Three cores at work.** The game, the graphics preparation and the GPU feed each run on their own Vita core, so busy scenes like the Comet Observatory keep moving.
- **Faster graphics engine.** Repeated graphics state is remembered instead of re-read every frame, and big scenes need fewer GPU passes.
- **Effects ready ahead of time.** 1,131 graphics effects ship already compiled for the Vita, so new ones load instantly instead of pausing the game.

### Better picture
- **Sharper than the Wii.** The game renders at up to the Vita's full 960×544 when there's room, with anti-aliased edges on top, and a sharpened upscale when it renders smaller.
- **Optional HD characters and items.** Sharper textures for Mario, Luigi, Peach, Rosalina, the Lumas, Toads, Bowser, Bowser Jr., the Power Stars, launch stars and power-ups, made on your computer from your own game files. [How](tools/hdtex/README.md).
- **Full-screen 16:9** by default.

### Under the hood
- Shares its graphics engine work with our Mario Strikers Charged port: safe texture clean-up while a scene is drawing, a large shader-program cache, and careful video memory use.
- A short log in `ux0:data/smg/log.txt` (frame rate, stutters, freezes, picture changes, errors) to attach to bug reports.

The full engineering list is in [docs/TECHNICAL.md](docs/TECHNICAL.md#whats-in-version-10).

