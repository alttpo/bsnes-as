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

uint8 vmaddrl, vmaddrh;

void ppu_written(uint32 addr, uint8 value) {
  if (addr == 0x2116) vmaddrl = value;
  else if (addr == 0x2117) vmaddrh = value;
  else message("bus::write_u8(0x" + fmtHex(addr, 6) + ", 0x" + fmtHex(value, 2) + ");");
}

void dma_interceptor(cpu::DMAIntercept @dma) {
  // Find DMA writes to tile memory in VRAM:
  if (
    // ALTTP specific:
    dma.sourceBank == 0x00 && dma.sourceAddress >= 0x1006 && dma.sourceAddress < 0x1100 &&
    // writing to 0x2118, 0x2119 VMDATAL & VMDATAH regs:
    dma.targetAddress == 0x18 &&
    vmaddrh < 0x40
  ) {
    message("DMA[" + fmtInt(dma.channel) + "] from 0x" +
      fmtHex(dma.sourceBank, 2) + fmtHex(dma.sourceAddress, 4) +
      " to 0x" + fmtHex(0x2100 | uint16(dma.targetAddress), 2) +
      " (ppu 0x" + fmtHex(uint16(vmaddrh) << 8 | vmaddrl, 4) + ") size 0x" + fmtHex(dma.transferSize, 4)
    );
  }
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
