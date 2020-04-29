
void pre_frame() {
  // Test assumes ALTTP ROM.
  auto module = bus::read_u8(0x7E0010);
  // Wait until we're into the main game:
  if (module >= 7 and module <= 0x13) {
    // This is the ROM address of a single RTL instruction. We're testing a JSL directly to an RTL so there should be no
    // adverse effect on the system.
    cpu::call(0x00D8D4);
  }
}
