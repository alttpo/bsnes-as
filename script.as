// AngelScript for ALTTP to draw white rectangles around in-game sprites
uint16   xoffs, yoffs;
uint16[] sprx(16);
uint16[] spry(16);
uint8[]  sprs(16);
uint8[]  sprt(16);

void init() {
  // initialize script state here.
  message("hello world!");
}

void main() {
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
    sprt[i] = SNES::Bus::read_u8(0x7E0E20 + i);
  }
}

void postRender() {
  // set drawing state:
  SNES::PPU::frame.font_height = 8; // select 8x8 or 8x16 font for text
  SNES::PPU::frame.draw_op = SNES::PPU::DrawOp::op_alpha;
  SNES::PPU::frame.alpha = 24;
  SNES::PPU::frame.color = 0x7fff;

  for (int i = 0; i < 16; i++) {
    // skip dead sprites:
    if (sprs[i] == 0) continue;

    // subtract BG2 offset from sprite x,y coords to get local screen coords:
    int16 rx = int16(sprx[i]) - int16(xoffs);
    int16 ry = int16(spry[i]) - int16(yoffs);

    // draw rectangle around the sprite, alpha-blended:
    SNES::PPU::frame.rect(rx, ry, 16, 16);

    // draw sprite type value, xored:
    auto str = hex(sprt[i],2);
    ry -= 16;
    SNES::PPU::frame.draw_op = SNES::PPU::DrawOp::op_alpha;
    SNES::PPU::frame.color = 0x0000;
    SNES::PPU::frame.alpha = 28;
    SNES::PPU::frame.text(rx+1, ry+1, str);
    SNES::PPU::frame.draw_op = SNES::PPU::DrawOp::op_alpha;
    SNES::PPU::frame.color = 0x7fff;
    SNES::PPU::frame.alpha = 24;
    SNES::PPU::frame.text(rx, ry, str);
  }
}
