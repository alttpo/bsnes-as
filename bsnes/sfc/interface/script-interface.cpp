
#include <sfc/sfc.hpp>

#include "vga-charset.cpp"
#include "script-string.cpp"

static auto script_bus_read_u8(uint32 addr) -> uint8 {
  return bus.read(addr, 0);
}

static auto script_bus_write_u8(uint32 addr, uint8 data) -> void {
  bus.write(addr, data);
}

static auto script_bus_read_u16(uint32 addr0, uint32 addr1) -> uint16 {
  return bus.read(addr0, 0) | (bus.read(addr1,0) << 8u);
}

static auto script_message(const string *msg) -> void {
  platform->scriptMessage(msg);
}

static auto script_rgb(uint8 r, uint8 g, uint8 b) -> uint16 {
  return ((uint16)(uclamp<5>(r)) << 10u) | ((uint16)(uclamp<5>(g)) << 5u) | (uint16)(uclamp<5>(b));
}

struct ScriptFrame {
  uint16 *&output = ppuFrame.output;
  uint &pitch = ppuFrame.pitch;
  uint &width = ppuFrame.width;
  uint &height = ppuFrame.height;

  int x_scale = 2;
  auto get_x_scale() -> int { return x_scale; }
  auto set_x_scale(int x_scale_p) -> void { x_scale = max(x_scale_p, 1); }

  int y_scale = 2;
  auto get_y_scale() -> int { return y_scale; }
  auto set_y_scale(int y_scale_p) -> void { y_scale = max(y_scale_p, 1); }

  auto get_width() -> uint { return width; }
  auto get_height() -> uint { return height; }

  int y_offset = 16;

  enum draw_op_t : int {
    op_solid,
    op_alpha,
    op_xor,
  } draw_op = op_solid;

  uint16 color = 0x7fff;
  auto get_color() -> uint16 { return color; }
  auto set_color(uint16 color_p) -> void { color = uclamp<15>(color_p); }

  uint8 alpha = 31;
  auto get_alpha() -> uint8 { return alpha; }
  auto set_alpha(uint8 alpha_p) { alpha = uclamp<5>(alpha_p); }

  bool text_shadow = false;

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

