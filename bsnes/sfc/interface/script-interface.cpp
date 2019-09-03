
struct ScriptFrame {
  uint16_t *&output = ppuFrame.output;
  uint &pitch = ppuFrame.pitch;
  uint &width = ppuFrame.width;
  uint &height = ppuFrame.height;

  uint16_t color = 0x7fff;
  auto get_color() -> uint16_t { return color; }
  auto set_color(uint16_t color_p) -> void { color = uclamp<15>(color_p); }

  enum DrawOp : int {
    op_solid,
    op_alpha,
    op_xor,
  } draw_op = op_solid;
  auto get_draw_op() -> DrawOp { return draw_op; }
  auto set_draw_op(DrawOp draw_op_p) { draw_op = draw_op_p; }

  uint8 alpha = 255;
  auto get_alpha() -> uint8 { return alpha; }
  auto set_alpha(uint8 alpha_p) { alpha = uclamp<5>(alpha_p); }

  auto inline draw(uint16_t *p) {
    switch (draw_op) {
      case op_alpha: {
	uint dr = (*p & 0x001f);
	uint dg = (*p & 0x03e0) >> 5;
	uint db = (*p & 0x7c00) >> 10;
	uint sr = (color & 0x001f);
	uint sg = (color & 0x03e0) >> 5;
	uint sb = (color & 0x7c00) >> 10;

	*p =
	  (((sr * alpha) + (dr * (31 - alpha))) / 31) |
	  (((sg * alpha) + (dg * (31 - alpha))) / 31) << 5 |
	  (((sb * alpha) + (db * (31 - alpha))) / 31) << 10;
	break;
      }

      case op_xor:
        *p ^= color;
        break;

      case op_solid:
      default:
	*p = color;
	break;
    }
  }

  int y_offset = 8;
  auto get_y_offset() -> int { return y_offset; }
  auto set_y_offset(int y_offs) -> void { y_offset = y_offs; }

  auto read_pixel(int x, int y) -> uint16_t {
    y += y_offset;
    if (x >= 0 && y >= 0 && x < (int) width && y < (int) height) {
      return output[y * pitch + x];
    }
    // impossible color on 15-bit RGB system:
    return 0xffff;
  }

  auto pixel(int x, int y) -> void {
    y += y_offset;
    if (x >= 0 && y >= 0 && x < (int) width && y < (int) height) {
      draw(&output[y * pitch + x]);
    }
  }

  // draw a horizontal line from x=lx to lx+w on y=ty:
  auto hline(int lx, int ty, int w) -> void {
    for (int x = lx; x < lx + w; ++x) {
      pixel(x, ty);
    }
  }

  // draw a vertical line from y=ty to ty+h on x=lx:
  auto vline(int lx, int ty, int h) -> void {
    for (int y = ty; y < ty + h; ++y) {
      pixel(lx, y);
    }
  }

  // draw a rectangle:
  auto rect(int x, int y, int w, int h) -> void {
    hline(x, y, w);
    hline(x + 1, y + h - 1, w - 1);
    vline(x, y + 1, h - 1);
    vline(x + w - 1, y + 1, h - 2);
  }
} scriptFrame;

uint8_t script_bus_read_u8(uint32_t addr) {
  return bus.read(addr, 0);
}

uint16_t script_bus_read_u16(uint32_t addr0, uint32_t addr1) {
  return bus.read(addr0, 0) | (bus.read(addr1,0) << 8u);
}

// string support:

struct CNallStringFactory : public asIStringFactory {
  CNallStringFactory() {}
  ~CNallStringFactory() {}

  const void *GetStringConstant(const char *data, asUINT length) {
    string *str = new string();
    str->resize(length);
    memory::copy(str->get(), str->capacity(), data, length);

    return reinterpret_cast<const void*>(str);
  }

  int ReleaseStringConstant(const void *str) {
    if (str == nullptr)
      return asERROR;

    const_cast<string *>(reinterpret_cast<const string*>(str))->reset();

    return asSUCCESS;
  }

  int GetRawStringData(const void *str, char *data, asUINT *length) const {
    if (str == nullptr)
      return asERROR;

    string *s = const_cast<string *>(reinterpret_cast<const string*>(str));

    if (length)
      *length = (asUINT)s->size();

    if (data)
      memory::copy(data, s->data(), s->size());

    return asSUCCESS;
  }
};

static CNallStringFactory *stringFactory = nullptr;

CNallStringFactory *GetStdStringFactorySingleton() {
  if(stringFactory == nullptr) {
    stringFactory = new CNallStringFactory();
  }
  return stringFactory;
}

static void ConstructString(string *thisPointer) {
  new(thisPointer) string();
}

static void CopyConstructString(const string &other, string *thisPointer) {
  new(thisPointer) string(other);
}

static void DestructString(string *thisPointer) {
  thisPointer->~string();
}

