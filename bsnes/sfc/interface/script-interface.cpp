struct ScriptFrame {
  int y_offset = 8;

  uint16_t *&output = ppuFrame.output;
  uint &pitch = ppuFrame.pitch;
  uint &width = ppuFrame.width;
  uint &height = ppuFrame.height;

  auto get_y_offset() -> int {
    return y_offset;
  }

  auto set_y_offset(int y_offs) -> void {
    y_offset = y_offs;
  }

  auto set(int x, int y, uint16_t color) -> void {
    y += y_offset;
    if (x >= 0 && y >= 0 && x < (int) width && y < (int) height) {
      output[y * pitch + x] = color;
    }
  }

  auto get(int x, int y) -> uint16_t {
    y += y_offset;
    if (x >= 0 && y >= 0 && x < (int) width && y < (int) height) {
      return output[y * pitch + x];
    }
    // impossible color on 15-bit RGB system:
    return 0xffff;
  }
} scriptFrame;

uint8_t script_bus_read_u8(uint32_t addr) {
  return bus.read(addr, 0);
}

uint16_t script_bus_read_u16(uint32_t addr0, uint32_t addr1) {
  return bus.read(addr0, 0) | (bus.read(addr1,0) << 8u);
}

auto Interface::registerScriptDefs() -> void {
  int r;

  script.engine = platform->scriptEngine();

  // register global functions for the script to use:
  auto defaultNamespace = script.engine->GetDefaultNamespace();
  // default SNES:Bus memory functions:
  r = script.engine->SetDefaultNamespace("SNES::Bus"); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 read_u8(uint32 addr)",  asFUNCTION(script_bus_read_u8), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint16 read_u16(uint32 addr0, uint32 addr1)", asFUNCTION(script_bus_read_u16), asCALL_CDECL); assert(r >= 0);
  //todo[jsd] add write functions

  // define SNES::PPU::Frame object type:
  r = script.engine->SetDefaultNamespace("SNES::PPU"); assert(r >= 0);
  r = script.engine->RegisterObjectType("Frame", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);

  // adjust y_offset of drawing functions:
  r = script.engine->RegisterObjectMethod("Frame", "int get_y_offset()", asMETHODPR(ScriptFrame, get_y_offset, (), int), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void set_y_offset(int y_offs)", asMETHODPR(ScriptFrame, set_y_offset, (int), void), asCALL_THISCALL); assert(r >= 0);

  // pixel access functions:
  r = script.engine->RegisterObjectMethod("Frame", "uint16 get(int x, int y)", asMETHODPR(ScriptFrame, get, (int, int), uint16_t), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void set(int x, int y, uint16 color)", asMETHODPR(ScriptFrame, set, (int, int, uint16_t), void), asCALL_THISCALL); assert(r >= 0);

  // global property to access current frame:
  r = script.engine->RegisterGlobalProperty("Frame frame", &scriptFrame); assert(r >= 0);
  r = script.engine->SetDefaultNamespace(defaultNamespace); assert(r >= 0);

  // create context:
  script.context = script.engine->CreateContext();
}

auto Interface::loadScript(string location) -> void {
  int r;

  if (!inode::exists(location)) return;

//  if (script.context) {
//    script.context->Release();
//  }
  if (script.module) {
    script.module->Discard();
  }

  // load script from file:
  string scriptSource = string::read(location);

  // add script into module:
  script.module = script.engine->GetModule(nullptr, asGM_ALWAYS_CREATE);
  r = script.module->AddScriptSection("script", scriptSource.begin(), scriptSource.length());
  assert(r >= 0);

  // compile module:
  r = script.module->Build();
  assert(r >= 0);

  // bind to functions in the script:
  script.funcs.init = script.module->GetFunctionByDecl("void init()");
  script.funcs.main = script.module->GetFunctionByDecl("void main()");
  script.funcs.postRender = script.module->GetFunctionByDecl("void postRender()");

  if (script.funcs.init) {
    script.context->Prepare(script.funcs.init);
    script.context->Execute();
  }
}
