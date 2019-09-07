// AngelScript to test ExtraTile features:
int x = 0;
void pre_frame() {
  ppu::extra.color = 0x7fff;
  ppu::extra.text_shadow = true;

  ppu::extra.count = 1;

  ppu::extra[0] = ppu::ExtraTile(108, 116, 2, true, true, 5, 42, 8);
  ppu::extra[0].pixels_clear();
  ppu::extra[0].pixel_set(x, 0, 0x001f);
  ppu::extra[0].pixel_set(x, 1, 0x7c00);
  ppu::extra[0].text(0, 0, "Hello");

  x++;
  if (x >= 42) x = 0;
}
