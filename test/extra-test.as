// AngelScript to test ExtraTile features:
int x = 0;

// Called on scanline 0 when PPU is about to start rendering a frame:
void pre_frame() {
  // draw with white color:
  ppu::extra.color = 0x7fff;
  // enable shadow beneath text for readability:
  ppu::extra.text_shadow = true;

  // we have 1 extra tile to render in the PPU:
  ppu::extra.count = 1;

  // put the extra tile at 108,116, layer 2 (BG3), on both main and sub screens, priority 5, 42x8 size
  ppu::extra[0].x = 108;
  ppu::extra[0].y = 116;
  ppu::extra[0].source = 2;
  ppu::extra[0].aboveEnable = true;
  ppu::extra[0].belowEnable = true;
  ppu::extra[0].priority = 5;
  ppu::extra[0].width = 42;
  ppu::extra[0].height = 8;
  // clear previous pixels in the tile:
  ppu::extra[0].pixels_clear();
  // test the pixel_set function by moving a pair of pixels from left to right and wrapping:
  ppu::extra[0].pixel_set(x, 0, 0x001f);
  ppu::extra[0].pixel_set(x, 1, 0x7c00);
  // draw "Hello" on the tile:
  ppu::extra[0].text(0, 0, "Hello");

  // wrap the X coordinate for the test pixels:
  x++;
  if (x >= 42) x = 0;
}
