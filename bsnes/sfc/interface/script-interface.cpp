
#include <sfc/sfc.hpp>

#include "vga-charset.cpp"
#include "script-string.cpp"

// R5G5B5 is what ends up on the final PPU frame buffer (R and B are swapped from SNES)
typedef uint16 r5g5b5;
// B5G5R5 is used by the SNES system
typedef uint16 b5g5r5;

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

struct ScriptInterface;
extern ScriptInterface scriptInterface;

struct ScriptInterface {
  struct PPUAccess {
    // Global functions related to PPU:
    static auto ppu_rgb(uint8 r, uint8 g, uint8 b) -> b5g5r5 {
      return ((b5g5r5) (uclamp<5>(b)) << 10u) | ((b5g5r5) (uclamp<5>(g)) << 5u) | (b5g5r5) (uclamp<5>(r));
    }

    static auto ppu_get_luma() -> uint8 {
      if (system.fastPPU()) {
        return ppufast.io.displayBrightness;
      }
      return ppu.io.displayBrightness;
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
  } ppuAccess;

  struct PostFrame {
    r5g5b5 *&output = ppuFrame.output;
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

    b5g5r5 color = 0x7fff;
    auto get_color() -> b5g5r5 { return color; }
    auto set_color(b5g5r5 color_p) -> void { color = uclamp<15>(color_p); }

    uint8 luma = 15;
    auto get_luma() -> uint8 { return luma; }
    auto set_luma(uint8 luma_p) { luma = uclamp<4>(luma_p); }

    uint8 alpha = 31;
    auto get_alpha() -> uint8 { return alpha; }
    auto set_alpha(uint8 alpha_p) { alpha = uclamp<5>(alpha_p); }

    bool text_shadow = false;

    auto (ScriptInterface::PostFrame::*draw_glyph)(int x, int y, int glyph) -> void = &ScriptInterface::PostFrame::draw_glyph_8;

    int font_height = 8;
    auto get_font_height() -> int { return font_height; }
    auto set_font_height(int font_height_p) {
      if (font_height_p <= 8) {
        font_height = 8;
        draw_glyph = &ScriptInterface::PostFrame::draw_glyph_8;
      } else {
        font_height = 16;
        draw_glyph = &ScriptInterface::PostFrame::draw_glyph_16;
      }
    }

    auto inline draw(r5g5b5 *p) {
      auto lumaLookup = ppu.lightTable[/*io.displayBrightness*/ luma];
      r5g5b5 real_color = lumaLookup[color];

      switch (draw_op) {
        case op_alpha: {
          uint db = (*p & 0x001fu);
          uint dg = (*p & 0x03e0u) >> 5u;
          uint dr = (*p & 0x7c00u) >> 10u;
          uint sb = (real_color & 0x001fu);
          uint sg = (real_color & 0x03e0u) >> 5u;
          uint sr = (real_color & 0x7c00u) >> 10u;

          *p =
            (((sb * alpha) + (db * (31u - alpha))) / 31u) |
            (((sg * alpha) + (dg * (31u - alpha))) / 31u) << 5u |
            (((sr * alpha) + (dr * (31u - alpha))) / 31u) << 10u;
          break;
        }

        case op_xor:
          *p ^= real_color;
          break;

        case op_solid:
        default:
          *p = real_color;
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
      // Check validity of array inputs:
      if (tile_data == nullptr) {
        asGetActiveContext()->SetException("tile_data array cannot be null", true);
        return;
      }
      if (tile_data->GetElementTypeId() != asTYPEID_UINT32) {
        asGetActiveContext()->SetException("tile_data array must be uint32[]", true);
        return;
      }
      if (tile_data->GetSize() < 8) {
        asGetActiveContext()->SetException("tile_data array must have at least 8 elements", true);
        return;
      }

      if (palette_data == nullptr) {
        asGetActiveContext()->SetException("palette_data array cannot be null", true);
        return;
      }
      if (palette_data->GetElementTypeId() != asTYPEID_UINT16) {
        asGetActiveContext()->SetException("palette_data array must be uint16[]", true);
        return;
      }
      if (palette_data->GetSize() < 16) {
        asGetActiveContext()->SetException("palette_data array must have at least 8 elements", true);
        return;
      }

      auto save_color = color;
      auto tile_data_p = static_cast<const uint32 *>(tile_data->At(0));
      if (tile_data_p == nullptr) {
        asGetActiveContext()->SetException("tile_data array value pointer must not be null", true);
        return;
      }
      auto palette_p = static_cast<const b5g5r5 *>(palette_data->At(0));
      if (palette_p == nullptr) {
        asGetActiveContext()->SetException("palette_data array value pointer must not be null", true);
        return;
      }

      for (int py = 0; py < 8; py++) {
        uint32 tile = tile_data_p[py];
        for (int px = 0; px < 8; px++) {
          uint32 c = 0u, shift = 7u - px;
          c += tile >> (shift +  0u) & 1u;
          c += tile >> (shift +  7u) & 2u;
          c += tile >> (shift + 14u) & 4u;
          c += tile >> (shift + 21u) & 8u;
          if (c) {
            color = palette_p[c];
            pixel(x + px, y + py);
          }
        }
      }

      color = save_color;
    }
  } postFrame;

