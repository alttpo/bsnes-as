// AngelScript to test ExtraTile features:
void pre_frame() {
  ppu::extra.count = 1;
  ppu::extra[0] = ppu::ExtraTile(108, 108, 5, true, 6, 2, 2);
  ppu::extra[0].pixel_set(0, 0, 0x001f);
  ppu::extra[0].pixel_set(1, 0, 0x001f);
  ppu::extra[0].pixel_set(0, 1, 0x001f);
  ppu::extra[0].pixel_set(1, 1, 0x001f);
}
