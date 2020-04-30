array<uint8> code(0x100);

void post_power(bool reset) {
  // use bank 0xBF to store a jump table:
  // map 8000-8fff in bank bf to our code array:
  bus::map("bf:8000-80ff", 0, 0x00ff, 0, code);

  // NOP out the code:
  for (uint i = 0; i < 0x100; i++) {
    code[i] = 0xEA; // NOP
  }

  if (true) {
    // JSL Module_MainRouting
    code[0] = 0x22; // JSL
    code[1] = 0xB5; //     L
    code[2] = 0x80; //     H
    code[3] = 0x00; //     B
    code[4] = 0x6B; // RTL
  }

  // patch ROM to JSL to our code:
  // JSL 0xBF8000
  bus::write_u8(0x008056, 0x22);  // JSL
  bus::write_u8(0x008057, 0x00);  //     L
  bus::write_u8(0x008058, 0x80);  //     H
  bus::write_u8(0x008059, 0xBF);  //     B
}