  // script interface for rendering extra tiles:
  struct ExtraLayer {
    // drawing state:
    uint16 color = 0x7fffu;
    auto get_color() -> uint16 { return color; }
    auto set_color(uint16 color_p) -> void { color = uclamp<15>(color_p); }

    bool text_shadow = false;
    auto get_text_shadow() -> bool { return text_shadow; }
    auto set_text_shadow(bool text_shadow_p) -> void { text_shadow = text_shadow_p; }

    auto measure_text(const string *msg) -> int {
      const char *c = msg->data();

      auto len = 0;

      while (*c != 0) {
        if ((*c < 0x20) || (*c > 0x7F)) {
          // Skip the character since it is out of ASCII range:
          c++;
          continue;
        }

        len++;
        c++;
      }

      return len;
    }

  public:
    // tile management:
    auto get_tile_count() -> uint { return ppufast.extraTileCount; }
    auto set_tile_count(uint count) { ppufast.extraTileCount = min(count, 256); }

    // reset all tiles to blank state:
    auto reset() -> void {
      ppufast.extraTileCount = 0;
      for (int i = 0; i < 256; i++) {
        tile_reset(&ppufast.extraTiles[i]);
      }
    }

    static auto get_tile(ExtraLayer *dummy, uint i) -> PPUfast::ExtraTile* {
      (void)dummy;
      return &ppufast.extraTiles[i];
    }

  public:
    // tile methods:
    static auto tile_reset(PPUfast::ExtraTile *t) -> void {
      t->x = 0;
      t->y = 0;
      t->source = 0;
      t->aboveEnable = false;
      t->belowEnable = false;
      t->priority = 0;
      t->width = 0;
      t->height = 0;
      tile_pixels_clear(t);
    }

    static auto tile_pixels_clear(PPUfast::ExtraTile *t) -> void {
      memory::fill<uint16>(t->colors, 1024, 0);
    }

    static auto tile_pixel(PPUfast::ExtraTile *t, int x, int y) -> void {
      // bounds check:
      if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;

      // make sure we're not writing past the end of the colors[] array:
      uint index = y * t->width + x;
      if (index >= 1024u) return;

      // write the pixel with opaque bit set:
      t->colors[index] = scriptInterface.extraLayer.color | 0x8000u;
    }

    static auto tile_pixel_set(PPUfast::ExtraTile *t, int x, int y, uint16 color) -> void {
      // bounds check:
      if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;

      // make sure we're not writing past the end of the colors[] array:
      uint index = y * t->width + x;
      if (index >= 1024u) return;

      // write the pixel with opaque bit set:
      t->colors[index] = color | 0x8000u;
    }

    static auto tile_pixel_off(PPUfast::ExtraTile *t, int x, int y) -> void {
      // bounds check:
      if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;

      // make sure we're not writing past the end of the colors[] array:
      uint index = y * t->width + x;
      if (index >= 1024u) return;

      // turn off opaque bit:
      t->colors[index] &= 0x7fffu;
    }

