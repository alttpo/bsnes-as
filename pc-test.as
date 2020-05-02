
void pc_intercept(uint32 addr) {
  message("pc!");
}

void post_power(bool reset) {
  message("power!");
  cpu::register_pc_interceptor(0x0080B5, @pc_intercept);
}
