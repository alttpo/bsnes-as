AngelScript
===========
I have embedded the [AngelScript v2.33.0](https://www.angelcode.com/angelscript/) engine into the bsnes code and created some rudimentary script function bindings between the bsnes emulator and AngelScript scripts.

As a working proof of concept of the scripting engine and the basic bindings set up, I've developed a script that draws white squares surrounding in-game sprites as they are defined in RAM by Zelda 3: A Link To The Past. To test this proof of concept:

1. clone this repository
2. `cd` to repository working copy directory
3. `git checkout 00cf4de1111d8cda3c4c0f7942390b25492ca958`
4. `make -C bsnes`
5. `./test.sh`

You will have to modify `test.sh` to override the location of your ALTTP ROM file as it's highly unlikely to be identical to mine.

Script Interface
----------------

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
