auto Bus::mirror(uint addr, uint size) -> uint {
  if(size == 0) return 0;
  uint base = 0;
  uint mask = 1 << 23;
  while(addr >= size) {
    while(!(addr & mask)) mask >>= 1;
    addr -= mask;
    if(size > mask) {
      size -= mask;
      base += mask;
    }
    mask >>= 1;
  }
  return base + addr;
}

auto Bus::reduce(uint addr, uint mask) -> uint {
  while(mask) {
    uint bits = (mask & -mask) - 1;
    addr = ((addr >> 1) & ~bits) | (addr & bits);
    mask = (mask & (mask - 1)) >> 1;
  }
  return addr;
}

auto Bus::read(uint addr, uint8 data) -> uint8 {
  return reader[lookup[addr]](target[addr], data);
}

auto Bus::write(uint addr, uint8 data) -> void {
  uint8 fn = lookup[addr];
  uint32 offset = target[addr];

  // call interceptor before actual write takes place:
  interceptor[interceptor_lookup[addr]](addr, data, [=](){ return reader[fn](offset, 0); });

  return writer[fn](offset, data);
}

auto Bus::write_no_intercept(uint addr, uint8 data) -> void {
  return writer[lookup[addr]](target[addr], data);
}
