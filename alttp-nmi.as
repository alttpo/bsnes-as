// Communicate with ROM hack via memory at $7F7667[0x6719]
class Packet {
  uint32 addr;

  uint32 location;

  Packet(uint32 addr) {
    this.addr = addr;
  }

  void readRAN() {
    auto a = addr;
    location = uint32(bus::read_u16(a+0, a+1)) | (uint32(bus::read_u8(a+2)) << 16);
  }
};

Packet local(0x7f7668);
Packet remote(0x7f7668); // TODO: update me to where remote packet is stored in RAM

void pre_frame() {
  local.readRAN();
}

void post_frame() {
  ppu::frame.text_shadow = true;

  // read local packet composed during NMI:
  ppu::frame.text(0, 0, fmtHex(local.location, 6));

  {
    auto in_dark_world = bus::read_u8(0x7E0FFF);
    auto in_dungeon = bus::read_u8(0x7E001B);
    auto overworld_room = bus::read_u16(0x7E008A, 0x7E008B);
    auto dungeon_room = bus::read_u16(0x7E00A0, 0x7E00A1);

    // compute aggregated location for Link into a single 24-bit number:
    auto location =
      uint32(in_dark_world & 1) << 17 |
      uint32(in_dungeon & 1) << 16 |
      uint32(in_dungeon != 0 ? dungeon_room : overworld_room);

    ppu::frame.text(0, 8, fmtHex(location, 6));
  }
}