static void script_message(const string *msg) {
  printf("script: %s\n", msg->data());
}

auto Interface::registerScriptDefs() -> void {
  int r;

  script.engine = platform->scriptEngine();

  // register global functions for the script to use:
  auto defaultNamespace = script.engine->GetDefaultNamespace();

  {
    // register string type:
    r = script.engine->RegisterObjectType("string", sizeof(string), asOBJ_VALUE | asOBJ_APP_CLASS_CDAK); assert( r >= 0 );
    r = script.engine->RegisterStringFactory("string", GetStdStringFactorySingleton());

    // Register the object operator overloads
    r = script.engine->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT,  "void f()",                    asFUNCTION(ConstructString), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT,  "void f(const string &in)",    asFUNCTION(CopyConstructString), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("string", asBEHAVE_DESTRUCT,   "void f()",                    asFUNCTION(DestructString),  asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("string", "string &opAssign(const string &in)", asMETHODPR(string, operator =, (const string&), string&), asCALL_THISCALL); assert(r >= 0);
    //todo[jsd] add more string functions if necessary
  }

  r = script.engine->RegisterGlobalFunction("void message(const string &in msg)", asFUNCTION(script_message), asCALL_CDECL); assert(r >= 0);

  // default SNES:Bus memory functions:
  r = script.engine->SetDefaultNamespace("SNES::Bus"); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 read_u8(uint32 addr)",  asFUNCTION(script_bus_read_u8), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint16 read_u16(uint32 addr0, uint32 addr1)", asFUNCTION(script_bus_read_u16), asCALL_CDECL); assert(r >= 0);
  //todo[jsd] add write functions

  // define SNES::PPU::Frame object type:
  r = script.engine->SetDefaultNamespace("SNES::PPU"); assert(r >= 0);
  r = script.engine->RegisterObjectType("Frame", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);

  // adjust y_offset of drawing functions (default 8):
  r = script.engine->RegisterObjectMethod("Frame", "int get_y_offset()", asMETHODPR(ScriptFrame, get_y_offset, (), int), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void set_y_offset(int y_offs)", asMETHODPR(ScriptFrame, set_y_offset, (int), void), asCALL_THISCALL); assert(r >= 0);

  // set color to use for drawing functions (15-bit RGB):
  r = script.engine->RegisterObjectMethod("Frame", "uint16 get_color()", asMETHODPR(ScriptFrame, get_color, (), uint16), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void set_color(uint16 color)", asMETHODPR(ScriptFrame, set_color, (uint16), void), asCALL_THISCALL); assert(r >= 0);

  // set alpha to use for drawing functions (0..31):
  r = script.engine->RegisterObjectMethod("Frame", "uint8 get_alpha()", asMETHODPR(ScriptFrame, get_alpha, (), uint8), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void set_alpha(uint8 alpha)", asMETHODPR(ScriptFrame, set_alpha, (uint8), void), asCALL_THISCALL); assert(r >= 0);

  // register the DrawOp enum:
  r = script.engine->RegisterEnum("DrawOp"); assert(r >= 0);
  r = script.engine->RegisterEnumValue("DrawOp", "op_solid", ScriptFrame::DrawOp::op_solid); assert(r >= 0);
  r = script.engine->RegisterEnumValue("DrawOp", "op_alpha", ScriptFrame::DrawOp::op_alpha); assert(r >= 0);
  r = script.engine->RegisterEnumValue("DrawOp", "op_xor", ScriptFrame::DrawOp::op_xor); assert(r >= 0);

  // set alpha to use for drawing functions (0..31):
  r = script.engine->RegisterObjectMethod("Frame", "DrawOp get_draw_op()", asMETHODPR(ScriptFrame, get_draw_op, (), ScriptFrame::DrawOp), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void set_draw_op(DrawOp draw_op)", asMETHODPR(ScriptFrame, set_draw_op, (ScriptFrame::DrawOp), void), asCALL_THISCALL); assert(r >= 0);

  // pixel access functions:
  r = script.engine->RegisterObjectMethod("Frame", "uint16 read_pixel(int x, int y)", asMETHODPR(ScriptFrame, read_pixel, (int, int), uint16_t), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void pixel(int x, int y)", asMETHODPR(ScriptFrame, pixel, (int, int), void), asCALL_THISCALL); assert(r >= 0);

  // primitive drawing functions:
  r = script.engine->RegisterObjectMethod("Frame", "void hline(int lx, int ty, int w)", asMETHODPR(ScriptFrame, hline, (int, int, int), void), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void vline(int lx, int ty, int h)", asMETHODPR(ScriptFrame, vline, (int, int, int), void), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void rect(int x, int y, int w, int h)", asMETHODPR(ScriptFrame, rect, (int, int, int, int), void), asCALL_THISCALL); assert(r >= 0);

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
