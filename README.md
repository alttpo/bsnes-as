AngelScript
===========
I have embedded the [AngelScript v2.33.0](https://www.angelcode.com/angelscript/) engine into the bsnes code and created some rudimentary script function bindings between the bsnes emulator and AngelScript scripts.

As a working proof of concept of the AngelScript scripting engine and the basic bindings set up, I've developed a script targeted at *Zelda 3: A Link To The Past* that draws white squares around in-game sprites as they are defined in RAM. To test this proof of concept:

1. clone this repository
2. `cd` to repository working copy directory
3. `git checkout 00cf4de1111d8cda3c4c0f7942390b25492ca958`
4. `make -C bsnes`
5. `./test.sh`

You will have to modify `test.sh` to override the location of your ALTTP ROM file as it's highly unlikely to be identical to mine.

![screenshot](screenshots/alttp-rectangles.png)

Example script:
```
// AngelScript for ALTTP to draw white rectangles around in-game sprites
uint16   xoffs, yoffs;
uint16[] sprx(16);
uint16[] spry(16);
uint8[]  sprs(16);
uint8[]  sprt(16);

void main() {
  // get screen x,y offset by reading BG2 scroll registers:
  xoffs = SNES::Bus::read_u16(0x00E2, 0x00E3);
  yoffs = SNES::Bus::read_u16(0x00E8, 0x00E9);

  for (int i = 0; i < 16; i++) {
    // sprite x,y coords are absolute from BG2 top-left:
    spry[i] = SNES::Bus::read_u16(0x0D00 + i, 0x0D20 + i);
    sprx[i] = SNES::Bus::read_u16(0x0D10 + i, 0x0D30 + i);
    // sprite state (0 = dead, else alive):
    sprs[i] = SNES::Bus::read_u8(0x0DD0 + i);
    // sprite kind:
    sprt[i] = SNES::Bus::read_u8(0x0E20 + i);
  }
}

// draw a horizontal line from x=lx to lx+w on y=ty:
void hline(int lx, int ty, int w, uint16 color) {
  for (int x = lx; x < lx + w; ++x)
    SNES::PPU::frame.set(x, ty, color);
}

// draw a vertical line from y=ty to ty+h on x=lx:
void vline(int lx, int ty, int h, uint16 color) {
  for (int y = ty; y < ty + h; ++y)
    SNES::PPU::frame.set(lx, y, color);
}

void postRender() {
  for (int i = 0; i < 16; i++) {
    // skip dead sprites:
    if (sprs[i] == 0) continue;

    // subtract BG2 offset from sprite x,y coords to get local screen coords:
    int16 rx = int16(sprx[i]) - int16(xoffs);
    int16 ry = int16(spry[i]) - int16(yoffs);

    // draw white rectangle:
    hline(rx     , ry     , 16, 0x7fff);
    vline(rx     , ry     , 16, 0x7fff);
    hline(rx     , ry + 15, 16, 0x7fff);
    vline(rx + 15, ry     , 16, 0x7fff);
  }
}
```

The final script bindings in bsnes will likely not look like those demonstrated here. I will introduce better drawing commands as well as some pixel-font text rendering support.

My Goals
--------
I want to create an emulator add-on defined entirely via scripts that enables a real-time networked multiplayer experience for *Zelda 3: A Link To The Past*. This game has an active and vibrant community surrounding it with things like Randomizer, Crowd Control, and Tournaments.

There appears to be interest in a multiplayer aspect to the game where players can see and interact with one another within the same game world. Some efforts have been made already in this space like with inventory sharing but as far as I'm aware, not to the extent of letting players visually see each other's characters.

The lack of the visual aspect of players seeing each other could be explained by the difficulty in displaying more than one copy of Link on the screen at one time with differing animation frames. CGRAM is dynamically memory mapped and the mapping (and thus the character data) changes depending on the current game state, e.g. Link's current animation frame. If one were to simply draw multiple copies of Link on the screen, they would all appear synchronized in animation.

My plan for the visual aspect is essentially to synchronize over the network, each player's Link state (x,y coords, overworld room number, dungeon room number, light/dark world, etc.) as well as CGRAM data. This extra CGRAM data could be used in a script-extended PPU renderer to essentially have more than one OBJ layer, possibly even one per player. The script-extended PPU rendering would source CGRAM data from script variables instead of from the local game ROM.

My near-term goals are to extend bsnes enough through scripts to allow for customization of the PPU rendering per scanline to get a proof of concept going.

Why AngelScript?
----------------

I came up with an ideal set of criteria for a scripting language to adhere to in order to seamlessly integrate with bsnes:

1. Easy integration with an existing C/C++ codebase (bsnes)
  1. Should be available for direct embedding within bsnes or at least accessible via static library
  1. If directly embeddable, must seamlessly integrate with existing build system (bsnes's composed `GNUmakefile`s)
2. Low developer overhead
  1. Should be easy to define interfaces, functions, and properties for bsnes to expose to scripts and vice-versa
  1. Avoid proxy code and data marshalling at runtime used for calling functions bidirectionally
3. Low runtime overhead
  1. Script functions should ideally have compatible calling conventions with bsnes (CDECL or THISCALL) to cut down on per-call overhead
4. Fast performance since script functions will likely be called at least once per frame
5. Familiar script syntax and similar language conventions to C/C++ code to avoid surprises for script developers
  1. Primitive data types can be shared between bsnes and scripts to avoid proxying and marshalling
  1. Must have first-class support for native integer types (int, uint, uint8, uint16, uint32, etc.)
  1. Must have first-class support for binary operations on integer types (and, or, not, xor, bit shifting, etc.)
  1. Must have support for array types; array indices must start at `0` (looking at you, Lua)
6. Script language MUST NOT be allowed to access anything outside of its scripting environment unless explicitly designed for and controlled by bsnes
  1. No external `.dll`/`.dylib`/`.so` dynamic library loading unless explicitly allowed by bsnes
  1. No access to filesystem unless explicitly allowed by bsnes
  1. No access to network or external peripherals unless explicitly allowed by bsnes
7. Supports building a script debugger within bsnes
8. Ideally statically typed with good compiler errors and warnings

As you can already guess, AngelScript meets all of these criteria.

Why not Lua?
------------

When looking at popular choices for scripting languages to embed into programs, Lua seems like a natural choice at first, but in practice doesn't meet most of the criteria listed above.

Lua only supports 8 primitive data types: nil, boolean, number, string, function, userdata, thread, and table.

There is no native support for integer types which is a big deal for emulators since these systems do not generally deal with IEEE floating point values and it is a lot of wasted runtime, developer time, and energy to convert back and forth between floating point and integers.

Lua also has no native support for binary operations on integers. There exists a C extension module `bitop` that adds support for this but it feels very clunky to use as all these operations are done with function calls and are not available as native infix binary operators, e.g. (`xor(a,b)` vs `a xor b` or `a ^ b`).

Lua has fast execution of its own scripts and even has a JIT, however, this speed is limited to executing within its own scripting environment and is not designed for fast function calling between the host and the script. Lots of support code is needed to write C extension functions to bridge the gap between Lua and the host application.

Lua array indices start at 1 (by convention) which makes for some unnecessarily different code practices and mental gymnastics when one is used to working with 0-based array indices which most other mainstream languages use.

---

AngelScript Interface
=====================

bsnes can bind to these functions optionally defined by scripts:

* `void postRender()` - called after a frame is rendered by the PPU but before being swapped to the display
* `void main()` - called after a frame is swapped to the display

Generally, in `main()` you will want to read memory values and store them in script variables, and in `postRender()` you may want to draw on top of the rendered frame. This is done to avoid a 1-frame latency for drawing.

Memory
------

All definitions in this section are defined in the `SNES::Bus` namespace, e.g. `SNES::Bus::read_u8`.

* `uint8 read_u8(uint32 addr)` - reads a `uint8` value from the given bus address (24-bit)
* `uint16 read_u16(uint32 addr0, uint32 addr1)` - reads a `uint16` value from the given bus addresses, using `addr0` for the low byte and `addr1` for the high byte. `addr0` and `addr1` can be any address or even the same address. `addr0` is always read before `addr1` is read.
* TODO: add write functions

PPU Frame Access
----------------

All properties and types in this section are defined in the `SNES::PPU` namespace, e.g. `SNES::PPU::frame`.

* `frame` is a global property that is no-handle reference to the rendered PPU frame which offers read/write access to draw things on top of

* `Frame` object type:
  * Coordinate system used: x = 0 up to 512 from left to right, y = 0 up to 480 from top to bottom; both coordinates depend on scaling settings, interlace mode, etc.
  * `uint16` is used for representing 15-bit RGB colors, where each color channel is allocated 5 bits each, from least-significant bits for blue, to most-significant bits for red. The most significant bit of the `uint16` value for color should always be `0`. `0x7fff` is full-white and `0x0000` is full black.
  * `uint16 get(int x, int y)` - gets the 15-bit RGB color at the x,y coordinate in the PPU frame
  * `void set(int x, int y, uint16 color)` - sets the 15-bit RGB color at the x,y coordinate in the PPU frame

---

bsnes
=====

bsnes is a multi-platform Super Nintendo (Super Famicom) emulator that focuses
on performance, features, and ease of use.

bsnes currently enjoys 100% known, bug-free compatibility with the entire SNES
library when configured to its most accurate settings, giving it the same
accuracy level as higan. Accuracy can also optionally be traded for performance,
allowing bsnes to operate more than 300% faster than higan while still remaining
almost as accurate.

Development
-----------

Git is used for the development of new releases, and represents a staging
environment. As bsnes is rather mature, things should generally be quite stable.
However, bugs will exist, regressions will occur, so proceed at your own risk.

If stability is required, please download the latest stable release from the
[official website.](https://bsnes.byuu.org)

Unique Features
---------------

  - 100% (known) bug-free compatibility with the entire officially licensed SNES games library
  - True Super Game Boy emulation (using the SameBoy core by Lior Halphon)
  - HD mode 7 graphics with optional supersampling (by DerKoun)
  - Low-level emulation of all SNES coprocessors (DSP-n, ST-01n, Cx4)
  - Multi-threaded PPU graphics renderer
  - Speed mode settings which retain smooth audio output (50%, 75%, 100%, 150%, 200%)
  - Built-in games database with thousands of game entries
  - Built-in cheat code database for hundreds of popular games (by mightymo)
  - Built-in save state manager with screenshot previews and naming capabilities
  - Support for ASIO low-latency audio
  - Customizable per-byte game mappings to support any cartridges, including prototype games
  - 7-zip decompression support
  - Extensive Satellaview emulation, including BS Memory flash write and wear-leveling emulation
  - 30-bit color output support (where supported)
  - Optional higan game folder support (standard game ROM files are also fully supported!)

Standard Features
-----------------

  - MSU1 support
  - BPS and IPS soft-patching support
  - Save states with undo and redo support (for reverting accidental saves and loads)
  - OpenGL multi-pass pixel shaders
  - Several built-in software filters, including HQ2x (by MaxSt) and snes_ntsc (by blargg)
  - Adaptive sync and dynamic rate control for perfect audio/video synchronization
  - Just-in-time input polling for minimal input latency
  - Support for Direct3D exclusive mode video
  - Support for WASAPI exclusive mode audio
  - Periodic auto-saving of game saves
  - Auto-saving of states when unloading games, and auto-resuming of states when reloading games
  - Sprite limit disable support
  - Cubic audio interpolation support
  - Optional high-level emulation of most SNES coprocessors
  - CPU, SA1, and SuperFX overclocking support
  - Frame advance support
  - Screenshot support
  - Cheat code search support
  - Movie recording and playback support
  - Rewind support
  - HiDPI support

Links
-----

  - [Official website](https://bsnes.byuu.org)
  - [Official git repository](https://github.com/byuu/bsnes)
  - [Donations](https://patreon.com/byuu)

Nightly Builds
--------------

  - [Download](https://cirrus-ci.com/github/byuu/bsnes/master)
  - ![Build status](https://api.cirrus-ci.com/github/byuu/bsnes.svg?task=windows-x86_64-binaries)
  - ![Build status](https://api.cirrus-ci.com/github/byuu/bsnes.svg?task=macOS-x86_64-binaries)
  - ![Build status](https://api.cirrus-ci.com/github/byuu/bsnes.svg?task=linux-x86_64-binaries)
  - ![Build status](https://api.cirrus-ci.com/github/byuu/bsnes.svg?task=freebsd-x86_64-binaries)
