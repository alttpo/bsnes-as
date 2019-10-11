AngelScript interface
=====================

NOTE: Always refer to [script-interface.cpp](bsnes/sfc/interface/script-interface.cpp) for the latest definitions of
script functions.

The SNES system is frozen during script execution and clock cycles are not advanced until the invoked script function
returns.

bsnes can bind to these optional functions defined by scripts:

  * `void init()` - called once immediately after script loaded or reloaded
  * `void pre_frame()` - called immediately before scanline 0 rendering begins for the current frame
  * `void post_frame()` - called after a frame is rendered by the PPU but before it is swapped to the display

NOTE: Always refer to [script-interface.cpp](bsnes/sfc/interface/script-interface.cpp) for the latest definitions of
script functions.

Global Functions
----------------

  * `void message(const string &in msg)` - show a message on emulator status bar, like a notification
  * `string &fmtHex(uint64 value, int precision = 0)` - formats a `uint64` in hexadecimal using `precision` number of
  digits
  * `string &fmtBinary(uint64 value, int precision = 0)` - formats a `uint64` in binary using `precision` number of
  digits
  * `string &fmtInt(int64 value)` - formats a `int64` as a string
  * `string &fmtUint(uint64 value)` - formats a `uint64` as a string

NOTE: Always refer to [script-interface.cpp](bsnes/sfc/interface/script-interface.cpp) for the latest definitions of
script functions.

Memory
------

All definitions in this section are defined in the `bus` namespace, e.g. `bus::read_u8`. 

Memory read functions:

  * `uint8 bus::read_u8(uint32 addr)` - reads a `uint8` value from the given bus address (24-bit)
  * `uint16 bus::read_u16(uint32 addr0, uint32 addr1)` - reads a `uint16` value from the given bus addresses (24-bit),
    using `addr0` for the low byte and `addr1` for the high byte. `addr0` and `addr1` can be any address or even the
    same address. `addr0` is always read before `addr1` is read.
  * `void read_block_u8(uint32 addr, uint offs, uint16 size, const array<uint8> &in output)` - reads a block of
    `uint8` data from address `addr` to a slice of an `array<uint8>` specified by `offs` and `size` (bytes).
  * `void read_block_u16(uint32 addr, uint offs, uint16 size, const array<uint16> &in output)` - reads a block of
    `uint16` data from address `addr` to a slice of an `array<uint16>` specified by `offs` and `size` (words).

Memory write functions:

  * `void bus::write_u8(uint32 addr, uint8 data)` - writes a `uint8` value to the given bus address (24-bit)
  * `void bus::write_u16(uint32 addr0, uint32 addr1, uint16 data)` - writes a `uint16` value to the given bus addresses
    (24-bit), using `addr0` for the low byte and `addr1` for the high byte. `addr0` and `addr1` can be any address or
    even the same address. `addr0` is always written before `addr1` is written.
  * `void bus::write_block_u8(uint32 addr, uint offs, uint16 size, const array<uint8> &in output)` - writes a block of
    `uint8` data to address `addr` from a slice of an `array<uint8>` specified by `offs` and `size` (bytes).
  * `void write_block_u16(uint32 addr, uint offs, uint16 size, const array<uint16> &in output)` - writes a block of
    `uint16` data to address `addr` from a slice of an `array<uint16>` specified by `offs` and `size` (words).

Reads and writes are done on the main bus and clock cycles are not advanced *unless* the addresses being read from
or written to cross any special memory-mapped hardware registers like for the PPU. Reads and writes to WRAM-mapped
addresses are always guaranteed to not advance clock cycles.

Memory write interception:

  * `void WriteInterceptCallback(uint32 addr, uint8 value)` - callback function definition for intercepting memory
    writes.
  * `void add_write_interceptor(const string &in addr, uint32 size, WriteInterceptCallback @cb)` - registers a script
    function to be called when any address within a memory address range is written to. An example of the `addr` string
    format is `00-10,20-40,7e-7f:2000-2fff,4000-4fff`. The string is split by `':'` to separate bank ranges from address
    ranges. Multiple bank ranges and address ranges are split by `','`. Bank ranges are specified as hex values split
    by `'-'` to define lower range inclusive and upper range inclusive; bank ranges only need to be at most 2 hex
    characters because they are 8-bit values. Address ranges are specified as hex values split by `'-'` to define lower
    range inclusive and upper range inclusive; address ranges only need to be at most 4 hex characters because they are
    16-bit values. The combination of a bank and an address create a full 24-bit address. When `size` is `0`, the
    mapping is non-contiguous, otherwise the mapping is treated as a contiguous range of size `size` bytes from lowest
    address to highest address across all address ranges.

