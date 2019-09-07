// AngelScript to test ExtraTile features:
int x = 0;
void pre_frame() {
  ppu::extra.count = 1;
  ppu::extra[0] = ppu::ExtraTile(108, 116, 2, true, true, 5, 32, 2);
  ppu::extra[0].pixels_clear();
  ppu::extra[0].pixel_set(x, 0, 0x001f);
  ppu::extra[0].pixel_set(x, 1, 0x7c00);
  x++;
  if (x >= 32) x = 0;
}
