# Credits and licensing

## The game

**We Become What We Behold** is by **Nicky Case** -- <https://ncase.itch.io/wbwwb> --
source at <https://github.com/ncase/wbwwb>.

> "I'm releasing all my code and art to the public domain, under the
> [Creative Commons Zero](http://creativecommons.org/publicdomain/zero/1.0/) un-license."

So the game's code, sprites, text and most of its audio are **CC0** (public domain):
the port may redistribute them, and you may reuse anything of theirs in turn.

## This port

The Wii U port (`src/`, `Makefile`, `convert-assets.ps1`) is **MIT** -- see `LICENSE`.
It is an independent C++ reimplementation of the original JavaScript, not a modification
of it, but every behaviour, string, sprite and sound is taken from the original.

## Sounds

The original's audio credits, which still apply to the converted WAVs in `assets/audio/`:

| Sound | Source | Licence |
| --- | --- | --- |
| squeak | [ermfilm](https://www.freesound.org/people/ermfilm/sounds/130011/) | CC BY |
| single cricket | [cs272](https://www.freesound.org/people/cs272/sounds/77034/) | CC BY |
| bloody impact | [Hybrid_V](https://www.freesound.org/people/Hybrid_V/sounds/319590/) | CC BY |
| creepy warp | [Andromadax24](https://www.freesound.org/people/Andromadax24/sounds/184476/) | CC BY |
| park ambience | [Mafon2](https://www.freesound.org/people/Mafon2/sounds/274175/) | CC0 |
| camera shutter | [uEffects](https://www.freesound.org/people/uEffects/sounds/207865/) | CC0 |
| multiple crickets | [alienistcog](https://www.freesound.org/people/alienistcog/sounds/124583/) | CC0 |
| news jingle | [Tuben](https://www.freesound.org/people/Tuben/sounds/272044/) | CC0 |
| scream #1 | [GreatNate98](https://www.freesound.org/people/GreatNate98/sounds/353086/) | CC0 |
| scream #2 | [mariallinas](https://www.freesound.org/people/mariallinas/sounds/222649/) | CC0 |
| gunshot | [mitchelk](https://www.freesound.org/people/mitchelk/sounds/136766/) | CC0 |
| gun cocked | [martian](https://www.freesound.org/people/martian/sounds/182229/) | CC0 |
| shotgun | [lensflare8642](https://www.freesound.org/people/lensflare8642/sounds/145209/) | CC0 |
| crowd screaming | [MultiMax2121](https://www.freesound.org/people/MultiMax2121/sounds/156860/) | CC0 |

The four **CC BY** sounds must keep their attribution wherever this port or its assets
are redistributed.

## Art

The ending's laptop is a modified [photo by Unsplash](https://unsplash.com/photos/XyNi3rUEReE) (CC0).
Everything else is Nicky Case's, CC0.

## Not included

The original's web stack (PIXI.js, Howler.js -- both MIT) and the Cairo webfont are not used
by this port and are not part of it. The text is drawn with a built-in 5x7 pixel font.

SDL2 (zlib licence) and devkitPro's wut are build-time dependencies only and are not
redistributed here.

## Disclaimer

Not affiliated with or endorsed by Nintendo. Wii U is a trademark of Nintendo.
This is free homebrew; do not sell it.
