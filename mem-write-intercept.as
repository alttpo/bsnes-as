// script to test registering memory-write intercepts.
array<TilemapWrite@> writes;

class TilemapWrite {
  uint32 addr;
  uint8 value;

  TilemapWrite(uint32 addr, uint8 value) {
    this.addr = addr;
    this.value = value;
  }
};

void init() {
  bus::add_write_interceptor(0x7E2000, 0x2000, @tilemap_written);
}

void tilemap_written(uint32 addr, uint8 value) {
  // record the tilemap write:
  writes.insertLast(TilemapWrite(addr, value));
}

void pre_frame() {
  auto len = writes.length();
  if (len > 0) {
    message("pre_frame writes " + fmtInt(len));
    if (len <= 32) {
      for (uint i = 0; i < len; i++) {
        auto write = writes[i];
        message(fmtHex(write.addr, 6) + " <- " + fmtHex(write.value, 2));
      }
    }
    // clear the tilemap writes for the next frame:
    writes.resize(0);
  }

  // NOTE: directly writing to tilemap successfully affects Link's interaction with the room!
  //bus::write_u8(0x7e2886, 0xca);
  //bus::write_u8(0x7e2887, 0x0d);
}