    static auto tile_pixel_on(PPUfast::ExtraTile *t, int x, int y) -> void {
      // bounds check:
      if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;

      // make sure we're not writing past the end of the colors[] array:
      uint index = y * t->width + x;
      if (index >= 1024u) return;

      // turn on opaque bit:
      t->colors[index] |= 0x8000u;
    }

    static auto tile_draw_sprite(PPUfast::ExtraTile *t, int x, int y, const CScriptArray *tile_data, const CScriptArray *palette_data) -> void {
      // Check validity of array inputs:
      if (tile_data == nullptr) {
        asGetActiveContext()->SetException("tile_data array cannot be null", true);
        return;
      }
      if (tile_data->GetElementTypeId() != asTYPEID_UINT32) {
        asGetActiveContext()->SetException("tile_data array must be uint32[]", true);
        return;
      }
      if (tile_data->GetSize() < 8) {
        asGetActiveContext()->SetException("tile_data array must have at least 8 elements", true);
        return;
      }

      if (palette_data == nullptr) {
        asGetActiveContext()->SetException("palette_data array cannot be null", true);
        return;
      }
      if (palette_data->GetElementTypeId() != asTYPEID_UINT16) {
        asGetActiveContext()->SetException("palette_data array must be uint16[]", true);
        return;
      }
      if (palette_data->GetSize() < 16) {
        asGetActiveContext()->SetException("palette_data array must have at least 8 elements", true);
        return;
      }

      auto tile_data_p = static_cast<const uint32 *>(tile_data->At(0));
      if (tile_data_p == nullptr) {
        asGetActiveContext()->SetException("tile_data array value pointer must not be null", true);
        return;
      }
      auto palette_p = static_cast<const b5g5r5 *>(palette_data->At(0));
      if (palette_p == nullptr) {
        asGetActiveContext()->SetException("palette_data array value pointer must not be null", true);
        return;
      }

      for (int py = 0; py < 8; py++) {
        uint32 tile = tile_data_p[py];
        for (int px = 0; px < 8; px++) {
          uint32 c = 0u, shift = 7u - px;
          c += tile >> (shift + 0u) & 1u;
          c += tile >> (shift + 7u) & 2u;
          c += tile >> (shift + 14u) & 4u;
          c += tile >> (shift + 21u) & 8u;
          if (c) {
            tile_pixel_set(t, x + px, y + py, palette_p[c]);
          }
        }
      }
    }

    // draw a horizontal line from x=lx to lx+w on y=ty:
    static auto tile_hline(PPUfast::ExtraTile *t, int lx, int ty, int w) -> void {
      for (int x = lx; x < lx + w; ++x) {
        tile_pixel(t, x, ty);
      }
    }

    // draw a vertical line from y=ty to ty+h on x=lx:
    static auto tile_vline(PPUfast::ExtraTile *t, int lx, int ty, int h) -> void {
      for (int y = ty; y < ty + h; ++y) {
        tile_pixel(t, lx, y);
      }
    }

    // draw a rectangle with zero overdraw (important for op_xor and op_alpha draw ops):
    static auto tile_rect(PPUfast::ExtraTile *t, int x, int y, int w, int h) -> void {
      tile_hline(t, x, y, w);
      tile_hline(t, x + 1, y + h - 1, w - 1);
      tile_vline(t, x, y + 1, h - 1);
      tile_vline(t, x + w - 1, y + 1, h - 2);
    }

    // fill a rectangle with zero overdraw (important for op_xor and op_alpha draw ops):
    static auto tile_fill(PPUfast::ExtraTile *t, int lx, int ty, int w, int h) -> void {
      for (int y = ty; y < ty+h; y++)
        for (int x = lx; x < lx+w; x++)
          tile_pixel(t, x, y);
    }

    static auto tile_glyph_8(PPUfast::ExtraTile *t, int x, int y, int glyph) -> void {
      for (int i = 0; i < 8; i++) {
        uint8 m = 0x80u;
        for (int j = 0; j < 8; j++, m >>= 1u) {
          uint8_t row = font8x8_basic[glyph][i];
          uint8 row1 = i < 7 ? font8x8_basic[glyph][i+1] : 0;
          if (row & m) {
            // Draw a shadow at x+1,y+1 if there won't be a pixel set there from the font:
            if (scriptInterface.extraLayer.text_shadow && ((row1 & (m>>1u)) == 0u)) {
              tile_pixel_set(t, x + j + 1, y + i + 1, 0x0000);
            }
            tile_pixel(t, x + j, y + i);
          }
        }
      }
    }

