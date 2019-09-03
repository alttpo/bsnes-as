// AngelScript for ALTTP to draw white rectangles around in-game sprites
uint16   xoffs, yoffs;
uint16[] sprx(16);
uint16[] spry(16);
uint8[]  sprs(16);
uint8[]  sprt(16);

void init() {
  // initialize script state here.
}

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
  for (int x = lx; x < lx + w; ++x) {
    SNES::PPU::frame.set(x, ty, color);
  }
}

// draw a vertical line from y=ty to ty+h on x=lx:
void vline(int lx, int ty, int h, uint16 color) {
  for (int y = ty; y < ty + h; ++y) {
    SNES::PPU::frame.set(lx, y, color);
  }
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