  auto inline draw(uint16 *p) {
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

  auto read_pixel(int x, int y) -> uint16 {
    // Scale the coordinates:
    x = (x * x_scale * ppuFrame.width_mult) / 2;
    y = ((y * y_scale + y_offset) * ppuFrame.height_mult) / 2;
    if (x >= 0 && y >= 0 && x < (int) width && y < (int) height) {
      return output[y * pitch + x];
    }
    // impossible color on 15-bit RGB system:
    return 0xffff;
  }

  auto pixel(int x, int y) -> void {
    // Scale the coordinates:
    x = (x * x_scale * ppuFrame.width_mult) / 2;
    y = ((y * y_scale + y_offset) * ppuFrame.height_mult) / 2;
    auto x_size = max((x_scale * ppuFrame.width_mult) / 2, 1);
    auto y_size = max((y_scale * ppuFrame.height_mult) / 2, 1);
    for (int sy = y; sy < y + y_size; sy++) {
      for (int sx = x; sx < x + x_size; sx++) {
        if (sx >= 0 && sy >= 0 && sx < (int) width && sy < (int) height) {
          draw(&output[sy * pitch + sx]);
        }
      }
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

  // fill a rectangle with zero overdraw (important for op_xor and op_alpha draw ops):
  auto fill(int lx, int ty, int w, int h) -> void {
    for (int y = ty; y < ty+h; y++)
      for (int x = lx; x < lx+w; x++)
        pixel(x, y);
  }

  auto draw_glyph_8(int x, int y, int glyph) -> void {
    for (int i = 0; i < 8; i++) {
      uint8 m = 0x80u;
      for (int j = 0; j < 8; j++, m >>= 1u) {
        uint8_t row = font8x8_basic[glyph][i];
        uint8 row1 = i < 7 ? font8x8_basic[glyph][i+1] : 0;
        if (row & m) {
          // Draw a shadow at x+1,y+1 if there won't be a pixel set there from the font:
          if (text_shadow && ((row1 & (m>>1u)) == 0u)) {
            // quick swap to black for shadow:
            auto save_color = color;
            color = 0x0000;
            pixel(x + j + 1, y + i + 1);
            // restore previous color:
            color = save_color;
          }
          pixel(x + j, y + i);
        }
      }
    }
  }

  auto draw_glyph_16(int x, int y, int glyph) -> void {
    for (int i = 0; i < 16; i++) {
      uint8 m = 0x80u;
      for (int j = 0; j < 8; j++, m >>= 1u) {
        uint8 row = font8x16_basic[glyph][i];
        if (row & m) {
          if (text_shadow) {
            uint8 row1 = i < 15 ? font8x16_basic[glyph][i + 1] : 0;
            // Draw a shadow at x+1,y+1 if there won't be a pixel set there from the font:
            if ((row1 & (m >> 1u)) == 0u) {
              // quick swap to black for shadow:
              auto save_color = color;
              color = 0x0000;
              pixel(x + j + 1, y + i + 1);
              // restore previous color:
              color = save_color;
            }
          }
          pixel(x + j, y + i);
        }
      }
    }
  }

  // draw a line of text (currently ASCII only due to font restrictions)
  auto text(int x, int y, const string *msg) -> int {
    const char *c = msg->data();

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

  auto draw_4bpp_8x8(int x, int y, const CScriptArray *tile_data, const CScriptArray *palette_data) -> void {
    // TODO: throw exceptions to script?
    assert(tile_data != nullptr);
    assert(tile_data->GetSize() >= 8);
    assert(tile_data->GetElementTypeId() == asTYPEID_UINT32);
    assert(palette_data != nullptr);
    assert(palette_data->GetSize() >= 16);
    assert(palette_data->GetElementTypeId() == asTYPEID_UINT16);

    auto save_color = color;

    for (int py = 0; py < 8; py++) {
      auto tile_data_p = static_cast<const uint32 *>(tile_data->At(py));
      if (tile_data_p == nullptr) break;

      uint32 tile = *tile_data_p;
      for (int px = 0; px < 8; px++) {
        uint32 c = 0u, shift = 7u - px;
        c += tile >> (shift +  0u) & 1u;
        c += tile >> (shift +  7u) & 2u;
        c += tile >> (shift + 14u) & 4u;
        c += tile >> (shift + 21u) & 8u;
        if (c) {
          auto palette_p = static_cast<const uint16 *>(palette_data->At(c));
          if (palette_p == nullptr) break;

          color = *palette_p;
          pixel(x + px, y + py);
        }
      }
    }

    color = save_color;
  }

  auto vram_read(uint16 addr) -> uint16 {
    if (system.fastPPU()) {
      return ppufast.vram[addr & 0x7fff];
    }
    return ppu.vram[addr];
  }

  auto cgram_read(uint8 palette) -> uint16 {
    if (system.fastPPU()) {
      return ppufast.cgram[palette];
    }
    return ppu.screen.cgram[palette];
  }

  uint9 obj_character = 0;
  auto obj_get_character() -> uint9 { return obj_character; }
  auto obj_set_character(uint9 character) { obj_character = character; }

  auto obj_tile_read(uint8 y) -> uint32 {
    uint16 tiledataAddress;
    uint8 chr = obj_character & 0xffu;
    uint1 nameselect = obj_character >> 8u;

    if (system.fastPPU()) {
      tiledataAddress = ppufast.io.obj.tiledataAddress;
      if (nameselect) tiledataAddress += 1 + ppufast.io.obj.nameselect << 12;
    } else {
      tiledataAddress = ppu.obj.io.tiledataAddress;
      if (nameselect) tiledataAddress += 1 + ppu.obj.io.nameselect << 12;
    }

    uint16 chrx =  (chr >> 0 & 15);
    uint16 chry = ((chr >> 4 & 15) + (y >> 3) & 15) << 4;

    uint pos = tiledataAddress + ((chry + chrx) << 4);
    uint16 addr = (pos & 0xfff0) + (y & 7);

    uint32 data = (uint32)vram_read(addr + 0) << 0u | (uint32)vram_read(addr + 8) << 16u;
    return data;
  }

  struct OAMObject {
    uint9 x;
    uint8 y;
    uint8 character;
    uint1 nameselect;
    uint1 vflip;
    uint1 hflip;
    uint2 priority;
    uint3 palette;
    uint1 size;
  } oam_object;
  uint8 oam_index;

  auto oam_get_index() -> uint8 { return oam_index; }
  auto oam_set_index(uint8 index) -> void {
    oam_index = index;
    if (system.fastPPU()) {
      auto po = ppufast.objects[index];
      oam_object.x = po.x;
      oam_object.y = po.y;
      oam_object.character = po.character;
      oam_object.nameselect = po.nameselect;
      oam_object.vflip = po.vflip;
      oam_object.hflip = po.hflip;
      oam_object.priority = po.priority;
      oam_object.palette = po.palette;
      oam_object.size = po.size;
    } else {
      auto po = ppu.obj.oam.object[index];
      oam_object.x = po.x;
      oam_object.y = po.y;
      oam_object.character = po.character;
      oam_object.nameselect = po.nameselect;
      oam_object.vflip = po.vflip;
      oam_object.hflip = po.hflip;
      oam_object.priority = po.priority;
      oam_object.palette = po.palette;
      oam_object.size = po.size;
    }
  }

  auto oam_get_x() -> uint16 { return oam_object.x; }
  auto oam_get_y() -> uint8 { return oam_object.y; }
  auto oam_get_character() -> uint8 { return oam_object.character; }
  auto oam_get_nameselect() -> uint8 { return oam_object.nameselect; }
  auto oam_get_vflip() -> uint8 { return oam_object.vflip; }
  auto oam_get_hflip() -> uint8 { return oam_object.hflip; }
  auto oam_get_priority() -> uint8 { return oam_object.priority; }
  auto oam_get_palette() -> uint8 { return oam_object.palette; }
  auto oam_get_size() -> uint8 { return oam_object.size; }
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

  // default bus memory functions:
  r = script.engine->SetDefaultNamespace("bus"); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 read_u8(uint32 addr)",  asFUNCTION(script_bus_read_u8), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint16 read_u16(uint32 addr0, uint32 addr1)", asFUNCTION(script_bus_read_u16), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("void write_u8(uint32 addr, uint8 data)", asFUNCTION(script_bus_write_u8), asCALL_CDECL); assert(r >= 0);

  // create ppu namespace
  r = script.engine->SetDefaultNamespace("ppu"); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint16 rgb(uint8 r, uint8 g, uint8 b)", asFUNCTION(script_rgb), asCALL_CDECL); assert(r >= 0);

  // define ppu::VRAM object type:
  r = script.engine->RegisterObjectType("VRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("VRAM", "uint16 opIndex(uint16 addr)", asMETHOD(ScriptFrame, vram_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("VRAM vram", &scriptFrame); assert(r >= 0);

  r = script.engine->RegisterObjectType("CGRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("CGRAM", "uint16 opIndex(uint16 addr)", asMETHOD(ScriptFrame, cgram_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("CGRAM cgram", &scriptFrame); assert(r >= 0);

  r = script.engine->RegisterObjectType("OBJ", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OBJ", "uint16 get_character()", asMETHOD(ScriptFrame, obj_get_character), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OBJ", "void set_character(uint16 chr)", asMETHOD(ScriptFrame, obj_set_character), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OBJ", "uint32 opIndex(uint8 y)", asMETHOD(ScriptFrame, obj_tile_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("OBJ obj", &scriptFrame); assert(r >= 0);

  r = script.engine->RegisterObjectType("OAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8 get_index()", asMETHOD(ScriptFrame, oam_get_index), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "void set_index(uint8 index)", asMETHOD(ScriptFrame, oam_set_index), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint16 get_x()", asMETHOD(ScriptFrame, oam_get_x), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_y()", asMETHOD(ScriptFrame, oam_get_y), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_character()", asMETHOD(ScriptFrame, oam_get_character), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_nameselect()", asMETHOD(ScriptFrame, oam_get_nameselect), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_vflip()", asMETHOD(ScriptFrame, oam_get_vflip), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_hflip()", asMETHOD(ScriptFrame, oam_get_hflip), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_priority()", asMETHOD(ScriptFrame, oam_get_priority), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_palette()", asMETHOD(ScriptFrame, oam_get_palette), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_size()", asMETHOD(ScriptFrame, oam_get_size), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("OAM oam", &scriptFrame); assert(r >= 0);

  // define ppu::Frame object type:
  r = script.engine->RegisterObjectType("Frame", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);

  // define width and height properties:
  r = script.engine->RegisterObjectMethod("Frame", "uint get_width()", asMETHOD(ScriptFrame, get_width), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "uint get_height()", asMETHOD(ScriptFrame, get_height), asCALL_THISCALL); assert(r >= 0);

  // define x_scale and y_scale properties:
  r = script.engine->RegisterObjectMethod("Frame", "int get_x_scale()", asMETHOD(ScriptFrame, get_x_scale), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void set_x_scale(int x_scale)", asMETHOD(ScriptFrame, set_x_scale), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "int get_y_scale()", asMETHOD(ScriptFrame, get_y_scale), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void set_y_scale(int y_scale)", asMETHOD(ScriptFrame, set_y_scale), asCALL_THISCALL); assert(r >= 0);

  // adjust y_offset of drawing functions (default 8):
  r = script.engine->RegisterObjectProperty("Frame", "int y_offset", asOFFSET(ScriptFrame, y_offset)); assert(r >= 0);

  // set color to use for drawing functions (15-bit RGB):
  r = script.engine->RegisterObjectMethod("Frame", "uint16 get_color()", asMETHODPR(ScriptFrame, get_color, (), uint16), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void set_color(uint16 color)", asMETHODPR(ScriptFrame, set_color, (uint16), void), asCALL_THISCALL); assert(r >= 0);

  // set alpha to use for drawing functions (0..31):
  r = script.engine->RegisterObjectMethod("Frame", "uint8 get_alpha()", asMETHODPR(ScriptFrame, get_alpha, (), uint8), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void set_alpha(uint8 alpha)", asMETHODPR(ScriptFrame, set_alpha, (uint8), void), asCALL_THISCALL); assert(r >= 0);

  // set font_height to use for text (8 or 16):
  r = script.engine->RegisterObjectProperty("Frame", "int font_height", asOFFSET(ScriptFrame, font_height)); assert(r >= 0);

  // set text_shadow to draw shadow behind text:
  r = script.engine->RegisterObjectProperty("Frame", "bool text_shadow", asOFFSET(ScriptFrame, text_shadow)); assert(r >= 0);

  // register the DrawOp enum:
  r = script.engine->RegisterEnum("draw_op"); assert(r >= 0);
  r = script.engine->RegisterEnumValue("draw_op", "op_solid", ScriptFrame::draw_op_t::op_solid); assert(r >= 0);
  r = script.engine->RegisterEnumValue("draw_op", "op_alpha", ScriptFrame::draw_op_t::op_alpha); assert(r >= 0);
  r = script.engine->RegisterEnumValue("draw_op", "op_xor", ScriptFrame::draw_op_t::op_xor); assert(r >= 0);

  // set draw_op:
  r = script.engine->RegisterObjectProperty("Frame", "draw_op draw_op", asOFFSET(ScriptFrame, draw_op)); assert(r >= 0);

  // pixel access functions:
  r = script.engine->RegisterObjectMethod("Frame", "uint16 read_pixel(int x, int y)", asMETHODPR(ScriptFrame, read_pixel, (int, int), uint16), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void pixel(int x, int y)", asMETHODPR(ScriptFrame, pixel, (int, int), void), asCALL_THISCALL); assert(r >= 0);

  // primitive drawing functions:
  r = script.engine->RegisterObjectMethod("Frame", "void hline(int lx, int ty, int w)", asMETHODPR(ScriptFrame, hline, (int, int, int), void), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void vline(int lx, int ty, int h)", asMETHODPR(ScriptFrame, vline, (int, int, int), void), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void rect(int x, int y, int w, int h)", asMETHODPR(ScriptFrame, rect, (int, int, int, int), void), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("Frame", "void fill(int x, int y, int w, int h)", asMETHODPR(ScriptFrame, fill, (int, int, int, int), void), asCALL_THISCALL); assert(r >= 0);

  // text drawing function:
  r = script.engine->RegisterObjectMethod("Frame", "int text(int x, int y, const string &in text)", asMETHODPR(ScriptFrame, text, (int, int, const string *), int), asCALL_THISCALL); assert(r >= 0);

  // draw 4bpp paletted 8x1 row:
  r = script.engine->RegisterObjectMethod("Frame", "int draw_4bpp_8x8(int x, int y, const array<uint32> &in tiledata, const array<uint16> &in palette)", asMETHOD(ScriptFrame, draw_4bpp_8x8), asCALL_THISCALL); assert(r >= 0);

  // global property to access current frame:
  r = script.engine->RegisterGlobalProperty("Frame frame", &scriptFrame); assert(r >= 0);
  r = script.engine->SetDefaultNamespace(defaultNamespace); assert(r >= 0);

  // create context:
  script.context = script.engine->CreateContext();
}

auto Interface::loadScript(string location) -> void {
  int r;

  if (!inode::exists(location)) return;

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
  script.funcs.pre_frame = script.module->GetFunctionByDecl("void pre_frame()");
  script.funcs.post_frame = script.module->GetFunctionByDecl("void post_frame()");

  if (script.funcs.init) {
    script.context->Prepare(script.funcs.init);
    script.context->Execute();
  }
}

auto Interface::unloadScript() -> void {
  if (script.module) {
    script.module->Discard();
    script.module = nullptr;
  }

  script.funcs.init = nullptr;
  script.funcs.pre_frame = nullptr;
  script.funcs.post_frame = nullptr;
}
