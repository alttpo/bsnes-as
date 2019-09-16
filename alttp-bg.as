// AngelScript for ALTTP to draw white rectangles around in-game sprites
uint16   xoffs, yoffs;
uint16[] sprx(16);
uint16[] spry(16);
uint8[]  sprs(16);
uint8[]  sprk(16);
uint32   location;

void init() {
  // initialize script state here.
  message("hello world!");
}

void pre_frame() {
  // fetch various room indices and flags about where exactly Link currently is:
  auto in_dark_world  = bus::read_u8 (0x7E0FFF);
  auto in_dungeon     = bus::read_u8 (0x7E001B);
  auto overworld_room = bus::read_u16(0x7E008A, 0x7E008B);
  auto dungeon_room   = bus::read_u16(0x7E00A0, 0x7E00A1);

  // compute aggregated location for Link into a single 24-bit number:
  location =
    uint32(in_dark_world & 1) << 17 |
    uint32(in_dungeon & 1) << 16 |
    uint32(in_dungeon != 0 ? dungeon_room : overworld_room);

  // get screen x,y offset by reading BG2 scroll registers:
  xoffs = bus::read_u16(0x7E00E2, 0x7E00E3);
  yoffs = bus::read_u16(0x7E00E8, 0x7E00E9);

  ppu::extra.color = 0x7fff;
  ppu::extra.text_shadow = true;
}

void post_frame() {
  // set drawing state
  // select 8x8 or 8x16 font for text:
  ppu::frame.font_height = 8;
  // draw using alpha blending:
  ppu::frame.draw_op = ppu::draw_op::op_alpha;
  // alpha is xx/31:
  ppu::frame.alpha = 20;
  // color is 0x7fff aka white (15-bit RGB)
  ppu::frame.color = ppu::rgb(31, 31, 31);

  // enable shadow under text for clearer reading:
  ppu::frame.text_shadow = true;

  // draw Link's location value in top-left:
  //ppu::frame.text(0, 0, fmtHex(location, 6));

  for (int i = 0; i < 256; i++) {
    auto rx = (i & 15) << 4;
    auto ry = (i >> 4) << 3;
    auto t = bus::read_u8(0x7EFA00 + i);
    ppu::frame.text(rx, ry, fmtHex(t, 2));
  }
}
