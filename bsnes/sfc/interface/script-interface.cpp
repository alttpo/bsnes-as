
#include <sfc/sfc.hpp>

#include "vga-charset.cpp"
#include "script-string.cpp"

static uint8_t script_bus_read_u8(uint32_t addr) {
  return bus.read(addr, 0);
}

static uint16_t script_bus_read_u16(uint32_t addr0, uint32_t addr1) {
  return bus.read(addr0, 0) | (bus.read(addr1,0) << 8u);
}

static void script_message(const string *msg) {
  printf("script: %s\n", msg->data());
}

struct ScriptFrame {
  uint16_t *&output = ppuFrame.output;
  uint &pitch = ppuFrame.pitch;
  uint &width = ppuFrame.width;
  uint &height = ppuFrame.height;

  enum DrawOp : int {
    op_solid,
    op_alpha,
    op_xor,
  } draw_op = op_solid;
  auto get_draw_op() -> DrawOp { return draw_op; }
  auto set_draw_op(DrawOp draw_op_p) { draw_op = draw_op_p; }

  uint16_t color = 0x7fff;
  auto get_color() -> uint16_t { return color; }
  auto set_color(uint16_t color_p) -> void { color = uclamp<15>(color_p); }

  uint8 alpha = 31;
  auto get_alpha() -> uint8 { return alpha; }
  auto set_alpha(uint8 alpha_p) { alpha = uclamp<5>(alpha_p); }

  bool text_shadow = false;
  auto get_text_shadow() -> bool { return text_shadow; }
  auto set_text_shadow(bool text_shadow_p) { text_shadow = text_shadow_p; }

  auto (ScriptFrame::*draw_glyph)(int x, int y, int glyph) -> void = &ScriptFrame::draw_glyph_8;

  int font_height = 8;
  auto get_font_height() -> int { return font_height; }
  auto set_font_height(int font_height_p) {
    if (font_height_p <= 8) {
      font_height = 8;
      draw_glyph = &ScriptFrame::draw_glyph_8;
    } else {
      font_height = 16;
      draw_glyph = &ScriptFrame::draw_glyph_16;
    }
  }

  auto inline draw(uint16_t *p) {
    switch (draw_op) {
      case op_alpha: {
	uint db = (*p & 0x001fu);
	uint dg = (*p & 0x03e0u) >> 5u;
	uint dr = (*p & 0x7c00u) >> 10u;
	uint sb = (color & 0x001fu);
	uint sg = (color & 0x03e0u) >> 5u;
	uint sr = (color & 0x7c00u) >> 10u;

	*p =
	  (((sb * alpha) + (db * (31u - alpha))) / 31u) |
	  (((sg * alpha) + (dg * (31u - alpha))) / 31u) << 5u |
	  (((sr * alpha) + (dr * (31u - alpha))) / 31u) << 10u;
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

  // draw a rectangle with zero overdraw (important for op_xor and op_alpha draw ops):
  auto rect(int x, int y, int w, int h) -> void {
    hline(x, y, w);
    hline(x + 1, y + h - 1, w - 1);
    vline(x, y + 1, h - 1);
    vline(x + w - 1, y + 1, h - 2);
  }

  auto draw_glyph_8(int x, int y, int glyph) -> void {
    for (int i = 0; i < 8; i++) {
      uint8 m = 0x80u;
      for (int j = 0; j < 8; j++, m >>= 1u) {
        if (font8x8_basic[glyph][i] & m)
          pixel(x + j, y + i);
      }
    }
  }

  auto draw_glyph_16(int x, int y, int glyph) -> void {
    for (int i = 0; i < 16; i++) {
      uint8 m = 0x80u;
      for (int j = 0; j < 8; j++, m >>= 1u) {
        if (font8x16_basic[glyph][i] & m)
          pixel(x + j, y + i);
      }
    }
  }

  auto draw_text(int x, int y, const char *c) -> int {
    auto len = 0;

    while (*c != 0) {
      if ((*c < 0x20) || (*c > 0x7F)) {
        // Skip the character since it is out of ASCII range:
        c++;
        continue;
      }

      int glyph = *c - 0x20;
      (this->*draw_glyph)(x, y, glyph);

      len++;
      c++;
      x += 8;
    }

    // return how many characters drawn:
    return len;
  }

  // draw a line of text (currently ASCII only due to font restrictions)
  auto text(int x, int y, const string *msg) -> int {
    const char *c = msg->data();

    // draw a shadow underneath text first:
    if (text_shadow) {
      auto save_alpha = alpha;
      auto save_color = color;
      alpha = uclamp<5>(save_alpha + 4);
      color = 0x0000;
      draw_text(x + 1, y + 1, c);
      alpha = save_alpha;
      color = save_color;
    }

    // return how many characters drawn:
    return draw_text(x, y, c);
  }
} scriptFrame;

auto Interface::registerScriptDefs() -> void {
  int r;

  script.engine = platform->scriptEngine();

  // register global functions for the script to use:
  auto defaultNamespace = script.engine->GetDefaultNamespace();

  // register string type:
  registerScriptString();

  // global function to write debug messages:
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

  // set font_height to use for text (8 or 16):
  r = script.engine->RegisterObjectMethod("Frame", "int get_font_height()", asMETHODPR(ScriptFrame, get_font_height, (), int), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void set_font_height(int font_height)", asMETHODPR(ScriptFrame, set_font_height, (int), void), asCALL_THISCALL); assert(r >= 0);

  // set text_shadow to draw shadow behind text:
  r = script.engine->RegisterObjectMethod("Frame", "bool get_text_shadow()", asMETHODPR(ScriptFrame, get_text_shadow, (), bool), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void set_text_shadow(bool font_height)", asMETHODPR(ScriptFrame, set_text_shadow, (bool), void), asCALL_THISCALL); assert(r >= 0);

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

  // text drawing function:
  r = script.engine->RegisterObjectMethod("Frame", "int text(int x, int y, const string &in text)", asMETHODPR(ScriptFrame, text, (int, int, const string *), int), asCALL_THISCALL); assert(r >= 0);

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