    // draw a line of text (currently ASCII only due to font restrictions)
    static auto tile_text(PPUfast::ExtraTile *t, int x, int y, const string *msg) -> int {
      const char *c = msg->data();

      auto len = 0;

      while (*c != 0) {
        if ((*c < 0x20) || (*c > 0x7F)) {
          // Skip the character since it is out of ASCII range:
          c++;
          continue;
        }

        int glyph = *c - 0x20;
        tile_glyph_8(t, x, y, glyph);

        len++;
        c++;
        x += 8;
      }

      // return how many characters drawn:
      return len;
    }
  } extraLayer;
};

ScriptInterface scriptInterface;

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
  r = script.engine->RegisterGlobalFunction("uint16 rgb(uint8 r, uint8 g, uint8 b)", asFUNCTION(ScriptInterface::PPUAccess::ppu_rgb), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 get_luma()", asFUNCTION(ScriptInterface::PPUAccess::ppu_get_luma), asCALL_CDECL); assert(r >= 0);

  // define ppu::VRAM object type:
  r = script.engine->RegisterObjectType("VRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("VRAM", "uint16 opIndex(uint16 addr)", asMETHOD(ScriptInterface::PPUAccess, vram_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("VRAM vram", &scriptInterface.ppuAccess); assert(r >= 0);

  r = script.engine->RegisterObjectType("CGRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("CGRAM", "uint16 opIndex(uint16 addr)", asMETHOD(ScriptInterface::PPUAccess, cgram_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("CGRAM cgram", &scriptInterface.ppuAccess); assert(r >= 0);

  r = script.engine->RegisterObjectType("OBJ", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OBJ", "uint16 get_character()", asMETHOD(ScriptInterface::PPUAccess, obj_get_character), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OBJ", "void set_character(uint16 chr)", asMETHOD(ScriptInterface::PPUAccess, obj_set_character), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OBJ", "uint32 opIndex(uint8 y)", asMETHOD(ScriptInterface::PPUAccess, obj_tile_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("OBJ obj", &scriptInterface.ppuAccess); assert(r >= 0);

  r = script.engine->RegisterObjectType("OAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8 get_index()", asMETHOD(ScriptInterface::PPUAccess, oam_get_index), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "void set_index(uint8 index)", asMETHOD(ScriptInterface::PPUAccess, oam_set_index), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint16 get_x()", asMETHOD(ScriptInterface::PPUAccess, oam_get_x), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_y()", asMETHOD(ScriptInterface::PPUAccess, oam_get_y), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_character()", asMETHOD(ScriptInterface::PPUAccess, oam_get_character), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_nameselect()", asMETHOD(ScriptInterface::PPUAccess, oam_get_nameselect), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_vflip()", asMETHOD(ScriptInterface::PPUAccess, oam_get_vflip), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_hflip()", asMETHOD(ScriptInterface::PPUAccess, oam_get_hflip), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_priority()", asMETHOD(ScriptInterface::PPUAccess, oam_get_priority), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_palette()", asMETHOD(ScriptInterface::PPUAccess, oam_get_palette), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "uint8  get_size()", asMETHOD(ScriptInterface::PPUAccess, oam_get_size), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("OAM oam", &scriptInterface.ppuAccess); assert(r >= 0);

  {
    // define ppu::Frame object type:
    r = script.engine->RegisterObjectType("Frame", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);

    // define width and height properties:
    r = script.engine->RegisterObjectMethod("Frame", "uint get_width()", asMETHOD(ScriptInterface::PostFrame, get_width), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "uint get_height()", asMETHOD(ScriptInterface::PostFrame, get_height), asCALL_THISCALL); assert(r >= 0);

    // define x_scale and y_scale properties:
    r = script.engine->RegisterObjectMethod("Frame", "int get_x_scale()", asMETHOD(ScriptInterface::PostFrame, get_x_scale), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void set_x_scale(int x_scale)", asMETHOD(ScriptInterface::PostFrame, set_x_scale), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "int get_y_scale()", asMETHOD(ScriptInterface::PostFrame, get_y_scale), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void set_y_scale(int y_scale)", asMETHOD(ScriptInterface::PostFrame, set_y_scale), asCALL_THISCALL); assert(r >= 0);

    // adjust y_offset of drawing functions (default 8):
    r = script.engine->RegisterObjectProperty("Frame", "int y_offset", asOFFSET(ScriptInterface::PostFrame, y_offset)); assert(r >= 0);

    // set color to use for drawing functions (15-bit RGB):
    r = script.engine->RegisterObjectMethod("Frame", "uint16 get_color()", asMETHOD(ScriptInterface::PostFrame, get_color), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void set_color(uint16 color)", asMETHOD(ScriptInterface::PostFrame, set_color), asCALL_THISCALL); assert(r >= 0);

    // set luma (paired with color) to use for drawing functions (0..15):
    r = script.engine->RegisterObjectMethod("Frame", "uint8 get_luma()", asMETHOD(ScriptInterface::PostFrame, get_luma), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void set_luma(uint8 luma)", asMETHOD(ScriptInterface::PostFrame, set_luma), asCALL_THISCALL); assert(r >= 0);

    // set alpha to use for drawing functions (0..31):
    r = script.engine->RegisterObjectMethod("Frame", "uint8 get_alpha()", asMETHOD(ScriptInterface::PostFrame, get_alpha), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void set_alpha(uint8 alpha)", asMETHOD(ScriptInterface::PostFrame, set_alpha), asCALL_THISCALL); assert(r >= 0);

    // set font_height to use for text (8 or 16):
    r = script.engine->RegisterObjectProperty("Frame", "int font_height", asOFFSET(ScriptInterface::PostFrame, font_height)); assert(r >= 0);

    // set text_shadow to draw shadow behind text:
    r = script.engine->RegisterObjectProperty("Frame", "bool text_shadow", asOFFSET(ScriptInterface::PostFrame, text_shadow)); assert(r >= 0);

    // register the DrawOp enum:
    r = script.engine->RegisterEnum("draw_op"); assert(r >= 0);
    r = script.engine->RegisterEnumValue("draw_op", "op_solid", ScriptInterface::PostFrame::draw_op_t::op_solid); assert(r >= 0);
    r = script.engine->RegisterEnumValue("draw_op", "op_alpha", ScriptInterface::PostFrame::draw_op_t::op_alpha); assert(r >= 0);
    r = script.engine->RegisterEnumValue("draw_op", "op_xor", ScriptInterface::PostFrame::draw_op_t::op_xor); assert(r >= 0);

    // set draw_op:
    r = script.engine->RegisterObjectProperty("Frame", "draw_op draw_op", asOFFSET(ScriptInterface::PostFrame, draw_op)); assert(r >= 0);

    // pixel access functions:
    r = script.engine->RegisterObjectMethod("Frame", "uint16 read_pixel(int x, int y)", asMETHOD(ScriptInterface::PostFrame, read_pixel), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void pixel(int x, int y)", asMETHOD(ScriptInterface::PostFrame, pixel), asCALL_THISCALL); assert(r >= 0);

    // primitive drawing functions:
    r = script.engine->RegisterObjectMethod("Frame", "void hline(int lx, int ty, int w)", asMETHOD(ScriptInterface::PostFrame, hline), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void vline(int lx, int ty, int h)", asMETHOD(ScriptInterface::PostFrame, vline), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void rect(int x, int y, int w, int h)", asMETHOD(ScriptInterface::PostFrame, rect), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Frame", "void fill(int x, int y, int w, int h)", asMETHOD(ScriptInterface::PostFrame, fill), asCALL_THISCALL); assert(r >= 0);

    // text drawing function:
    r = script.engine->RegisterObjectMethod("Frame", "int text(int x, int y, const string &in text)", asMETHOD(ScriptInterface::PostFrame, text), asCALL_THISCALL); assert(r >= 0);

    // draw 4bpp paletted 8x1 row:
    r = script.engine->RegisterObjectMethod("Frame", "int draw_4bpp_8x8(int x, int y, const array<uint32> &in tiledata, const array<uint16> &in palette)", asMETHOD(ScriptInterface::PostFrame, draw_4bpp_8x8), asCALL_THISCALL); assert(r >= 0);

    // global property to access current frame:
    r = script.engine->RegisterGlobalProperty("Frame frame", &scriptInterface.postFrame); assert(r >= 0);
  }

  {
    // extra-tiles access:
    r = script.engine->RegisterObjectType("Extra", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);

    r = script.engine->RegisterObjectType("ExtraTile", sizeof(PPUfast::ExtraTile), asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

    r = script.engine->RegisterObjectProperty("ExtraTile", "uint x", asOFFSET(PPUfast::ExtraTile, x)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint y", asOFFSET(PPUfast::ExtraTile, y)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint source", asOFFSET(PPUfast::ExtraTile, source)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "bool aboveEnable", asOFFSET(PPUfast::ExtraTile, aboveEnable)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "bool belowEnable", asOFFSET(PPUfast::ExtraTile, belowEnable)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint priority", asOFFSET(PPUfast::ExtraTile, priority)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint width", asOFFSET(PPUfast::ExtraTile, width)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint height", asOFFSET(PPUfast::ExtraTile, height)); assert(r >= 0);

    r = script.engine->RegisterObjectMethod("ExtraTile", "void reset()", asFUNCTION(ScriptInterface::ExtraLayer::tile_reset), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixels_clear()", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixels_clear), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixel_set(int x, int y, uint16 color)", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixel_set), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixel_off(int x, int y)", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixel_off), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixel_on(int x, int y)", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixel_on), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixel(int x, int y)", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixel_set), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void draw_sprite(int x, int y, const array<uint32> &in tiledata, const array<uint16> &in palette)", asFUNCTION(ScriptInterface::ExtraLayer::tile_draw_sprite), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // primitive drawing functions:
    r = script.engine->RegisterObjectMethod("ExtraTile", "void hline(int lx, int ty, int w)", asFUNCTION(ScriptInterface::ExtraLayer::tile_hline), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void vline(int lx, int ty, int h)", asFUNCTION(ScriptInterface::ExtraLayer::tile_vline), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void rect(int x, int y, int w, int h)", asFUNCTION(ScriptInterface::ExtraLayer::tile_rect), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void fill(int x, int y, int w, int h)", asFUNCTION(ScriptInterface::ExtraLayer::tile_fill), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    // text drawing function:
    r = script.engine->RegisterObjectMethod("ExtraTile", "int text(int x, int y, const string &in text)", asFUNCTION(ScriptInterface::ExtraLayer::tile_text), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = script.engine->RegisterObjectMethod("Extra", "uint16 get_color()", asMETHOD(ScriptInterface::ExtraLayer, get_color), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "void set_color(uint16 color)", asMETHOD(ScriptInterface::ExtraLayer, set_color), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "bool get_text_shadow()", asMETHOD(ScriptInterface::ExtraLayer, get_text_shadow), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "void set_text_shadow(bool color)", asMETHOD(ScriptInterface::ExtraLayer, set_text_shadow), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "int measure_text(const string &in text)", asMETHOD(ScriptInterface::ExtraLayer, measure_text), asCALL_THISCALL); assert(r >= 0);

    r = script.engine->RegisterObjectMethod("Extra", "void reset()", asMETHOD(ScriptInterface::ExtraLayer, reset), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "uint get_count()", asMETHOD(ScriptInterface::ExtraLayer, get_tile_count), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "void set_count(uint count)", asMETHOD(ScriptInterface::ExtraLayer, set_tile_count), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Extra", "ExtraTile @get_opIndex(uint i)", asFUNCTION(ScriptInterface::ExtraLayer::get_tile), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterGlobalProperty("Extra extra", &scriptInterface.extraLayer); assert(r >= 0);
  }

  r = script.engine->SetDefaultNamespace(defaultNamespace); assert(r >= 0);

  // create context:
  script.context = script.engine->CreateContext();
}

auto Interface::loadScript(string location) -> void {
  int r;

  if (!inode::exists(location)) return;

  if (script.module) {
    unloadScript();
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

  // reset extra-tile data:
  scriptInterface.extraLayer.reset();
}
