// script to test registering memory-write intercepts.
void init() {
  bus::add_write_interceptor(0x7E2000, 0x2000, @tilemap_written);
}

void tilemap_written(uint32 addr, uint8 new_value) {
  message(fmtHex(addr, 6) + " <- " + fmtHex(new_value, 2));
}
