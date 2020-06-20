
struct ExtraLayer;

struct ExtraLayer {
  // drawing state:
  uint16 color = 0x7fffu;
  auto get_color() -> uint16 { return color; }
  auto set_color(uint16 color_p) -> void { color = uclamp<15>(color_p); }

  uint16 outline_color = 0x0000u;
  auto get_outline_color() -> uint16 { return outline_color; }
  auto set_outline_color(uint16 outline_color_p) -> void { outline_color = uclamp<15>(outline_color_p); }

  // set default font:
  PixelFonts::Font *font = PixelFonts::fonts[0];
  auto get_font() -> PixelFonts::Font * {
    return font;
  }
  auto set_font(PixelFonts::Font *font_p) -> void {
    font = font_p;
  }

  bool text_shadow = false;
  auto get_text_shadow() -> bool { return text_shadow; }
  auto set_text_shadow(bool text_shadow_p) -> void { text_shadow = text_shadow_p; }

  bool text_outline = false;
  auto get_text_outline() -> bool { return text_outline; }
  auto set_text_outline(bool text_outline_p) -> void { text_outline = text_outline_p; }

public:
  // tile management:
  auto get_tile_count() -> uint { return ppufast.extraTileCount; }
  auto set_tile_count(uint count) { ppufast.extraTileCount = min(count, 128); }

  // reset all tiles to blank state:
  auto reset() -> void {
	  ppufast.extraTileCount = 0;
	  for (int i = 0; i < 128; i++) {
		  tile_reset(&ppufast.extraTiles[i]);
	  }
  }

  static auto get_tile(ExtraLayer *dummy, uint i) -> PPUfast::ExtraTile* {
	  (void)dummy;
	  if (i >= 128) {
		  asGetActiveContext()->SetException("Cannot access extra[i] where i >= 128", true);
		  return nullptr;
	  }
	  return &ppufast.extraTiles[min(i,128-1)];
  }

public:
  // tile methods:
  static auto tile_reset(PPUfast::ExtraTile *t) -> void {
	  t->x = 0;
	  t->y = 0;
	  t->source = 0;
	  t->hflip = false;
	  t->vflip = false;
	  t->priority = 0;
	  t->width = 0;
	  t->height = 0;
	  tile_pixels_clear(t);
  }

  static auto tile_pixels_clear(PPUfast::ExtraTile *t) -> void {
	  memory::fill<uint16>(t->colors, PPUfast::extra_max_colors, 0);
  }

  static auto tile_pixel(PPUfast::ExtraTile *t, int x, int y) -> void;

  static auto tile_pixel_set(PPUfast::ExtraTile *t, int x, int y, uint16 color) -> void {
	  // bounds check:
	  if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;

	  // make sure we're not writing past the end of the colors[] array:
	  uint index = y * t->width + x;
	  if (index >= PPUfast::extra_max_colors) return;

	  // write the pixel with opaque bit set:
	  t->colors[index] = color | 0x8000u;
  }

  static auto tile_pixel_off(PPUfast::ExtraTile *t, int x, int y) -> void {
	  // bounds check:
	  if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;

	  // make sure we're not writing past the end of the colors[] array:
	  uint index = y * t->width + x;
	  if (index >= PPUfast::extra_max_colors) return;

	  // turn off opaque bit:
	  t->colors[index] &= 0x7fffu;
  }

  static auto tile_pixel_on(PPUfast::ExtraTile *t, int x, int y) -> void {
	  // bounds check:
	  if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;

	  // make sure we're not writing past the end of the colors[] array:
	  uint index = y * t->width + x;
	  if (index >= PPUfast::extra_max_colors) return;

	  // turn on opaque bit:
	  t->colors[index] |= 0x8000u;
  }

