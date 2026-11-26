# bsnes-headless

The bsnes core and its AngelScript host with no window, audio or input devices, for testing scripts. It runs a real
ROM at several hundred frames per second, and is driven one command per line on stdin.

Build (objects go to `obj-headless/`, apart from the GUI build's `obj/`):

    make -C bsnes target=headless -j10      # -> bsnes/out/bsnes-headless

The script GUI API (`GUI::Window` and friends) is registered against stubs, so scripts that build windows compile and
run. The windows are never shown, but widgets keep the values scripts set on them, so scripts that read their settings
back from widgets work. Discord is stubbed out. Nothing is saved to the cartridge: SRAM starts empty unless `--sram=`
gives a file to read it from. Scripts' `UserSettings` files go to `--settings-dir=<dir>`, by default a new temporary
folder, never the GUI's settings folder unless you pass it.

## Compile check

    bsnes-headless --compile-only --script=../alttpo/alttpo/

Compiles the script (a `.as` file, or a directory whose `*.as` files make one module) without a ROM and without running
`init()`, prints diagnostics as `path:line:col: error: message` and a count, and exits 1 if there are errors.

## Running

    bsnes-headless [--script=<path>] [--sram=<file>] [--settings-dir=<dir>] [--entropy=none|low|high]
                   [--accurate-ppu] [--allow-opposite-directions] [--frames=<n>] rom.sfc

`--allow-opposite-directions` lets up+down and left+right reach the game together (the core option
`Controller/AllowOppositeDirections`); real D-pads can't, but snes9x movies recorded with its "allow left+right"
option rely on it.

The ROM loads and powers on, then the script loads (`init()`, `cartridge_loaded()`, `post_power(false)`). With
`--frames=<n>` it runs n frames and exits (1 if the script threw); otherwise it prints `ok ready <title>` and reads
commands. RAM initialization defaults to `--entropy=none` so runs are repeatable: the same commands give the same
state.

Every command is answered by exactly one line, `ok [result]` or `err <reason>`. Script output produced while the
command ran comes before it, one line each: `log <frame> <level> <text>` where level is `debug`, `info`, `warn` or
`error`. Compiler diagnostics and script exceptions (with a stack trace) are `error` logs. Numbers are decimal, or hex
with a `$` or `0x` prefix. Blank lines and lines starting with `#` are ignored, so a command file can be piped in.

| command | result |
| --- | --- |
| `frame [n]` | runs n frames (default 1): `ok <frames since power on>` |
| `input <port> [buttons...]` | holds buttons on controller 1 or 2 until the next `input`: `up down left right b a y x l r select start`, or a mask like `$800` (bit 0 = up ... bit 11 = start). No buttons releases all |
| `read <addr> [size]` | reads the bus: `ok <hex bytes>` |
| `write <addr> <hex bytes>` | writes the bus |
| `reset` / `power` | soft reset / power cycle (power resets the frame count) |
| `save-state <file>` / `load-state <file>` | core state only; the script's variables are not saved |
| `screenshot <file.bmp>` | the last frame, overscan cropped |
| `call <decl>` | calls a script function without parameters by declaration or name: `ok <return value>` for numbers, bools and strings |
| `exec <statements>` | compiles and runs statements in the script module's scope, e.g. `exec message(fmtHex(bus::read_u8(0x7e0010), 2))` |
| `script-load <path>` / `script-reload` / `script-unload` | |
| `status` | `ok frame=<n> exceptions=<n> rom=<file> script=<path>` |
| `quit` | runs the script's `unload()` and exits; so does end of input |

`bsnes_headless.py` wraps the protocol for Python tests.