NOTE: Always refer to [script-interface.cpp](bsnes/sfc/interface/script-interface.cpp) for the latest definitions of
script functions.

DMA interception
----------------

All definitions in this section are defined in the `cpu` namespace. 

`DMAIntercept` class:
  * `uint8  channel`
  * `uint8  transferMode`
  * `uint8  fixedTransfer`
  * `uint8  reverseTransfer`
  * `uint8  unused`
  * `uint8  indirect`
  * `uint8  direction`
  * `uint8  targetAddress`
  * `uint16 sourceAddress`
  * `uint8  sourceBank`
  * `uint16 transferSize`
  * `uint16 indirectAddress`
  * `uint8  indirectBank`

Functions:

  * `void DMAInterceptCallback(DMAIntercept @dma)` - callback function definition for DMA interception
  * `void register_dma_interceptor(DMAInterceptCallback @cb)` - registers the DMA interceptor callback function

NOTE: Always refer to [script-interface.cpp](bsnes/sfc/interface/script-interface.cpp) for the latest definitions of
script functions.

Graphics Integration
====================

Post-Frame Rendering
--------------------

The best way to affect the rendering of a frame is to wait for the PPU to finish rendering a frame and then draw
directly on top of the final frame. This mode is useful for making large-scale annotations on the frame for debugging
purposes or for creating in-game mechanics that don't need tight integration with the PPU. This mode draws directly
onto the rendered PPU frame and is destructive by nature. Supported features are drawing pixels directly, drawing
horizontal and vertical lines, drawing rectangles, and drawing text using an 8x8 or 8x16 pixel font. Drawing operations
can make use of alpha-blending and colors are represented with 15-bit BGR values.

This post-frame mode may only be meaningfully used within the context of the emulator calling the `void post_frame()`
function defined in the script. Do note that if reading SNES memory during `post_frame()`, most values will be relevant
for the _next_ frame and not the current already-rendered frame. This is because most SNES games execute their game
logic while the PPU is rendering a frame and so most RAM is updated while rendering. To avoid this 1 frame delay, it is
advised to read/write memory during `void pre_frame()` and store copies of values read into script variables to be later
drawn or acted upon in `void post_frame()`. Although it is not illegal to call post-frame drawing operations in
`pre_frame()` function, their effect would be meaningless because the frame would be cleared out by the PPU and will be
redrawn.

Rendering Interfaces for Scripts
================================

Note that the SNES works internally with 15-bit BGR color values.

  * For scripts, the `uint16` type is used for representing 15-bit BGR color values.
  * Each color channel is allocated 5 bits, from least-significant bits for red, to most-significant bits for blue.
  * The most significant bit of the `uint16` value for color should always be `0`.
  * Reference colors:
    * `0x7fff` is 100% white.
    * `0x0000` is 100% black.
    * `0x7c00` is 100% blue.
    * `0x03e0` is 100% green.
    * `0x001f` is 100% red.
  * Since it is cumbersome to mentally compose hex values for 15-bit RGB colors, the `ppu::rgb` function is provided to
  construct the final `uint16` color value given each color channel's 5-bit value.

General PPU Data Access Interface
---------------------------------

`ppu` namespace methods and properties:
  * `uint16 ppu::rgb(uint8 r, uint8 g, uint8 b)` - function used to construct 15-bit RGB color values; each color
  channel is a 5-bit value between 0..31.
  * `uint8 ppu::luma { get; }` - global read-only property representing the current PPU luminance value (0..15) where 0
  is very dark and 15 is full brightness.
  * `uint8 ppu::sprite_width(uint8 baseSize, uint8 size)` - calculates a sprite's width given the base size and sprite
  size
  * `uint8 ppu::sprite_height(uint8 baseSize, uint8 size)` - calculates a sprite's height given the base size and sprite
  size
  * `uint8 ppu::sprite_base_size()` - fetches the current sprite base size from PPU state
  * `uint16 ppu::tile_address(bool nameselect)` - calculates the tile address in VRAM depending on the `nameselect` bit
  * `ppu::VRAM  ppu::vram` - global property to access VRAM with
  * `ppu::CGRAM ppu::cgram` - global property to access CGRAM with
  * `ppu::OAM   ppu::oam` - global property to access OAM with

