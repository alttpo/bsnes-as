// AngelScript to test ExtraTile features:
void pre_frame() {
  ppu::extra.count = 1;
  ppu::extra[0] = ppu::ExtraTile(128, 12, 5, true, 3, 16, 8);
}
