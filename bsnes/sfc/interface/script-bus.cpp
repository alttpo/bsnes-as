
struct Bus {
  static auto read_u8(uint32 addr) -> uint8 {
    return ::SuperFamicom::bus.read(addr, 0);
  }

  static auto read_block_u8(uint32 addr, uint offs, uint16 size, CScriptArray *output) -> void {
    if (output == nullptr) {
      asGetActiveContext()->SetException("output array cannot be null", true);
      return;
    }
    if (output->GetElementTypeId() != asTYPEID_UINT8) {
      asGetActiveContext()->SetException("output array must be of type uint8[]", true);
      return;
    }

    for (uint32 a = 0; a < size; a++) {
      auto value = ::SuperFamicom::bus.read(addr + a);
      output->SetValue(offs + a, &value);
    }
  }

  static auto read_block_u16(uint32 addr, uint offs, uint16 size, CScriptArray *output) -> void {
    if (output == nullptr) {
      asGetActiveContext()->SetException("output array cannot be null", true);
      return;
    }
    if (output->GetElementTypeId() != asTYPEID_UINT16) {
      asGetActiveContext()->SetException("output array must be of type uint16[]", true);
      return;
    }

    for (uint32 a = 0; a < size; a++) {
      auto lo = ::SuperFamicom::bus.read(addr + (a << 1u) + 0u);
      auto hi = ::SuperFamicom::bus.read(addr + (a << 1u) + 1u);
      auto value = uint16(lo) | (uint16(hi) << 8u);
      output->SetValue(offs + a, &value);
    }
  }

  static auto read_u16(uint32 addr0, uint32 addr1) -> uint16 {
    return ::SuperFamicom::bus.read(addr0, 0) | (::SuperFamicom::bus.read(addr1,0) << 8u);
  }

  static auto write_u8(uint32 addr, uint8 data) -> void {
    Memory::GlobalWriteEnable = true;
    {
      // prevent scripts from intercepting their own writes:
      ::SuperFamicom::bus.write_no_intercept(addr, data);
    }
    Memory::GlobalWriteEnable = false;
  }

  static auto write_u16(uint32 addr0, uint32 addr1, uint16 data) -> void {
    Memory::GlobalWriteEnable = true;
    {
      // prevent scripts from intercepting their own writes:
      ::SuperFamicom::bus.write_no_intercept(addr0, data & 0x00FFu);
      ::SuperFamicom::bus.write_no_intercept(addr1, (data >> 8u) & 0x00FFu);
    }
    Memory::GlobalWriteEnable = false;
  }

  static auto write_block_u8(uint32 addr, uint offs, uint16 size, CScriptArray *input) -> void {
    if (input == nullptr) {
      asGetActiveContext()->SetException("input array cannot be null", true);
      return;
    }
    if (input->GetElementTypeId() != asTYPEID_UINT8) {
      asGetActiveContext()->SetException("input array must be of type uint8[]", true);
      return;
    }

    Memory::GlobalWriteEnable = true;
    {
      for (uint32 a = 0; a < size; a++) {
        auto value = *(uint8*)input->At(offs + a);
        ::SuperFamicom::bus.write_no_intercept(addr + a, value);
      }
    }
    Memory::GlobalWriteEnable = false;
  }

  static auto write_block_u16(uint32 addr, uint offs, uint16 size, CScriptArray *input) -> void {
    if (input == nullptr) {
      asGetActiveContext()->SetException("input array cannot be null", true);
      return;
    }
    if (input->GetElementTypeId() != asTYPEID_UINT16) {
      asGetActiveContext()->SetException("input array must be of type uint16[]", true);
      return;
    }

    Memory::GlobalWriteEnable = true;
    {
      for (uint32 a = 0; a < size; a++) {
        auto value = *(uint16*)input->At(offs + a);
        auto lo = uint8(value & 0xFF);
        auto hi = uint8((value >> 8) & 0xFF);
        ::SuperFamicom::bus.write_no_intercept(addr + (a << 1u) + 0u, lo);
        ::SuperFamicom::bus.write_no_intercept(addr + (a << 1u) + 1u, hi);
      }
    }
    Memory::GlobalWriteEnable = false;
  }

  struct write_interceptor {
    asIScriptFunction *cb;
    asIScriptContext  *ctx;

    write_interceptor(asIScriptFunction *cb, asIScriptContext *ctx) : cb(cb), ctx(ctx) {
      cb->AddRef();
    }
    write_interceptor(const write_interceptor& other) : cb(other.cb), ctx(other.ctx) {
      cb->AddRef();
    }
    write_interceptor(const write_interceptor&& other) : cb(other.cb), ctx(other.ctx) {
    }
    ~write_interceptor() {
      cb->Release();
    }

    auto operator()(uint addr, uint8 new_value) -> void {
      ctx->Prepare(cb);
      ctx->SetArgDWord(0, addr);
      ctx->SetArgByte(1, new_value);
      ctx->Execute();
    }
  };

  static auto add_write_interceptor(const string *addr, uint32 size, asIScriptFunction *cb) -> void {
    auto ctx = asGetActiveContext();

    ::SuperFamicom::bus.add_write_interceptor(*addr, size, write_interceptor(cb, ctx));
  }

  struct dma_interceptor {
    asIScriptFunction *cb;
    asIScriptContext  *ctx;

    dma_interceptor(asIScriptFunction *cb, asIScriptContext *ctx) : cb(cb), ctx(ctx) {
      cb->AddRef();
    }
    dma_interceptor(const dma_interceptor& other) : cb(other.cb), ctx(other.ctx) {
      cb->AddRef();
    }
    dma_interceptor(const dma_interceptor&& other) : cb(other.cb), ctx(other.ctx) {
    }
    ~dma_interceptor() {
      cb->Release();
    }

    auto operator()(const CPU::DMAIntercept &dma) -> void {
      ctx->Prepare(cb);
      ctx->SetArgObject(0, (void *)&dma);
      ctx->Execute();
    }
  };

  static auto register_dma_interceptor(asIScriptFunction *cb) -> void {
    auto ctx = asGetActiveContext();

    ::SuperFamicom::cpu.register_dma_interceptor(dma_interceptor(cb, ctx));
  }
} bus;
