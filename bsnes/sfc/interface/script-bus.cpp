
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
    if (offs + size > output->GetSize()) {
      asGetActiveContext()->SetException("offset and size exceed the bounds of the output array", true);
      return;
    }
    if (addr + size > 0xFFFFFF) {
      asGetActiveContext()->SetException("addr + size exceeds the address space", true);
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
    if (offs + size > output->GetSize()) {
      asGetActiveContext()->SetException("offset and size exceed the bounds of the output array", true);
      return;
    }
    if (addr + (size << 1) > 0xFFFFFF) {
      asGetActiveContext()->SetException("addr + size exceeds the address space", true);
      return;
    }

    for (uint32 a = 0; a < size; a++) {
      auto lo = ::SuperFamicom::bus.read(addr + (a << 1u) + 0u);
      auto hi = ::SuperFamicom::bus.read(addr + (a << 1u) + 1u);
      auto value = uint16(lo) | (uint16(hi) << 8u);
      output->SetValue(offs + a, &value);
    }
  }

  static auto read_u16(uint32 addr) -> uint16 {
    return ::SuperFamicom::bus.read(addr & 0xFFFFFF, 0) | (::SuperFamicom::bus.read((addr + 1) & 0xFFFFFF,0) << 8u);
  }

  static auto read_u16_ext(uint32 addr0, uint32 addr1) -> uint16 {
    return ::SuperFamicom::bus.read(addr0 & 0xFFFFFF, 0) | (::SuperFamicom::bus.read(addr1 & 0xFFFFFF, 0) << 8u);
  }

  static auto write_u8(uint32 addr, uint8 data) -> void {
    Memory::GlobalWriteEnable = true;
    {
      // prevent scripts from intercepting their own writes:
      ::SuperFamicom::bus.write_no_intercept(addr, data);
    }
    Memory::GlobalWriteEnable = false;
  }

  static auto write_u16(uint32 addr, uint16 data) -> void {
    Memory::GlobalWriteEnable = true;
    {
      // prevent scripts from intercepting their own writes:
      ::SuperFamicom::bus.write_no_intercept((addr+0) & 0xFFFFFF, data & 0x00FFu);
      ::SuperFamicom::bus.write_no_intercept((addr+1) & 0xFFFFFF, (data >> 8u) & 0x00FFu);
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
    if (offs + size > input->GetSize()) {
      asGetActiveContext()->SetException("offset and size exceed the bounds of the input array", true);
      return;
    }
    if (addr + size > 0xFFFFFF) {
      asGetActiveContext()->SetException("addr + size exceeds the address space", true);
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
    if (offs + size > input->GetSize()) {
      asGetActiveContext()->SetException("offset and size exceed the bounds of the input array", true);
      return;
    }
    if (addr + (size << 1) > 0xFFFFFF) {
      asGetActiveContext()->SetException("addr + size exceeds the address space", true);
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

  static auto map_array(const string *addr, uint32 size, uint32 mask, uint32 offset, CScriptArray *memory) -> void {
    // this never gets released because the map read/write functions are also never released:
    memory->AddRef();

    function<uint8 (uint, uint8)> reader = [=] (uint addr, uint8 data) -> uint8 {
      auto idx = (addr & mask) + offset;
      //printf("read  at(0x%04x)", idx);
      auto val = *(uint8*)memory->At(idx);
      //printf(" -> 0x%02x\n", val);
      return *(uint8*)memory->At(idx);
    };
    function<void  (uint, uint8)> writer = [=] (uint addr, uint8 data) -> void {
      auto idx = (addr & mask) + offset;
      //printf("write at(0x%04x), 0x%02x\n", idx, data);
      *(uint8*)memory->At(idx) = data;
    };

    ::SuperFamicom::bus.map(reader, writer, *addr, size);
  }

  struct write_interceptor {
    asIScriptFunction *cb;

    write_interceptor(asIScriptFunction *cb) : cb(cb) {
      auto refs = cb->AddRef();
      //printf("ctor() this=%p cb=%p refs=%d\n", this, cb, refs);
    }
    write_interceptor(const write_interceptor& other) : cb(other.cb) {
      auto refs = cb->AddRef();
      //printf("copy() this=%p cb=%p refs=%d\n", this, cb, refs);
    }
    write_interceptor(const write_interceptor&& other) : cb(other.cb) {
      //printf("move() this=%p cb=%p\n", this, cb);
    }
    ~write_interceptor() {
      auto refs = cb->Release();
      //printf("dtor() this=%p cb=%p refs=%d\n", this, cb, refs);
    }

    auto operator()(uint addr, uint8 new_value, const function<uint8()> &get_old_value) -> void {
      platform->scriptInvokeFunction(cb, [=](auto ctx) {
        //printf("call() this=%p ctx=%p cb=%p\n", this, ctx, cb);
        ctx->SetArgDWord(0, addr);
        ctx->SetArgByte(1, get_old_value());
        ctx->SetArgByte(2, new_value);
      });
    }
  };

  static auto add_write_interceptor(const string *addr, asIScriptFunction *cb) -> void {
    ::SuperFamicom::bus.add_write_interceptor(*addr, write_interceptor(cb));
  }

  struct dma_interceptor {
    asIScriptFunction *cb;

    dma_interceptor(asIScriptFunction *cb) : cb(cb) {
      cb->AddRef();
    }
    dma_interceptor(const dma_interceptor& other) : cb(other.cb) {
      cb->AddRef();
    }
    dma_interceptor(const dma_interceptor&& other) : cb(other.cb) {
    }
    ~dma_interceptor() {
      cb->Release();
    }

    auto operator()(const CPU::DMAIntercept &dma) -> void {
      platform->scriptInvokeFunction(cb, [=](auto ctx) {
        ctx->SetArgObject(0, (void *)&dma);
      });
    }
  };

  static auto register_dma_interceptor(asIScriptFunction *cb) -> void {
    ::SuperFamicom::cpu.register_dma_interceptor(dma_interceptor(cb));
  }

  struct pc_interceptor {
    asIScriptFunction *cb;

    pc_interceptor(asIScriptFunction *cb) : cb(cb) {
      cb->AddRef();
    }
    pc_interceptor(const pc_interceptor& other) : cb(other.cb) {
      cb->AddRef();
    }
    pc_interceptor(const pc_interceptor&& other) : cb(other.cb) {
    }
    ~pc_interceptor() {
      cb->Release();
    }

    auto operator()(uint32 addr) -> void {
      platform->scriptInvokeFunction(cb, [=](auto ctx) {
        ctx->SetArgDWord(0, addr);
      });
    }
  };

  static auto register_pc_interceptor(uint32 addr, asIScriptFunction *cb) -> void {
    ::SuperFamicom::cpu.register_pc_callback(addr, pc_interceptor(cb));
  }

  static auto unregister_pc_interceptor(uint32 addr) -> void {
    ::SuperFamicom::cpu.unregister_pc_callback(addr);
  }
} bus;

auto RegisterBus(asIScriptEngine *e) -> void {
  int r;

  {
    r = e->SetDefaultNamespace("bus"); assert(r >= 0);

    // read functions:
    r = e->RegisterGlobalFunction("uint8 read_u8(uint32 addr)",  asFUNCTION(Bus::read_u8), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("uint16 read_u16(uint32 addr)", asFUNCTION(Bus::read_u16), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("uint16 read_u16_ext(uint32 addr0, uint32 addr1)", asFUNCTION(Bus::read_u16_ext), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("void read_block_u8(uint32 addr, uint offs, uint16 size, const array<uint8> &in output)",  asFUNCTION(Bus::read_block_u8), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("void read_block_u16(uint32 addr, uint offs, uint16 size, const array<uint16> &in output)",  asFUNCTION(Bus::read_block_u16), asCALL_CDECL); assert(r >= 0);

    // write functions:
    r = e->RegisterGlobalFunction("void write_u8(uint32 addr, uint8 data)", asFUNCTION(Bus::write_u8), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("void write_u16(uint32 addr, uint16 data)", asFUNCTION(Bus::write_u16), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("void write_block_u8(uint32 addr, uint offs, uint16 size, const array<uint8> &in output)", asFUNCTION(Bus::write_block_u8), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("void write_block_u16(uint32 addr, uint offs, uint16 size, const array<uint16> &in output)", asFUNCTION(Bus::write_block_u16), asCALL_CDECL); assert(r >= 0);

    // map function:
    r = e->RegisterGlobalFunction("void map(const string &in addr, uint32 size, uint32 mask, uint32 offset, array<uint8> @memory)", asFUNCTION(Bus::map_array), asCALL_CDECL); assert(r >= 0);

    r = e->RegisterFuncdef("void WriteInterceptCallback(uint32 addr, uint8 oldValue, uint8 newValue)"); assert(r >= 0);
    r = e->RegisterGlobalFunction("void add_write_interceptor(const string &in addr, WriteInterceptCallback @cb)", asFUNCTION(Bus::add_write_interceptor), asCALL_CDECL); assert(r >= 0);
  }

  {
    r = e->SetDefaultNamespace("cpu"); assert(r >= 0);

    r = e->RegisterObjectType    ("Registers", sizeof(CPU::Registers), asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = e->RegisterObjectType    ("RegisterFlags", sizeof(CPU::f8), asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = e->RegisterObjectProperty("RegisterFlags", "bool c", asOFFSET(CPU::f8, c)); assert(r >= 0);
    r = e->RegisterObjectProperty("RegisterFlags", "bool z", asOFFSET(CPU::f8, z)); assert(r >= 0);
    r = e->RegisterObjectProperty("RegisterFlags", "bool i", asOFFSET(CPU::f8, i)); assert(r >= 0);
    r = e->RegisterObjectProperty("RegisterFlags", "bool d", asOFFSET(CPU::f8, d)); assert(r >= 0);
    r = e->RegisterObjectProperty("RegisterFlags", "bool x", asOFFSET(CPU::f8, x)); assert(r >= 0);
    r = e->RegisterObjectProperty("RegisterFlags", "bool m", asOFFSET(CPU::f8, m)); assert(r >= 0);
    r = e->RegisterObjectProperty("RegisterFlags", "bool v", asOFFSET(CPU::f8, v)); assert(r >= 0);
    r = e->RegisterObjectProperty("RegisterFlags", "bool n", asOFFSET(CPU::f8, n)); assert(r >= 0);

    r = e->RegisterObjectProperty("Registers", "uint32        pc", asOFFSET(CPU::Registers, pc.d)); assert(r >= 0);
    r = e->RegisterObjectProperty("Registers", "uint16        a", asOFFSET(CPU::Registers, a.w)); assert(r >= 0);
    r = e->RegisterObjectProperty("Registers", "uint16        x", asOFFSET(CPU::Registers, x.w)); assert(r >= 0);
    r = e->RegisterObjectProperty("Registers", "uint16        y", asOFFSET(CPU::Registers, y.w)); assert(r >= 0);
    r = e->RegisterObjectProperty("Registers", "uint16        z", asOFFSET(CPU::Registers, z.w)); assert(r >= 0);
    r = e->RegisterObjectProperty("Registers", "uint16        s", asOFFSET(CPU::Registers, s.w)); assert(r >= 0);
    r = e->RegisterObjectProperty("Registers", "uint16        d", asOFFSET(CPU::Registers, d.w)); assert(r >= 0);
    r = e->RegisterObjectProperty("Registers", "uint8         b", asOFFSET(CPU::Registers, b)); assert(r >= 0);
    r = e->RegisterObjectProperty("Registers", "RegisterFlags p", asOFFSET(CPU::Registers, p)); assert(r >= 0);
    r = e->RegisterObjectProperty("Registers", "bool          e", asOFFSET(CPU::Registers, e)); assert(r >= 0);
    r = e->RegisterObjectProperty("Registers", "bool          irq", asOFFSET(CPU::Registers, irq)); assert(r >= 0);
    r = e->RegisterObjectProperty("Registers", "bool          wai", asOFFSET(CPU::Registers, wai)); assert(r >= 0);
    r = e->RegisterObjectProperty("Registers", "bool          stp", asOFFSET(CPU::Registers, stp)); assert(r >= 0);
    r = e->RegisterGlobalProperty("Registers r", &cpu.r); assert(r >= 0);

    r = e->RegisterObjectType    ("DMAIntercept", sizeof(CPU::DMAIntercept), asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint8  channel", asOFFSET(CPU::DMAIntercept, channel)); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint8  transferMode", asOFFSET(CPU::DMAIntercept, transferMode)); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint8  fixedTransfer", asOFFSET(CPU::DMAIntercept, fixedTransfer)); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint8  reverseTransfer", asOFFSET(CPU::DMAIntercept, reverseTransfer)); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint8  unused", asOFFSET(CPU::DMAIntercept, unused)); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint8  indirect", asOFFSET(CPU::DMAIntercept, indirect)); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint8  direction", asOFFSET(CPU::DMAIntercept, direction)); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint8  targetAddress", asOFFSET(CPU::DMAIntercept, targetAddress)); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint16 sourceAddress", asOFFSET(CPU::DMAIntercept, sourceAddress)); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint8  sourceBank", asOFFSET(CPU::DMAIntercept, sourceBank)); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint16 transferSize", asOFFSET(CPU::DMAIntercept, transferSize)); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint16 indirectAddress", asOFFSET(CPU::DMAIntercept, indirectAddress)); assert(r >= 0);
    r = e->RegisterObjectProperty("DMAIntercept", "uint8  indirectBank", asOFFSET(CPU::DMAIntercept, indirectBank)); assert(r >= 0);

    r = e->RegisterFuncdef("void DMAInterceptCallback(DMAIntercept @dma)"); assert(r >= 0);
    r = e->RegisterGlobalFunction("void register_dma_interceptor(DMAInterceptCallback @cb)", asFUNCTION(Bus::register_dma_interceptor), asCALL_CDECL); assert(r >= 0);

    r = e->RegisterFuncdef("void PCInterceptCallback(uint32 addr)"); assert(r >= 0);
    r = e->RegisterGlobalFunction("void register_pc_interceptor(uint32 addr, PCInterceptCallback @cb)", asFUNCTION(Bus::register_pc_interceptor), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("void unregister_pc_interceptor(uint32 addr)", asFUNCTION(Bus::unregister_pc_interceptor), asCALL_CDECL); assert(r >= 0);

  }
}
