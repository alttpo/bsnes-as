// Communicate with ROM hack via memory at $7F7667[0x6719]

class OAMSprite {
  uint8 x;
  uint8 y;
  uint8 chr;
  uint8 flags;

  OAMSprite() {}

  OAMSprite(uint8 x, uint8 y, uint8 chr, uint8 flags) {
    this.x = x;
    this.y = y;
    this.chr = chr;
    this.flags = flags;
  }
};

class Packet {
  uint32 addr;

  uint32 location;
  uint16 x, y, z;
  uint16 xoffs, yoffs;

  uint16 oam_size;
  array<OAMSprite> oam_table;

  Packet(uint32 addr) {
    this.addr = addr;
  }

  void readRAN() {
    auto a = addr;
    location = uint32(bus::read_u16(a+0, a+1)) | (uint32(bus::read_u8(a+2)) << 16); a += 3;
    x = bus::read_u16(a+0, a+1); a += 2;
    y = bus::read_u16(a+0, a+1); a += 2;
    z = bus::read_u16(a+0, a+1); a += 2;
    xoffs = bus::read_u16(a+0, a+1); a += 2;
    yoffs = bus::read_u16(a+0, a+1); a += 2;

    // read oam_table:
    oam_size = bus::read_u16(a+0, a+1); a += 2;
    auto len = oam_size >> 2;
    oam_table.resize(len);
    for (uint i = 0; i < len; i++) {
      oam_table[i].x = bus::read_u8(a); a++;
      oam_table[i].y = bus::read_u8(a); a++;
      oam_table[i].chr = bus::read_u8(a); a++;
      oam_table[i].flags = bus::read_u8(a); a++;
    }
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
  ppu::frame.text(52, 0, fmtHex(local.x, 4));
  ppu::frame.text(88, 0, fmtHex(local.y, 4));
  ppu::frame.text(124, 0, fmtHex(local.z, 4));
  ppu::frame.text(160, 0, fmtHex(local.xoffs, 4));
  ppu::frame.text(196, 0, fmtHex(local.yoffs, 4));

  ppu::frame.text(0, 8, fmtHex(local.oam_size, 4));

  if (false) {
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