`ppu::VRAM` object for direct access to VRAM:
  * `uint16 opIndex(uint16 addr)` - reads a 16-bit value from VRAM at absolute address `addr`
  * `uint16 chr_address(uint16 chr)` - computes the VRAM address for a given sprite CHR value (9-bit), valid return
  values are from `0x4000` to `0x5fff`.
  * `void read_block(uint16 addr, uint offs, uint16 size, array<uint16> &inout output)` - reads 16-bit VRAM data from
  address `addr` into `output` array at offset `offs` for size `size` words. Does not advance clock cycles; reads
  directly from emulator's representation of VRAM and not via the main bus.
  * `"void write_block(uint16 addr, uint offs, uint16 size, const array<uint16> &in data)"` - writes 16-bit VRAM data to
  address `addr` from `output` array at offset `offs` for size `size` words. Does not advance clock cycles; writes
  directly into emulator's representation of VRAM and not via the main bus.

`ppu::CGRAM` object for direct access to CGRAM:
  * `uint16 opIndex(uint16 addr)` - reads a 16-bit color value from CGRAM at absolute address `addr`

`ppu::OAMSprite` object represents a copy of an OAM sprite:
  * `uint8  index { get; set; }` - gets/sets the current OAM sprite index to read from
  * `bool   is_enabled { get; }` - determines if the OAM sprite is on screen
  * `uint16 x { get; }` - gets the X coordinate (9-bit) of the current OAM sprite
  * `uint8  y { get; }` - gets the Y coordinate (8-bit) of the current OAM sprite
  * `uint16 character { get; }` - gets the CHR value (9-bit) of the current OAM sprite
  * `bool   vflip { get; }` - gets the vertical flip flag of the current OAM sprite
  * `bool   hflip { get; }` - gets the horizontal flip flag of the current OAM sprite
  * `uint8  priority { get; }` - gets the priority (2-bit) of the current OAM sprite
  * `uint8  palette { get; }` - gets the palette (3-bit) of the current OAM sprite
  * `uint8  size { get; }` - gets the size (1-bit) of the current OAM sprite
  * `uint8  width { get; }` - gets the width in pixels of the current OAM sprite (convenience property calculated from
  `size`)
  * `uint8  height { get; }` - gets the height in pixels of the current OAM sprite (convenience property calculated from
  `size`)

`ppu::OAM` object represents direct access to OAM memory:
  * `OAMSprite @get_opIndex(uint8 chr)` - gets a copy of an OAM sprite given its index (7-bit)
  * `void set_opIndex(uint8 chr, OAMSprite @sprite)` - writes an OAM sprite to the OAM table given its index (7-bit)

NOTE: Always refer to [script-interface.cpp](bsnes/sfc/interface/script-interface.cpp) for the latest definitions of
script functions.

Extra OAM Sprite Definition Interface
-------------------------------------

All features for defining "extra" OAM sprites are found in `ppu::extra` script object.

Global properties:
  * `ppu::Extra ppu::extra`

`ppu::Extra` methods and properties:
  * `uint16 get_color()` - gets the current drawing color
  * `void   set_color(uint16 color)` - sets the current drawing color for drawing operations that don't otherwise have
  parameters to specify which color or colors to draw
  * `bool   get_text_shadow()` - gets the current flag for rendering shadows under text
  * `void   set_text_shadow(bool color)` - enables or disables a black shadow +1 pixel right and +1 pixel below and
  underneath any future calls to `text()`
  * `int    measure_text(const string &in text)` - measures the width of a text string in pixels
  * `void   reset()` - resets all `ExtraTile` objects to their initial states
  * `uint   get_count()` - gets the number of valid `ExtraTile` objects for the PPU to render
  * `void   set_count(uint count)` - sets the number of valid `ExtraTile` objects for the PPU to render, starting from
  index `0` in the `get_opIndex(uint i)` array.
  * `ExtraTile @get_opIndex(uint i)` - gets a reference to the `ExtraTile` object at index `i`; valid values for `i`
  are `[0..127]`, i.e. there is a maximum of 128 extra sprites that can be drawn.