  // draws a single 8x8 4bpp sprite at (x,y) in the tile:
  static auto tile_draw_sprite_4bpp(PPUfast::ExtraTile *t, int x, int y, int palette, const CScriptArray *tile_data, const CScriptArray *palettes) -> void {
	  // Check validity of array inputs:
	  if (tile_data == nullptr) {
		  asGetActiveContext()->SetException("tile_data array cannot be null", true);
		  return;
	  }
	  if (tile_data->GetElementTypeId() != asTYPEID_UINT16) {
		  asGetActiveContext()->SetException("tile_data array must be uint16[]", true);
		  return;
	  }
	  if (tile_data->GetSize() != 16) {
		  asGetActiveContext()->SetException("tile_data array must have exactly 16 elements", true);
		  return;
	  }

	  if (palettes == nullptr) {
		  asGetActiveContext()->SetException("palettes array cannot be null", true);
		  return;
	  }
	  if (palettes->GetElementTypeId() != asTYPEID_UINT16) {
		  asGetActiveContext()->SetException("palettes array must be uint16[]", true);
		  return;
	  }
	  if (palettes->GetSize() < ((palette << 4) + 16)) {
		  asGetActiveContext()->SetException("palettes array must have enough elements to cover 16 colors at the given palette index", true);
		  return;
	  }

    auto palette_p = static_cast<const b5g5r5 *>(palettes->At(palette << 4));
    if (palette_p == nullptr) {
      asGetActiveContext()->SetException("failed to index into palettes array using palette index", true);
      return;
    }

    auto tile_data_p = static_cast<const uint16 *>(tile_data->At(0));
    if (tile_data_p == nullptr) {
      asGetActiveContext()->SetException("failed to index into tile_data array at element 0", true);
      return;
    }

    for (int py = 0; py < 8; py++) {
      uint32 tile;
      tile  = tile_data_p[py + 0] <<  0;
      tile |= tile_data_p[py + 8] << 16;

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

  // draw a line of text (currently ASCII only due to font restrictions)
  static auto tile_text(PPUfast::ExtraTile *t, int x, int y, const string *msg) -> int;
} extraLayer;

auto ExtraLayer::tile_pixel(PPUfast::ExtraTile *t, int x, int y) -> void {
	// bounds check:
	if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;

	// make sure we're not writing past the end of the colors[] array:
	uint index = y * t->width + x;
	if (index >= PPUfast::extra_max_colors) return;

	// write the pixel with opaque bit set:
	t->colors[index] = extraLayer.color | 0x8000u;
}

// draw a line of text (currently ASCII only due to font restrictions)
auto ExtraLayer::tile_text(PPUfast::ExtraTile *t, int x, int y, const string *msg) -> int {
  const char *c = msg->data();

  auto totalWidth = 0;

  while (*c != 0) {
    uint32_t glyph = *c;

    // if outline enabled, draw a black square box around each pixel in the font before stroking the glyph:
    if (extraLayer.text_outline) {
      extraLayer.font->drawGlyph(glyph, [=](int xo, int yo) {
        for (int ny = -1; ny <= 1; ny++) {
          for (int nx = -1; nx <= 1; nx++) {
            if (ny == 0 && nx == 0) continue;
            extraLayer.tile_pixel_set(t, x + xo + nx, y + yo + ny, extraLayer.outline_color);
          }
        }
      });
    } else if (extraLayer.text_shadow) {
      // if shadow enabled, draw a black shadow right+down one pixel before stroking the glyph:
      extraLayer.font->drawGlyph(glyph, [=](int xo, int yo) {
        extraLayer.tile_pixel_set(t, x + xo + 1, y + yo + 1, extraLayer.outline_color);
      });
    }

    // stroke the font with the current color:
    int width = extraLayer.font->drawGlyph(glyph, [=](int xo, int yo) {
      extraLayer.tile_pixel(t, x + xo, y + yo);
    });

    x += width;
    totalWidth += width;

    c++;
  }

  // return how many pixels drawn horizontally:
  return totalWidth;
}

auto RegisterPPUExtra(asIScriptEngine *e) -> void {
  int r;

  // assumes current default namespace is 'ppu'

  // extra-tiles access:
  r = e->RegisterObjectType("Extra", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);

  r = e->RegisterObjectType("ExtraTile", sizeof(PPUfast::ExtraTile), asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);

  r = e->RegisterObjectProperty("ExtraTile", "int x", asOFFSET(PPUfast::ExtraTile, x)); assert(r >= 0);
  r = e->RegisterObjectProperty("ExtraTile", "int y", asOFFSET(PPUfast::ExtraTile, y)); assert(r >= 0);
  r = e->RegisterObjectProperty("ExtraTile", "uint source", asOFFSET(PPUfast::ExtraTile, source)); assert(r >= 0);
  r = e->RegisterObjectProperty("ExtraTile", "bool hflip", asOFFSET(PPUfast::ExtraTile, hflip)); assert(r >= 0);
  r = e->RegisterObjectProperty("ExtraTile", "bool vflip", asOFFSET(PPUfast::ExtraTile, vflip)); assert(r >= 0);
  r = e->RegisterObjectProperty("ExtraTile", "uint priority", asOFFSET(PPUfast::ExtraTile, priority)); assert(r >= 0);
  r = e->RegisterObjectProperty("ExtraTile", "uint width", asOFFSET(PPUfast::ExtraTile, width)); assert(r >= 0);
  r = e->RegisterObjectProperty("ExtraTile", "uint height", asOFFSET(PPUfast::ExtraTile, height)); assert(r >= 0);
  r = e->RegisterObjectProperty("ExtraTile", "uint index", asOFFSET(PPUfast::ExtraTile, index)); assert(r >= 0);

  r = e->RegisterObjectMethod("ExtraTile", "void reset()", asFUNCTION(ExtraLayer::tile_reset), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("ExtraTile", "void pixels_clear()", asFUNCTION(ExtraLayer::tile_pixels_clear), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  r = e->RegisterObjectMethod("ExtraTile", "void pixel_set(int x, int y, uint16 color)", asFUNCTION(ExtraLayer::tile_pixel_set), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("ExtraTile", "void pixel_off(int x, int y)", asFUNCTION(ExtraLayer::tile_pixel_off), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("ExtraTile", "void pixel_on(int x, int y)", asFUNCTION(ExtraLayer::tile_pixel_on), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("ExtraTile", "void pixel(int x, int y)", asFUNCTION(ExtraLayer::tile_pixel_set), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("ExtraTile", "void draw_sprite_4bpp(int x, int y, int palette, const array<uint16> &in tiledata, const array<uint16> &in palettes)", asFUNCTION(ExtraLayer::tile_draw_sprite_4bpp), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  // primitive drawing functions:
  r = e->RegisterObjectMethod("ExtraTile", "void hline(int lx, int ty, int w)", asFUNCTION(ExtraLayer::tile_hline), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("ExtraTile", "void vline(int lx, int ty, int h)", asFUNCTION(ExtraLayer::tile_vline), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("ExtraTile", "void rect(int x, int y, int w, int h)", asFUNCTION(ExtraLayer::tile_rect), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("ExtraTile", "void fill(int x, int y, int w, int h)", asFUNCTION(ExtraLayer::tile_fill), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  // text drawing function:
  r = e->RegisterObjectMethod("ExtraTile", "int text(int x, int y, const string &in text)", asFUNCTION(ExtraLayer::tile_text), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  r = e->RegisterObjectMethod("Extra", "uint16 get_color() property", asMETHOD(ExtraLayer, get_color), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Extra", "void set_color(uint16 color) property", asMETHOD(ExtraLayer, set_color), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Extra", "uint16 get_outline_color() property", asMETHOD(ExtraLayer, get_outline_color), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Extra", "void set_outline_color(uint16 color) property", asMETHOD(ExtraLayer, set_outline_color), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Extra", "Font &get_font() property", asMETHOD(ExtraLayer, get_font), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Extra", "void set_font(const Font &in font) property", asMETHOD(ExtraLayer, set_font), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Extra", "bool get_text_shadow() property", asMETHOD(ExtraLayer, get_text_shadow), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Extra", "void set_text_shadow(bool color) property", asMETHOD(ExtraLayer, set_text_shadow), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Extra", "bool get_text_outline() property", asMETHOD(ExtraLayer, get_text_outline), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Extra", "void set_text_outline(bool color) property", asMETHOD(ExtraLayer, set_text_outline), asCALL_THISCALL); assert(r >= 0);

  r = e->RegisterObjectMethod("Extra", "void reset()", asMETHOD(ExtraLayer, reset), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Extra", "uint get_count() property", asMETHOD(ExtraLayer, get_tile_count), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Extra", "void set_count(uint count) property", asMETHOD(ExtraLayer, set_tile_count), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Extra", "ExtraTile @get_opIndex(uint i) property", asFUNCTION(ExtraLayer::get_tile), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterGlobalProperty("Extra extra", &extraLayer); assert(r >= 0);
}
