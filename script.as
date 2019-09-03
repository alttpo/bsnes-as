uint16   xoffs, yoffs;
uint16[] sprx(16);
uint16[] spry(16);
uint8[]  sprt(16);
uint8[]  sprs(16);

void main() {
  xoffs = SNES::Bus::read_u16(0x00E2);
  yoffs = SNES::Bus::read_u16(0x00E8);

  for (int i = 0; i < 16; i++) {
	spry[i] = SNES::Bus::read_u16(0x0D00 + i);
	sprx[i] = SNES::Bus::read_u16(0x0D10 + i);
	sprt[i] = SNES::Bus::read_u8( 0x0E20 + i);
	sprs[i] = SNES::Bus::read_u8( 0x0DD0 + i);
  }
}

void postRender() {

}