`ppu::ExtraTile` methods and properties:
  * `int  x` - X coordinate on screen for top-left of sprite
  * `int  y` - Y coordinate on screen for top-left of sprite
  * `uint source` - What source layer the sprite should be drawn into. Use 4 for OBJ1 and 5 for OBJ2 (only values
  currently supported; 0..3 represent BG layers but extra sprites cannot be drawn there yet)
  * `bool hflip` - Set to true to horizontally flip the sprite when rendering
  * `bool vflip` - Set to true to vertically flip the sprite when rendering
  * `uint priority` - when `source` is OBJ1 or OBJ2, use priority values `0..3` which have identical meaning to
  hardware OAM priority values, e.g. `0` is lowest priority and `3` is highest priority. When a priority conflict
  exists against hardware OAM sprites, the hardware OAM sprites will win (i.e. be drawn over extra sprites).
  * `uint width` - Width in pixels of the sprite
  * `uint height` - Height in pixels of the sprite
  * `uint index` - Which OAM index to emulate the sprite being at `0..127`; hardware OAM sprite index order affects
  which sprites are drawn on top of other sprites. OAM indexes with lower numbers override those with higher indexes.
  * `void reset()` - resets all fields to defaults and clears pixel data.
  * `void pixels_clear()` - clears pixel data to be all transparent pixels.
  * `void pixel_set(int x, int y, uint16 color)` - sets the pixel at x,y to the specific 15-bit BGR color
  * `void pixel_off(int x, int y)` - turns off the opacity of the pixel at x,y (retains existing color data)
  * `void pixel_on(int x, int y)` - turns on the opacity of the pixel at x,y (retains existing color data)
  * `void pixel(int x, int y)` - sets the pixel at x,y to the current global `ppu::Extra.color` value
  * `void draw_sprite(int x, int y, int width, int height, const array<uint32> &in tiledata, const array<uint16> &in
  palette)` - Renders a VRAM 4bpp sprite at the x,y coordinate within this extra sprite. `tiledata` is organized like
  VRAM for 4bpp sprite tiles; compatible with data retrieved from `ppu::vram.read_sprite()` function. `palette` is a 16
  value array of 15-bit BGR color values which may be directly copied from `ppu::CGRAM[]`.
  * `void hline(int lx, int ty, int w)` - draws a horizontal line using current drawing color
  * `void vline(int lx, int ty, int h)` - draws a vertical line using current drawing color
  * `void rect(int x, int y, int w, int h)` - draws a rectangle using current drawing color
  * `void fill(int x, int y, int w, int h)` - fills a rectangle using current drawing color
  * `int  text(int x, int y, const string &in text)` - renders text using an 8x8 pixel font using the current drawing
  color

NOTE: Always refer to [script-interface.cpp](bsnes/sfc/interface/script-interface.cpp) for the latest definitions of
script functions.

Post-Frame Rendering Interface
------------------------------

Notes:
  * Coordinate system used
     * x = 0..255 from left to right
     * y = 0..239 from top to bottom
     * Top and bottom 8 rows are for overscan
     * The visible portion of the screen is 256x224 resolution
     * In absolute coordinates, the visible top-left coordinate is (0,8) and the visible bottom-right coordinate is
     (255,223)
     * `ppu::frame.y_offset` property exists to offset all drawing Y coordinates (in 480 column scale) to account for
     overscan. It defaults to `+16` so that (0,0) in drawing coordinates is the visible top-left coordinate for all
     drawing functions. Scripts are free to change this property to `0` (to draw in the overscan area) or to any other
     value.
     * `ppu::frame.x_scale` is a multiplier to X coordinates to scale to a unified 512x480 pixel buffer; default = 2
     which implies a lo-res 256 column mode; switch to 1 to use hi-res 512 column mode
     * `ppu::frame.y_scale` is a multiplier to Y coordinates to scale to a unified 512x480 pixel buffer; default = 2
     which implies a lo-res 240 row mode; switch to 1 to use hi-res 480 row mode
  * alpha channel is represented in a `uint8` type as a 5-bit value between 0..31 where 0 is completely transparent and
  31 is completely opaque.
  * alpha blending equation is `[src_rgb*src_a + dst_rgb*(31 - src_a)] / 31`
  * `ppu::draw_op` - enum of drawing operations used to draw pixels; available drawing operations are:
     * `ppu::draw_op::op_solid` - draw solid pixels, ignoring alpha (default)
     * `ppu::draw_op::op_alpha` - draw new pixels alpha-blended with existing pixels
     * `ppu::draw_op::op_xor` - XORs new pixel color value with existing pixel color value
   * **IMPORTANT**: All drawing operations are performed *immediately* and *directly* on the PPU frame buffer.
     * Drawing operations are **destructive** to the PPU rendered frame.

Globals:
  * `uint16 ppu::rgb(uint8 r, uint8 g, uint8 b)` - function used to construct 15-bit RGB color values; each color
  channel is a 5-bit value between 0..31.
  * `uint8 ppu::luma { get; }` - global read-only property representing the current PPU luminance value (0..15) where 0
  is very dark and 15 is full brightness.
  * `ppu::Frame ppu::frame { get; }` - global read-only property representing the current rendered PPU frame that is
  ready to swap to the display. Contains methods and properties that allow for drawing things on top of the frame.

