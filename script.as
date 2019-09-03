uint16   xoffs, yoffs;
uint16[] sprx(16);
uint16[] spry(16);
uint8[]  sprt(16);
uint8[]  sprs(16);

void main() {
  xoffs = SNES::Bus::read_u16(0x00E2, 0x00E3);
  yoffs = SNES::Bus::read_u16(0x00E8, 0x00E9);

  for (int i = 0; i < 16; i++) {
	spry[i] = SNES::Bus::read_u16(0x0D00 + i, 0x0D20 + i);
	sprx[i] = SNES::Bus::read_u16(0x0D10 + i, 0x0D30 + i);
	sprt[i] = SNES::Bus::read_u8(0x0E20 + i);
	sprs[i] = SNES::Bus::read_u8(0x0DD0 + i);
  }
}

void hline(int lx, int ty, int w, uint16 color) {
  for (int x = lx; x < lx + w; ++x)
    SNES::PPU::frame.set(x, ty, color);
}

void vline(int lx, int ty, int h, uint16 color) {
  for (int y = ty; y < ty + h; ++y)
    SNES::PPU::frame.set(lx, y, color);
}

void postRender() {
  for (int i = 0; i < 16; i++) {
    if (sprt[i] != 0 && sprs[i] != 0) {
      int16 rx = int16(sprx[i]) - int16(xoffs);
      int16 ry = int16(spry[i]) - int16(yoffs);
      hline(rx     , ry     , 16, 0x7fff);
      vline(rx     , ry     , 16, 0x7fff);
      hline(rx     , ry + 15, 16, 0x7fff);
      vline(rx + 15, ry     , 16, 0x7fff);
      //printf("[%x] x=%04x,y=%04x t=%02x\n", i, x, y, t);
    }
  }
}
