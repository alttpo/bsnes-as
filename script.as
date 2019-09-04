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

void main() {
  // fetch various room indices and flags about where exactly Link currently is:
  auto in_dark_world  = SNES::Bus::read_u8 (0x7E0FFF);
  auto in_dungeon     = SNES::Bus::read_u8 (0x7E001B);
  auto overworld_room = SNES::Bus::read_u16(0x7E008A, 0x7E008B);
  auto dungeon_room   = SNES::Bus::read_u16(0x7E00A0, 0x7E00A1);

  // compute aggregated location for Link into a single 24-bit number:
  location =
    uint32(in_dark_world & 1) << 17 |
    uint32(in_dungeon & 1) << 16 |
    uint32(in_dungeon != 0 ? dungeon_room : overworld_room);

  // get screen x,y offset by reading BG2 scroll registers:
  xoffs = SNES::Bus::read_u16(0x7E00E2, 0x7E00E3);
  yoffs = SNES::Bus::read_u16(0x7E00E8, 0x7E00E9);

  for (int i = 0; i < 16; i++) {
    // sprite x,y coords are absolute from BG2 top-left:
    spry[i] = SNES::Bus::read_u16(0x7E0D00 + i, 0x7E0D20 + i);
    sprx[i] = SNES::Bus::read_u16(0x7E0D10 + i, 0x7E0D30 + i);
    // sprite state (0 = dead, else alive):
    sprs[i] = SNES::Bus::read_u8(0x7E0DD0 + i);
    // sprite kind:
    sprk[i] = SNES::Bus::read_u8(0x7E0E20 + i);
  }
}

void postRender() {
  // set drawing state:
  SNES::PPU::frame.font_height = 8; // select 8x8 or 8x16 font for text
  // draw using alpha blending
  SNES::PPU::frame.draw_op = SNES::PPU::DrawOp::op_alpha;
  // alpha is 20/31:
  SNES::PPU::frame.alpha = 24;
  // color is 0x7fff aka white (15-bit RGB)
  SNES::PPU::frame.color = 0x7fff;

  // enable shadow under text for clearer reading:
  SNES::PPU::frame.text_shadow = true;

  // draw Link's location value in top-left:
  SNES::PPU::frame.text(0, 0, fmtHex(location, 6));

  for (int i = 0; i < 16; i++) {
    // skip dead sprites:
    if (sprs[i] == 0) continue;

    // subtract BG2 offset from sprite x,y coords to get local screen coords:
    int16 rx = int16(sprx[i]) - int16(xoffs);
    int16 ry = int16(spry[i]) - int16(yoffs);

    // draw box around the sprite:
    SNES::PPU::frame.rect(rx, ry, 16, 16);

    // draw sprite type value above box:
    ry -= SNES::PPU::frame.font_height;
    SNES::PPU::frame.text(rx, ry, fmtHex(sprk[i], 2));
  }
}