`ppu::Frame` properties:
  * `int y_offset { get; set; }` - property to adjust Y-offset of drawing functions (default = +16; skips top overscan
  area so that x=0,y=0 is top-left of visible screen)
  * `ppu::draw_op draw_op { get; set; }` - current drawing operation used to draw pixels
  * `uint16 color { get; set; }` - current color for drawing with (0..0x7fff)
  * `uint8 luma { get; set; }` - current luminance for drawing with (0..15); `color` is luma-mapped (applied before
  alpha blending) for drawing
  * `uint8 alpha { get; set; }` - current alpha value for drawing with (0..31)
  * `int font_height { get; set; }` - current font height in pixels; valid values are 8 or 16 (default = 8).
  * `bool text_shadow { get; set; }` - determines whether to draw a black shadow one pixel below and to the right behind
  any text; text does not overdraw the shadow in order to avoid alpha-blending artifacts.

`ppu::Frame` methods:
  * `uint16 read_pixel(int x, int y)` - gets the 15-bit RGB color at the x,y coordinate in the PPU frame
  * `void pixel(int x, int y)` - sets the 15-bit RGB color at the x,y coordinate in the PPU frame
  * `void hline(int lx, int ty, int w)` - draws a horizontal line between `ty <= y < ty+h`
  * `void vline(int lx, int ty, int h)` - draws a vertical line between `lx <= x < lx+w`
  * `void rect(int lx, int ty, int w, int h)` - draws a rectangle at the boundaries `lx <= x < lx+w` and
  `ty <= y < ty+h`; does not overdraw corners
  * `void fill(int lx, int ty, int w, int h)` - fills a rectangle within `lx <= x < lx+w` and `ty <= y < ty+h`; does not
  overdraw
  * `int text(int lx, int ty, const string &in text)` - draws a horizontal span of ASCII text using the current
  `font_height`
    * returns `len` as number of characters drawn
    * all non-printable and control characters (CR, LF, TAB, etc) are skipped for rendering and do not contribute to the
    width of the text's bounding box
    * top-left corner of text bounding box is specified by `(lx, ty)` coordinates
    * bottom-right corner of text bounding box is computed as `(lx+(8*len), ty+font_height)` where `len` is the number
    of characters to be drawn (excludes non-printable chars)
    * bounding box coordinates are not explicitly computed by the function up-front but serve to define the exact
    rendering behavior of the function
    * character glyphs are rendered relative to their top-left corner
  * `void draw_4bpp_8x8(int lx, int ty, uint32[] tile_data, uint16[] palette_data)` - draws an 8x8 tile in 4bpp color
  mode using the given `tile_data` from VRAM and `palette_data` from CGRAM. pixels are drawn using the current drawing
  operations; `color` is ignored; `luma` is used to luma-map the palette colors for display.
  **WARNING:** this method is likely to be deprecated as it is not directly compatible with VRAM read_block/write_block
  functions which return data as `uint16[]` and not `uint32[]`.

NOTE: Always refer to [script-interface.cpp](bsnes/sfc/interface/script-interface.cpp) for the latest definitions of
script functions.

Network Access for Scripts
==========================

`net::UDPSocket@` type represents a UDP socket which can listen for and send UDP packets.
  * `net::UDPSocket@ constructor(const string &in host, const int port)` - construct a UDP socket listening at `host`
  address on `port` port number.
  * `int sendto(const array<uint8> &in msg, const string &in host, const int port)` - sends a UDP packet to the given
  destination address and port; non-blocking.
  * `int recv(const array<uint8> &in msg)` - attempts to receive a UDP packet into the `msg` array and returns `0` if no
  packet available, or returns the number of bytes received; non-blocking. (internally uses a `poll()` followed by
  `recvfrom()` if the poll indicates that data is available to be read)

**WARNING:** this API is likely to be refactored to extract out IPAddress structures instead of specifying host as
string and port as integer. This change will also make these functions more performant due to not having to call
`getaddrinfo()` every time.

NOTE: Always refer to [script-interface.cpp](bsnes/sfc/interface/script-interface.cpp)
for the latest definitions of script functions.

GUI Integrations for Scripts
===========================

TODO: document `gui` namespace members.

NOTE: Always refer to [script-interface.cpp](bsnes/sfc/interface/script-interface.cpp)
for the latest definitions of script functions.
