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

void tilemap_written(uint32 addr, uint8 value) {
  // record the tilemap write:
  //writes.insertLast(TilemapWrite(addr, value));
  message("0x" + fmtHex(addr, 6) + " <- 0x" + fmtHex(value, 2));
}

void dma_written(uint32 addr, uint8 value) {
  message("bus::write_u8(0x" + fmtHex(addr, 6) + ", 0x" + fmtHex(value, 2) + ");");
}

void ppu_written(uint32 addr, uint8 value) {
  message("bus::write_u8(0x" + fmtHex(addr, 6) + ", 0x" + fmtHex(value, 2) + ");");
}

void dma_interceptor(cpu::DMAIntercept @dma) {
  message("DMA[" + fmtInt(dma.channel) + "]...");
}

void init() {
  bus::add_write_interceptor("7e:2000-3fff", 0, @tilemap_written);
  bus::add_write_interceptor("00-3f,80-bf:2116-2119", 0, @ppu_written);
  cpu::register_dma_interceptor(@dma_interceptor);
}

void pre_frame() {
  auto len = writes.length();
  message("pre_frame writes " + fmtInt(len));
  if (len > 0) {
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
