# We Become What We Behold -- Wii U

A native Wii U port of Nicky Case's [*We Become What We Behold*](https://ncase.itch.io/wbwwb),
written in C++17 with [wut](https://github.com/devkitPro/wut) and SDL2.

It reimplements the original JavaScript tick for tick: the same fixed 60 Hz update, the same
timers and photo/director timing, the same strings and sprites. The one visible difference is
that text uses a built-in 5x7 pixel font instead of the original webfont.
## Build

Needs devkitPro (devkitPPC + `wut`) and the Wii U SDL2 portlib. In devkitPro's MSYS2 shell:

```
pacman -S wut wiiu-sdl2
```

Then, from this directory:

```
make
```

This produces `wbwwb.wuhb` -- the executable plus all art and audio in one file. `make clean`
removes it.

## Install

Copy it to the SD card as:

```
sd:/wiiu/apps/wbwwb/wbwwb.wuhb
```

Launch it from the Homebrew Launcher, or straight from the Wii U Menu: Aroma's bundled
`homebrew_on_menu` plugin scans `sd:/wiiu/apps` and puts every `.wuhb` on the home screen. If
the game doesn't appear there, open the plugin config with **L + D-Pad Down + Minus** and make
sure *Hide all homebrew* is off. Nothing is installed, so deleting the folder removes the icon.

## Controls

| Input | Action |
| --- | --- |
| Left stick or D-Pad | move the camera |
| A / click / touch / Space | take the photo |
| Escape | quit |

Wii Remote, nunchuk, Classic Controller, Pro Controller and GamePad all work.

## Layout

| Path | What |
| --- | --- |
| `src/main.cpp` | the whole port |
| `src/generated_sprites.h` | sprite atlas rectangles (generated) |
| `assets/` | the art and audio packed into `/vol/content` |
| `convert-assets.ps1` | rebuilds `assets/*.bmp` and the header from the original sprites |
| `docs/PORTING-NOTES.md` | JavaScript-to-C++ mapping, bugs found, remaining limitations |

The converted assets are committed, so building needs no image tooling. To regenerate them, copy
the original game's `sprites/` folder here, install ImageMagick 7 and run
`pwsh ./convert-assets.ps1`. The WAVs were converted once and are not regenerated.

## Licence

Port code is MIT (`LICENSE`). The game's code, art, text and most sounds are CC0 by Nicky Case;
four sounds are CC BY and must keep their attribution -- see `CREDITS.md`.
