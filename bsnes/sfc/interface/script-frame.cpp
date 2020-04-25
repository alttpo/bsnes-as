
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

  auto (PostFrame::*draw_glyph)(int x, int y, int glyph) -> void = &PostFrame::draw_glyph_8;

  int font_height = 8;
  auto get_font_height() -> int { return font_height; }
  auto set_font_height(int font_height_p) {
    if (font_height_p <= 8) {
      font_height = 8;
      draw_glyph = &PostFrame::draw_glyph_8;
    } else {
      font_height = 16;
      draw_glyph = &PostFrame::draw_glyph_16;
    }
  }

  auto inline draw(r5g5b5 *p) {
    auto lumaLookup = ppuAccess.lightTable_lookup(/*io.displayBrightness*/ luma);
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

auto RegisterPPUFrame(asIScriptEngine *e) -> void {
  int r;

  // assumes current default namespace is 'ppu'

  // define ppu::Frame object type:
  r = e->RegisterObjectType("Frame", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);

  // define width and height properties:
  r = e->RegisterObjectMethod("Frame", "uint get_width() property", asMETHOD(PostFrame, get_width), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Frame", "uint get_height() property", asMETHOD(PostFrame, get_height), asCALL_THISCALL); assert(r >= 0);

  // define x_scale and y_scale properties:
  r = e->RegisterObjectMethod("Frame", "int get_x_scale() property", asMETHOD(PostFrame, get_x_scale), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Frame", "void set_x_scale(int x_scale) property", asMETHOD(PostFrame, set_x_scale), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Frame", "int get_y_scale() property", asMETHOD(PostFrame, get_y_scale), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Frame", "void set_y_scale(int y_scale) property", asMETHOD(PostFrame, set_y_scale), asCALL_THISCALL); assert(r >= 0);

  // adjust y_offset of drawing functions (default 8):
  r = e->RegisterObjectProperty("Frame", "int y_offset", asOFFSET(PostFrame, y_offset)); assert(r >= 0);

  // set color to use for drawing functions (15-bit RGB):
  r = e->RegisterObjectMethod("Frame", "uint16 get_color() property", asMETHOD(PostFrame, get_color), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Frame", "void set_color(uint16 color) property", asMETHOD(PostFrame, set_color), asCALL_THISCALL); assert(r >= 0);

  // set luma (paired with color) to use for drawing functions (0..15):
  r = e->RegisterObjectMethod("Frame", "uint8 get_luma() property", asMETHOD(PostFrame, get_luma), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Frame", "void set_luma(uint8 luma) property", asMETHOD(PostFrame, set_luma), asCALL_THISCALL); assert(r >= 0);

  // set alpha to use for drawing functions (0..31):
  r = e->RegisterObjectMethod("Frame", "uint8 get_alpha() property", asMETHOD(PostFrame, get_alpha), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Frame", "void set_alpha(uint8 alpha) property", asMETHOD(PostFrame, set_alpha), asCALL_THISCALL); assert(r >= 0);

  // set font_height to use for text (8 or 16):
  r = e->RegisterObjectProperty("Frame", "int font_height", asOFFSET(PostFrame, font_height)); assert(r >= 0);

  // set text_shadow to draw shadow behind text:
  r = e->RegisterObjectProperty("Frame", "bool text_shadow", asOFFSET(PostFrame, text_shadow)); assert(r >= 0);

  // register the DrawOp enum:
  r = e->RegisterEnum("draw_op"); assert(r >= 0);
  r = e->RegisterEnumValue("draw_op", "op_solid", PostFrame::draw_op_t::op_solid); assert(r >= 0);
  r = e->RegisterEnumValue("draw_op", "op_alpha", PostFrame::draw_op_t::op_alpha); assert(r >= 0);
  r = e->RegisterEnumValue("draw_op", "op_xor", PostFrame::draw_op_t::op_xor); assert(r >= 0);

  // set draw_op:
  r = e->RegisterObjectProperty("Frame", "draw_op draw_op", asOFFSET(PostFrame, draw_op)); assert(r >= 0);

  // pixel access functions:
  r = e->RegisterObjectMethod("Frame", "uint16 read_pixel(int x, int y)", asMETHOD(PostFrame, read_pixel), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Frame", "void pixel(int x, int y)", asMETHOD(PostFrame, pixel), asCALL_THISCALL); assert(r >= 0);

  // primitive drawing functions:
  r = e->RegisterObjectMethod("Frame", "void hline(int lx, int ty, int w)", asMETHOD(PostFrame, hline), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Frame", "void vline(int lx, int ty, int h)", asMETHOD(PostFrame, vline), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Frame", "void rect(int x, int y, int w, int h)", asMETHOD(PostFrame, rect), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Frame", "void fill(int x, int y, int w, int h)", asMETHOD(PostFrame, fill), asCALL_THISCALL); assert(r >= 0);

  // text drawing function:
  r = e->RegisterObjectMethod("Frame", "int text(int x, int y, const string &in text)", asMETHOD(PostFrame, text), asCALL_THISCALL); assert(r >= 0);

  // draw 4bpp paletted 8x1 row:
  r = e->RegisterObjectMethod("Frame", "int draw_4bpp_8x8(int x, int y, const array<uint32> &in tiledata, const array<uint16> &in palette)", asMETHOD(PostFrame, draw_4bpp_8x8), asCALL_THISCALL); assert(r >= 0);

  // global property to access current frame:
  r = e->RegisterGlobalProperty("Frame frame", &postFrame); assert(r >= 0);
}
