
void pc_intercept(uint32 addr) {
  message("pc=" + fmtHex(cpu::r.pc, 6) +
          " a=" + fmtHex(cpu::r.a, 4) +
          " x=" + fmtHex(cpu::r.x, 4) +
          " y=" + fmtHex(cpu::r.y, 4) +
          " z=" + (cpu::r.p.z ? "1" : "0")
  );
}

void post_power(bool reset) {
  message("power!");
  cpu::register_pc_interceptor(0x0080B5, @pc_intercept);
}
