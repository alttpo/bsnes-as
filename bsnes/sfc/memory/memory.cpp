#include <sfc/sfc.hpp>

namespace SuperFamicom {

bool Memory::GlobalWriteEnable = false;
Bus bus;

Bus::~Bus() {
  if(lookup) delete[] lookup;
  if(target) delete[] target;
  if(interceptor_lookup) delete[] interceptor_lookup;
}

auto Bus::reset() -> void {
  for(uint id : range(256)) {
    reader[id].reset();
    writer[id].reset();
    counter[id] = 0;
  }

  if(lookup) delete[] lookup;
  if(target) delete[] target;

  lookup = new uint8 [16 * 1024 * 1024]();
  target = new uint32[16 * 1024 * 1024]();

  reader[0] = [](uint, uint8 data) -> uint8 { return data; };
  writer[0] = [](uint, uint8) -> void {};

  // [jsd]
  reset_interceptors();
}

auto Bus::reset_interceptors() -> void {
  for(uint id : range(256)) {
    interceptor[id].reset();
    interceptor_counter[id] = 0;
  }

  if(interceptor_lookup) delete[] interceptor_lookup;

  interceptor_lookup = new uint8 [16 * 1024 * 1024]();

  interceptor[0] = [](uint, uint8, function<uint8()>) -> void {};
}

auto Bus::map(
  const function<uint8 (uint, uint8)>& read,
  const function<void  (uint, uint8)>& write,
  const string& addr, uint size, uint base, uint mask
) -> uint {
  uint id = 1;
  while(counter[id]) {
    if(++id >= 256) return print("SFC error: bus map exhausted\n"), 0;
  }

  reader[id] = read;
  writer[id] = write;

  auto p = addr.split(":", 1L);
  auto banks = p(0).split(",");
  auto addrs = p(1).split(",");
  for(auto& bank : banks) {
    for(auto& addr : addrs) {
      auto bankRange = bank.split("-", 1L);
      auto addrRange = addr.split("-", 1L);
      uint bankLo = bankRange(0).hex();
      uint bankHi = bankRange(1, bankRange(0)).hex();
      uint addrLo = addrRange(0).hex();
      uint addrHi = addrRange(1, addrRange(0)).hex();

      for(uint bank = bankLo; bank <= bankHi; bank++) {
        for(uint addr = addrLo; addr <= addrHi; addr++) {
          uint pid = lookup[bank << 16 | addr];
          if(pid && --counter[pid] == 0) {
            reader[pid].reset();
            writer[pid].reset();
          }

          uint offset = reduce(bank << 16 | addr, mask);
          if(size) base = mirror(base, size);
          if(size) offset = base + mirror(offset, size - base);
          lookup[bank << 16 | addr] = id;
          target[bank << 16 | addr] = offset;
          counter[id]++;
        }
      }
    }
  }

  return id;
}

auto Bus::unmap(const string& addr) -> void {
  auto p = addr.split(":", 1L);
  auto banks = p(0).split(",");
  auto addrs = p(1).split(",");
  for(auto& bank : banks) {
    for(auto& addr : addrs) {
      auto bankRange = bank.split("-", 1L);
      auto addrRange = addr.split("-", 1L);
      uint bankLo = bankRange(0).hex();
      uint bankHi = bankRange(1, bankRange(0)).hex();
      uint addrLo = addrRange(0).hex();
      uint addrHi = addrRange(1, addrRange(1)).hex();

      for(uint bank = bankLo; bank <= bankHi; bank++) {
        for(uint addr = addrLo; addr <= addrHi; addr++) {
          uint pid = lookup[bank << 16 | addr];
          if(pid && --counter[pid] == 0) {
            reader[pid].reset();
            writer[pid].reset();
          }

          lookup[bank << 16 | addr] = 0;
          target[bank << 16 | addr] = 0;
        }
      }
    }
  }
}

auto Bus::add_write_interceptor(
  const string& addr,
  const function<void (uint, uint8, function<uint8()>)> &intercept
) -> uint {
  uint id = 1;
  while(interceptor_counter[id]) {
    if(++id >= 256) return print("SFC error: bus intercept map exhausted\n"), 0;
  }

  interceptor[id] = intercept;

  auto p = addr.split(":", 1L);
  auto banks = p(0).split(",");
  auto addrs = p(1).split(",");
  for(auto& bank : banks) {
    for(auto& addr : addrs) {
      auto bankRange = bank.split("-", 1L);
      auto addrRange = addr.split("-", 1L);
      uint bankLo = bankRange(0).hex();
      uint bankHi = bankRange(1, bankRange(0)).hex();
      uint addrLo = addrRange(0).hex();
      uint addrHi = addrRange(1, addrRange(0)).hex();

      for(uint bank = bankLo; bank <= bankHi; bank++) {
        for(uint addr = addrLo; addr <= addrHi; addr++) {
          uint pid = interceptor_lookup[bank << 16u | addr];
          if(pid && --interceptor_counter[pid] == 0) {
            interceptor[pid].reset();
          }

          interceptor_lookup[bank << 16u | addr] = id;
          interceptor_counter[id]++;
        }
      }
    }
  }

  return id;
}

}
