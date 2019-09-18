
//#include <sfc/sfc.hpp>

#if !defined(PLATFORM_WINDOWS)
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #define SEND_BUF_CAST(t) ((const void *)(t))
  #define RECV_BUF_CAST(t) ((void *)(t))

char* sockerr() {
  return strerror(errno);
}

bool sockhaserr() {
  return errno != 0;
}
#else
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #define SEND_BUF_CAST(t) ((const char *)(t))
  #define RECV_BUF_CAST(t) ((char *)(t))

/* gcc doesn't know _Thread_local from C11 yet */
#ifdef __GNUC__
# define thread_local __thread
#elif __STDC_VERSION__ >= 201112L
# define thread_local _Thread_local
#elif defined(_MSC_VER)
# define thread_local __declspec( thread )
#else
# error Cannot define thread_local
#endif

thread_local char errmsg[256];

char* sockerr(int errcode) {
  DWORD len = FormatMessageA(FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errcode, 0, errmsg, 255, NULL);
  if (len != 0)
    errmsg[len] = 0;
  else
    sprintf(errmsg, "error %d", errcode);
  return errmsg;
}

bool sockhaserr() {
  return FAILED(WSAGetLastError());
}
#endif

#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION __FILE__ ":" S2(__LINE__)

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
  void exceptionCallback(asIScriptContext *ctx) {
    asIScriptEngine *engine = ctx->GetEngine();

    // Determine the exception that occurred
    const asIScriptFunction *function = ctx->GetExceptionFunction();
    printf(
      "EXCEPTION \"%s\" occurred in `%s` (line %d)\n",
      ctx->GetExceptionString(),
      function->GetDeclaration(),
      ctx->GetExceptionLineNumber()
    );

    // Determine the function where the exception occurred
    //printf("func: %s\n", function->GetDeclaration());
    //printf("modl: %s\n", function->GetModuleName());
    //printf("sect: %s\n", function->GetScriptSectionName());
    // Determine the line number where the exception occurred
    //printf("line: %d\n", ctx->GetExceptionLineNumber());
  }

  static void uint8_array_append_uint16(CScriptArray *array, const uint16 *value) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertLast((void *)value);
      return;
    }

    array->InsertLast((void *)&((const uint8 *)value)[0]);
    array->InsertLast((void *)&((const uint8 *)value)[1]);
  }

  static void uint8_array_append_uint32(CScriptArray *array, const uint32 *value) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertLast((void *)value);
      return;
    }

    array->InsertLast((void *)&((const uint8 *)value)[0]);
    array->InsertLast((void *)&((const uint8 *)value)[1]);
    array->InsertLast((void *)&((const uint8 *)value)[2]);
    array->InsertLast((void *)&((const uint8 *)value)[3]);
  }

  static void uint8_array_append_array(CScriptArray *array, CScriptArray *other) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertAt(array->GetSize(), *other);
      return;
    }

    if (other->GetElementTypeId() == asTYPEID_UINT8) {
      array->InsertAt(array->GetSize(), *other);
      return;
    } else if (other->GetElementTypeId() == asTYPEID_UINT16) {
      for (uint i = 0; i < other->GetSize(); i++) {
        auto value = (const uint16 *) other->At(i);
        array->InsertLast((void *) &((const uint8 *) value)[0]);
        array->InsertLast((void *) &((const uint8 *) value)[1]);
      }
    } else if (other->GetElementTypeId() == asTYPEID_UINT32) {
      for (uint i = 0; i < other->GetSize(); i++) {
        auto value = (const uint32 *) other->At(i);
        array->InsertLast((void *) &((const uint8 *) value)[0]);
        array->InsertLast((void *) &((const uint8 *) value)[1]);
        array->InsertLast((void *) &((const uint8 *) value)[2]);
        array->InsertLast((void *) &((const uint8 *) value)[3]);
      }
    }
  }

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

    static auto ppu_sprite_width(uint8 baseSize, uint8 size) -> uint8 {
      if(size == 0) {
        static const uint8 width[] = { 8,  8,  8, 16, 16, 32, 16, 16};
        return width[baseSize & 7u];
      } else {
        static const uint8 width[] = {16, 32, 64, 32, 64, 64, 32, 32};
        return width[baseSize & 7u];
      }
    }

    static auto ppu_sprite_height(uint8 baseSize, uint8 size) -> uint8 {
      if(size == 0) {
        static const uint8 height[] = { 8,  8,  8, 16, 16, 32, 32, 32};
        return height[baseSize & 7u];
      } else {
        static const uint8 height[] = {16, 32, 64, 32, 64, 64, 64, 32};
        return height[baseSize & 7u];
      }
    }

    static auto ppu_sprite_base_size() -> uint8 {
      if (system.fastPPU()) {
        return ppufast.io.obj.baseSize;
      } else {
        return ppu.obj.io.baseSize;
      }
    }

    static auto ppu_tile_address(bool nameselect) -> uint16 {
      uint16 tiledataAddress;

      if (system.fastPPU()) {
        tiledataAddress = ppufast.io.obj.tiledataAddress;
        if (nameselect) tiledataAddress += 1u + ppufast.io.obj.nameselect << 12u;
      } else {
        tiledataAddress = ppu.obj.io.tiledataAddress;
        if (nameselect) tiledataAddress += 1u + ppu.obj.io.nameselect << 12u;
      }

      return tiledataAddress;
    }

    auto cgram_read(uint8 palette) -> uint16 {
      if (system.fastPPU()) {
        return ppufast.cgram[palette];
      }
      return ppu.screen.cgram[palette];
    }

    auto vram_read(uint16 addr) -> uint16 {
      if (system.fastPPU()) {
        return ppufast.vram[addr & 0x7fff];
      }
      return ppu.vram[addr];
    }

    // Read a sprite from VRAM into `output`
    auto vram_read_sprite(uint16 tiledataAddress, uint8 chr, uint8 width, uint8 height, CScriptArray *output) -> uint {
      if (output == nullptr) {
        asGetActiveContext()->SetException("output array cannot be null", true);
        return 0;
      }
      if (output->GetElementTypeId() != asTYPEID_UINT32) {
        asGetActiveContext()->SetException("output array must be of type uint32[]", true);
        return 0;
      }
      if ((width == 0u) || (width & 7u)) {
        asGetActiveContext()->SetException("width must be a non-zero multiple of 8", true);
        return 0;
      }
      if ((height == 0u) || (height & 7u)) {
        asGetActiveContext()->SetException("height must be a non-zero multiple of 8", true);
        return 0;
      }

      // Figure out the size of the sprite to read:
      uint tileWidth = width >> 3u;

      // Make sure output array has proper capacity:
      output->Resize(tileWidth * height);

      uint out = 0;
      uint16 chrx = (chr >> 0u & 15u);
      for (uint y = 0; y < height; y++) {
        uint16 chry = ((chr >> 4u & 15u) + (y >> 3u) & 15u) << 4u;
        for (uint tx = 0; tx < tileWidth; tx++) {
          uint pos = tiledataAddress + ((chry + (chrx + tx & 15u)) << 4u);
          uint16 addr = (pos & 0xfff0u) + (y & 7u);

          uint32 data = (uint32) vram_read(addr + 0u) << 0u | (uint32) vram_read(addr + 8u) << 16u;

          // Write to output array:
          output->SetValue(out++, &data);
        }
      }

      return out;
    }

    struct OAMObject {
      uint9 x;
      uint8 y;
      uint8 character;
      bool  nameselect;
      bool  vflip;
      bool  hflip;
      uint2 priority;
      uint3 palette;
      uint1 size;

      auto get_is_enabled() -> bool {
        // easy check if below visible screen:
        if (y > 240) return false;

        // slightly more difficult check if off to left or right of visible screen:
        uint sprite_width = ppu_sprite_width(ppu_sprite_base_size(), size);

        uint sx = x + sprite_width & 511u;
        return !(x != 256 && sx >= 256 && sx + 7 < 512);
      }

      auto get_width() -> uint8 { return ppu_sprite_width(ppu_sprite_base_size(), size); }
      auto get_height() -> uint8 { return ppu_sprite_height(ppu_sprite_base_size(), size); }
    } oam_objects[128];

    static auto oam_get_object(ScriptInterface::PPUAccess *local, uint8 index) -> ScriptInterface::PPUAccess::OAMObject* {
      ScriptInterface::PPUAccess::OAMObject oam_object;
      if (system.fastPPU()) {
        auto po = ppufast.objects[index];
        local->oam_objects[index].x = po.x;
        local->oam_objects[index].y = po.y;
        local->oam_objects[index].character = po.character;
        local->oam_objects[index].nameselect = po.nameselect;
        local->oam_objects[index].vflip = po.vflip;
        local->oam_objects[index].hflip = po.hflip;
        local->oam_objects[index].priority = po.priority;
        local->oam_objects[index].palette = po.palette;
        local->oam_objects[index].size = po.size;
      } else {
        auto po = ppu.obj.oam.object[index];
        local->oam_objects[index].x = po.x;
        local->oam_objects[index].y = po.y;
        local->oam_objects[index].character = po.character;
        local->oam_objects[index].nameselect = po.nameselect;
        local->oam_objects[index].vflip = po.vflip;
        local->oam_objects[index].hflip = po.hflip;
        local->oam_objects[index].priority = po.priority;
        local->oam_objects[index].palette = po.palette;
        local->oam_objects[index].size = po.size;
      }
      return &local->oam_objects[index];
    }
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

    static auto tile_draw_sprite(PPUfast::ExtraTile *t, int x, int y, uint8 width, uint8 height, const CScriptArray *tile_data, const CScriptArray *palette_data) -> void {
      if ((width == 0u) || (width & 7u)) {
        asGetActiveContext()->SetException("width must be a non-zero multiple of 8", true);
        return;
      }
      if ((height == 0u) || (height & 7u)) {
        asGetActiveContext()->SetException("height must be a non-zero multiple of 8", true);
        return;
      }
      uint tileWidth = width >> 3u;

      // Check validity of array inputs:
      if (tile_data == nullptr) {
        asGetActiveContext()->SetException("tile_data array cannot be null", true);
        return;
      }
      if (tile_data->GetElementTypeId() != asTYPEID_UINT32) {
        asGetActiveContext()->SetException("tile_data array must be uint32[]", true);
        return;
      }
      if (tile_data->GetSize() < height * tileWidth) {
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

      for (int py = 0; py < height; py++) {
        for (int tx = 0; tx < tileWidth; tx++) {
          auto tile_data_p = static_cast<const uint32 *>(tile_data->At(py * tileWidth + tx));
          if (tile_data_p == nullptr) {
            asGetActiveContext()->SetException("tile_data array index out of range", true);
            return;
          }

          uint32 tile = *tile_data_p;
          for (int px = 0; px < 8; px++) {
            uint32 c = 0u, shift = 7u - px;
            c += tile >> (shift + 0u) & 1u;
            c += tile >> (shift + 7u) & 2u;
            c += tile >> (shift + 14u) & 4u;
            c += tile >> (shift + 21u) & 8u;
            if (c) {
              auto palette_p = static_cast<const b5g5r5 *>(palette_data->At(c));
              if (palette_p == nullptr) {
                asGetActiveContext()->SetException("palette_data array value pointer must not be null", true);
                return;
              }

              tile_pixel_set(t, x + px + (tx << 3), y + py, *palette_p);
            }
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

  struct Net {
    struct UDPSocket {
      addrinfo *serverinfo;
#if !defined(PLATFORM_WINDOWS)
      int sockfd;

      UDPSocket(int sockfd, addrinfo *serverinfo)
              : sockfd(sockfd), serverinfo(serverinfo)
      {
        ref = 1;
      }

      ~UDPSocket() {
        close(sockfd);
        freeaddrinfo(serverinfo);
      }
#else
      SOCKET sock;

      UDPSocket(SOCKET sock, addrinfo *serverinfo)
        : sock(sock), serverinfo(serverinfo)
      {
        ref = 1;
      }

      ~UDPSocket() {
        closesocket(sock);
        freeaddrinfo(serverinfo);
      }
#endif

      int ref;
      void addRef() {
        ref++;
      }
      void release() {
        if (--ref == 0)
          delete this;
      }

      int sendto(const CScriptArray *msg, const string *host, int port) {
        if (msg == nullptr) {
          asGetActiveContext()->SetException("msg cannot be null", true);
          return 0;
        }
        //assert msg->GetElementTypeId() != asTYPEID_OBJHANDLE ...

        addrinfo hints;
        const char *server;

        // TODO: wasteful to keep calling `getaddrinfo` every time and freeing memory
        memset(&hints, 0, sizeof(addrinfo));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        if ((*host) == "") {
          server = (const char *)nullptr;
          hints.ai_flags = AI_PASSIVE;
        } else {
          server = (const char *)*host;
        }

#if !defined(PLATFORM_WINDOWS)
        addrinfo *targetaddr;
        int status = getaddrinfo(server, string(port), &hints, &targetaddr);
        if (status != 0) {
          asGetActiveContext()->SetException(string{LOCATION " getaddrinfo: ", sockerr()}, true);
          return 0;
        }

        ssize_t num = ::sendto(
          sockfd,
          SEND_BUF_CAST(msg->At(0)),
          msg->GetSize(),
          0,
          targetaddr->ai_addr,
          targetaddr->ai_addrlen
        );
        if (num == -1) {
          asGetActiveContext()->SetException(string{LOCATION " sendto: ", sockerr()}, true);
          return 0;
        }

        freeaddrinfo(targetaddr);

        return num;
#else
        addrinfo *targetaddr;
        int status = getaddrinfo(server, string(port), &hints, &targetaddr);
        if (status != 0) {
          int le = WSAGetLastError();
          asGetActiveContext()->SetException(string{LOCATION " getaddrinfo: le=", le, "; ", sockerr(le)}, true);
          return 0;
        }

        // [in    ]
        WSABUF buffer;
        buffer.buf = (char *)(msg->At(0));
        buffer.len = msg->GetSize();

        // [   out]
        DWORD bufsize = 0;
        // [in    ]
        DWORD flags = 0;

        int rc = ::WSASendTo(
          sock,
          &buffer,
          1,
          &bufsize,
          flags,
          targetaddr->ai_addr,
          targetaddr->ai_addrlen,
          // no overlapped I/O
          nullptr,
          nullptr
        );
        if (rc == SOCKET_ERROR) {
          int le = WSAGetLastError();
          switch (le) {
            default:
              asGetActiveContext()->SetException(string{LOCATION " WSASendTo: le=", le, "; ", sockerr(le)}, true);
              return 0;
          }
        }
        return bufsize;
#endif
      }

      int recv(CScriptArray *msg) {
#if !defined(PLATFORM_WINDOWS)
        // TODO: send back the received-from address somehow.
        struct sockaddr addr;
        socklen_t addrlen;

        struct pollfd pfd;
        pfd.fd = sockfd;
        pfd.events = POLLIN;

        int rv = ::poll(&pfd, 1, 0);
        if (rv == -1) {
          asGetActiveContext()->SetException(string{LOCATION " poll: ", sockerr()}, true);
          return 0;
        }
        if (rv == 0) {
          // no data ready:
          return 0;
        }
        if ((pfd.events & POLLIN) == 0) {
          // no data ready:
          return 0;
        }

        ssize_t num = ::recvfrom(sockfd, RECV_BUF_CAST(msg->At(0)), msg->GetSize(), 0, &addr, &addrlen);
        if (num == -1) {
          if (sockhaserr()) {
            asGetActiveContext()->SetException(string{LOCATION " recvfrom: ", sockerr()}, true);
            return 0;
          }
        }

        return num;
#else
        WSAPOLLFD pfd;
        pfd.fd = sock;
        pfd.events = POLLIN;

        int rc = ::WSAPoll(&pfd, 1, 0);
        if (rc == SOCKET_ERROR) {
            int le = WSAGetLastError();
            switch (le) {
                default:
                    asGetActiveContext()->SetException(string{LOCATION " WSAPoll: le=", le, "; ", sockerr(le)}, true);
                    return 0;
            }
        }
        if (rc == 0) {
            // no data available
            return 0;
        }

        struct sockaddr_in addr;
        int addrlen = sizeof(addr);

        // [in out]
        WSABUF buffer;
        buffer.buf = (char *)(msg->At(0));
        buffer.len = msg->GetSize();

        // [   out]
        DWORD bufsize = 0;
        // [in out]
        DWORD flags = 0;

        rc = ::WSARecvFrom(
          sock,
          &buffer,
          1,
          &bufsize,
          &flags,
          (SOCKADDR *)&addr,
          &addrlen,
          // no overlapped I/O
          nullptr,
          nullptr
        );
        if (rc == SOCKET_ERROR) {
          int le = WSAGetLastError();
          switch (le) {
            case WSAEMSGSIZE:
              return -1;

            default:
              asGetActiveContext()->SetException(string{LOCATION " WSARecvFrom: le=", le, "; ", sockerr(le)}, true);
              return 0;
          }
        }
        return bufsize;
#endif
      }
    };

    static UDPSocket *create_udp_socket(const string *host, int port) {
      assert(host != nullptr);

      addrinfo hints;
      const char *server;

      memset(&hints, 0, sizeof(addrinfo));
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_DGRAM;
      if ((*host) == "") {
        server = (const char *)nullptr;
        hints.ai_flags = AI_PASSIVE;
      } else {
        server = (const char *)*host;
      }

#if !defined(PLATFORM_WINDOWS)
      addrinfo *serverinfo;
      int status = getaddrinfo(server, string(port), &hints, &serverinfo);
      if (status != 0) {
        asGetActiveContext()->SetException(string{LOCATION " getaddrinfo: ", sockerr()}, true);
        return nullptr;
      }

      int sockfd = socket(serverinfo->ai_family, serverinfo->ai_socktype, serverinfo->ai_protocol);
      if (sockfd == -1) {
        asGetActiveContext()->SetException(string{LOCATION " socket: ", sockerr()}, true);
        return nullptr;
      }

      int yes = 1;
      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        asGetActiveContext()->SetException(string{LOCATION " setsockopt: ", sockerr()}, true);
        return nullptr;
      }

      if (server) {
        if (bind(sockfd, serverinfo->ai_addr, serverinfo->ai_addrlen) == -1) {
          close(sockfd);
          asGetActiveContext()->SetException(string{LOCATION " bind: ", sockerr()}, true);
          return nullptr;
        }
      }

      return new UDPSocket(sockfd, serverinfo);
#else
      SOCKET sock = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, 0);
      if (sock == INVALID_SOCKET) {
        int le = WSAGetLastError();
        asGetActiveContext()->SetException(string{LOCATION " WSASocket: le=", le, "; ", sockerr(le)}, true);
        return nullptr;
      }

      int yes = 1;
      int rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
      if (rc == SOCKET_ERROR) {
        int le = WSAGetLastError();
        asGetActiveContext()->SetException(string{LOCATION " setsockopt: le=", le, "; ", sockerr(le)}, true);
        return nullptr;
      }

      addrinfo *serverinfo;
      int status = getaddrinfo(server, string(port), &hints, &serverinfo);
      if (status != 0) {
        int le = WSAGetLastError();
        asGetActiveContext()->SetException(string{LOCATION " getaddrinfo: le=", le, "; ", sockerr(le)}, true);
        return nullptr;
      }

      if (server) {
        rc = bind(sock, serverinfo->ai_addr, serverinfo->ai_addrlen);
        if (rc == SOCKET_ERROR) {
          int le = WSAGetLastError();
          closesocket(sock);
          asGetActiveContext()->SetException(string{LOCATION " bind: le=", le, "; ", sockerr(le)}, true);
          return nullptr;
        }
      }

      return new UDPSocket(sock, serverinfo);
#endif
    }
  };

  struct GUI {
    struct Object {
      virtual operator hiro::mObject*() = 0;
    };

    struct Sizable : Object {
      virtual operator hiro::mSizable*() = 0;
    };

#define BindShared(Name) \
    hiro::s##Name self; \
    ~Name() { \
      self->setVisible(false); \
      self->reset(); \
      /*self->destruct();*/ \
    } \
    auto ref_add() -> void { self.manager->strong++; } \
    auto ref_release() -> void { \
      if (--self.manager->strong == 0) delete this; \
    }

#define BindObject(Name) \
    operator hiro::mObject*() { return (hiro::mObject*)self.data(); } \
    auto setVisible(bool visible = true) -> void { \
      self->setVisible(visible); \
    }

#define BindConstructor(Name) \
    Name() { \
      self = new hiro::m##Name(); \
      self->construct(); \
    } \

#define BindSizable(Name) \
    operator hiro::mSizable*() override { \
      return (hiro::mSizable*)self.data(); \
    }

    struct Window : Object {
      BindShared(Window) \
      BindObject(Window)

      Window() {
        self = new hiro::mWindow();
        self->construct();
        self->setVisible();
        self->setFocused();
      }

      // create a Window relative to main presentation window
      Window(float x, float y, bool relative = false) : Window() {
        if (relative) {
          self->setPosition(platform->presentationWindow(), hiro::Position{x, y});
        } else {
          self->setPosition(hiro::Position{x, y});
        }
      }

      auto appendSizable(Sizable *child) -> void {
        self->append((hiro::mSizable*)*child);
      }

      auto setSize(hiro::Size *size) -> void {
        self->setSize(*size);
      }

      auto setTitle(const string *title) -> void {
        self->setTitle(*title);
      }
    };

    struct VerticalLayout : Sizable {
      BindShared(VerticalLayout)
      BindObject(VerticalLayout)
      BindSizable(VerticalLayout)

      VerticalLayout() {
        self = new hiro::mVerticalLayout();
        self->construct();
      }

      auto appendSizable(Sizable *child, hiro::Size *size) -> void {
        self->append((hiro::mSizable*)*child, *size);
      }
    };

    struct HorizontalLayout : Sizable {
      BindShared(HorizontalLayout)
      BindObject(HorizontalLayout)
      BindSizable(HorizontalLayout)

      HorizontalLayout() {
        self = new hiro::mHorizontalLayout();
        self->construct();
      }

      auto appendSizable(Sizable *child, hiro::Size *size) -> void {
        self->append((hiro::mSizable*)*child, *size);
      }
    };

    struct LineEdit : Sizable {
      BindShared(LineEdit)
      BindObject(LineEdit)
      BindSizable(LineEdit)

      LineEdit() {
        self = new hiro::mLineEdit();
        self->construct();
        self->setFocusable();
        self->setVisible();
      }

      auto getText() -> string* {
        return new string(self->text());
      }

      auto setText(const string *text) -> void {
        self->setText(*text);
      }

      asIScriptFunction *onChange = nullptr;
      auto setOnChange(asIScriptFunction *func) -> void {
        if (onChange != nullptr) {
          onChange->Release();
        }

        onChange = func;

        // register onChange callback with LineEdit:
        auto me = this;
        auto ctx = asGetActiveContext();
        self->onChange([=]() -> void {
          ctx->Prepare(onChange);
          ctx->SetArgObject(0, me);
          ctx->Execute();
        });
      }
    };

    struct Label : Sizable {
      BindShared(Label)
      BindObject(Label)
      BindSizable(Label)

      Label() {
        self = new hiro::mLabel();
        self->construct();
        self->setVisible();
      }

      auto getText() -> string * {
        return new string(self->text());
      }

      auto setText(const string *text) -> void {
        self->setText(*text);
      }
    };

    struct Button : Sizable {
      BindShared(Button)
      BindObject(Button)
      BindSizable(Button)

      Button() {
        self = new hiro::mButton();
        self->construct();
        self->setFocusable();
        self->setVisible();
      }

      auto getText() -> string* {
        return new string(self->text());
      }

      auto setText(const string *text) -> void {
        self->setText(*text);
      }

      asIScriptFunction *onActivate = nullptr;
      auto setOnActivate(asIScriptFunction *func) -> void {
        if (onActivate != nullptr) {
          onActivate->Release();
        }

        onActivate = func;

        // register onActivate callback with Button:
        auto me = this;
        auto ctx = asGetActiveContext();
        self->onActivate([=]() -> void {
          ctx->Prepare(onActivate);
          ctx->SetArgObject(0, me);
          ctx->Execute();
        });
      }
    };

#define Constructor(Name) \
    static auto create##Name() -> Name* { return new Name(); }

    // Size is a value type:
    static auto createSize(void *memory) -> void { new(memory) hiro::Size(); }
    static auto createSizeWH(float width, float height, void *memory) -> void { new(memory) hiro::Size(width, height); }
    static auto destroySize(void *memory) -> void { ((hiro::Size*)memory)->~Size(); }

    Constructor(Window)
    static auto createWindowAtPosition(float x, float y, bool relative) -> Window* { return new Window(x, y, relative); }
    Constructor(VerticalLayout)
    Constructor(HorizontalLayout)
    Constructor(LineEdit)
    Constructor(Label)
    Constructor(Button)

#undef Constructor
  };
};

ScriptInterface scriptInterface;

auto Interface::registerScriptDefs() -> void {
  int r;

  script.engine = platform->scriptEngine();

  // register global functions for the script to use:
  auto defaultNamespace = script.engine->GetDefaultNamespace();

  // Register script-array extension:
  RegisterScriptArray(script.engine, true /* bool defaultArrayType */);

  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const uint16 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint16), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const uint32 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint32), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const ? &in other)", asFUNCTION(ScriptInterface::uint8_array_append_array), asCALL_CDECL_OBJFIRST); assert(r >= 0);

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
  r = script.engine->RegisterGlobalFunction("uint8 sprite_width(uint8 baseSize, uint8 size)", asFUNCTION(ScriptInterface::PPUAccess::ppu_sprite_width), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 sprite_height(uint8 baseSize, uint8 size)", asFUNCTION(ScriptInterface::PPUAccess::ppu_sprite_height), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 sprite_base_size()", asFUNCTION(ScriptInterface::PPUAccess::ppu_sprite_base_size), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint16 tile_address(bool nameselect)", asFUNCTION(ScriptInterface::PPUAccess::ppu_tile_address), asCALL_CDECL); assert(r >= 0);

  // define ppu::VRAM object type for opIndex convenience:
  r = script.engine->RegisterObjectType("VRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("VRAM", "uint16 opIndex(uint16 addr)", asMETHOD(ScriptInterface::PPUAccess, vram_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("VRAM", "uint read_sprite(uint16 tiledataAddress, uint8 chr, uint8 width, uint8 height, array<uint32> &inout output)", asMETHOD(ScriptInterface::PPUAccess, vram_read_sprite), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("VRAM vram", &scriptInterface.ppuAccess); assert(r >= 0);

  // define ppu::CGRAM object type for opIndex convenience:
  r = script.engine->RegisterObjectType("CGRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("CGRAM", "uint16 opIndex(uint16 addr)", asMETHOD(ScriptInterface::PPUAccess, cgram_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("CGRAM cgram", &scriptInterface.ppuAccess); assert(r >= 0);

  r = script.engine->RegisterObjectType    ("OAMSprite", sizeof(ScriptInterface::PPUAccess::OAMObject), asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
  r = script.engine->RegisterObjectMethod  ("OAMSprite", "bool   get_is_enabled()", asMETHOD(ScriptInterface::PPUAccess::OAMObject, get_is_enabled), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint16 x", asOFFSET(ScriptInterface::PPUAccess::OAMObject, x)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  y", asOFFSET(ScriptInterface::PPUAccess::OAMObject, y)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  character", asOFFSET(ScriptInterface::PPUAccess::OAMObject, character)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "bool   nameselect", asOFFSET(ScriptInterface::PPUAccess::OAMObject, nameselect)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "bool   vflip", asOFFSET(ScriptInterface::PPUAccess::OAMObject, vflip)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "bool   hflip", asOFFSET(ScriptInterface::PPUAccess::OAMObject, hflip)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  priority", asOFFSET(ScriptInterface::PPUAccess::OAMObject, priority)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  palette", asOFFSET(ScriptInterface::PPUAccess::OAMObject, palette)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  size", asOFFSET(ScriptInterface::PPUAccess::OAMObject, size)); assert(r >= 0);
  r = script.engine->RegisterObjectMethod  ("OAMSprite", "uint8  get_width()", asMETHOD(ScriptInterface::PPUAccess::OAMObject, get_width), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod  ("OAMSprite", "uint8  get_height()", asMETHOD(ScriptInterface::PPUAccess::OAMObject, get_height), asCALL_THISCALL); assert(r >= 0);

  r = script.engine->RegisterObjectType  ("OAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "OAMSprite @get_opIndex(uint8 chr)", asFUNCTION(ScriptInterface::PPUAccess::oam_get_object), asCALL_CDECL_OBJFIRST); assert(r >= 0);

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

    r = script.engine->RegisterObjectProperty("ExtraTile", "int x", asOFFSET(PPUfast::ExtraTile, x)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "int y", asOFFSET(PPUfast::ExtraTile, y)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint source", asOFFSET(PPUfast::ExtraTile, source)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "bool hflip", asOFFSET(PPUfast::ExtraTile, hflip)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "bool vflip", asOFFSET(PPUfast::ExtraTile, vflip)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint priority", asOFFSET(PPUfast::ExtraTile, priority)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint width", asOFFSET(PPUfast::ExtraTile, width)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint height", asOFFSET(PPUfast::ExtraTile, height)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("ExtraTile", "uint index", asOFFSET(PPUfast::ExtraTile, index)); assert(r >= 0);

    r = script.engine->RegisterObjectMethod("ExtraTile", "void reset()", asFUNCTION(ScriptInterface::ExtraLayer::tile_reset), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixels_clear()", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixels_clear), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixel_set(int x, int y, uint16 color)", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixel_set), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixel_off(int x, int y)", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixel_off), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixel_on(int x, int y)", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixel_on), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void pixel(int x, int y)", asFUNCTION(ScriptInterface::ExtraLayer::tile_pixel_set), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("ExtraTile", "void draw_sprite(int x, int y, int width, int height, const array<uint32> &in tiledata, const array<uint16> &in palette)", asFUNCTION(ScriptInterface::ExtraLayer::tile_draw_sprite), asCALL_CDECL_OBJFIRST); assert(r >= 0);

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

  {
    r = script.engine->SetDefaultNamespace("net"); assert(r >= 0);
    r = script.engine->RegisterObjectType("UDPSocket", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("UDPSocket", asBEHAVE_FACTORY, "UDPSocket@ f(const string &in host, const int port)", asFUNCTION(ScriptInterface::Net::create_udp_socket), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("UDPSocket", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::UDPSocket, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("UDPSocket", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::UDPSocket, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("UDPSocket", "int sendto(const array<uint8> &in msg, const string &in host, const int port)", asMETHOD(ScriptInterface::Net::UDPSocket, sendto), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("UDPSocket", "int recv(const array<uint8> &in msg)", asMETHOD(ScriptInterface::Net::UDPSocket, recv), asCALL_THISCALL); assert( r >= 0 );
  }

  // UI
  {
    r = script.engine->SetDefaultNamespace("gui"); assert(r >= 0);

    // Register types first:
    r = script.engine->RegisterObjectType("Window", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("VerticalLayout", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("HorizontalLayout", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("LineEdit", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("Label", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("Button", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectType("Size", sizeof(hiro::Size), asOBJ_VALUE); assert(r >= 0);

    // Size value type:
    r = script.engine->RegisterObjectBehaviour("Size", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(ScriptInterface::GUI::createSize), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Size", asBEHAVE_CONSTRUCT, "void f(float width, float height)", asFUNCTION(ScriptInterface::GUI::createSizeWH), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Size", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(ScriptInterface::GUI::destroySize), asCALL_CDECL_OBJLAST); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Size", "float get_width()", asMETHOD(hiro::Size, width), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Size", "void set_width(float width)", asMETHOD(hiro::Size, setWidth), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Size", "float get_height()", asMETHOD(hiro::Size, height), asCALL_THISCALL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Size", "void set_height(float height)", asMETHOD(hiro::Size, setHeight), asCALL_THISCALL); assert(r >= 0);

    // Window
    r = script.engine->RegisterObjectBehaviour("Window", asBEHAVE_FACTORY, "Window@ f()", asFUNCTION(ScriptInterface::GUI::createWindow), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Window", asBEHAVE_FACTORY, "Window@ f(float rx, float ry, bool relative)", asFUNCTION(ScriptInterface::GUI::createWindowAtPosition), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Window", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::Window, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("Window", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::Window, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Window", "void append(VerticalLayout @sizable)", asMETHOD(ScriptInterface::GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Window", "void append(HorizontalLayout @sizable)", asMETHOD(ScriptInterface::GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Window", "void append(LineEdit @sizable)", asMETHOD(ScriptInterface::GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Window", "void append(Label @sizable)", asMETHOD(ScriptInterface::GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Window", "void append(Button @sizable)", asMETHOD(ScriptInterface::GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Window", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::Window, setVisible), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Window", "void set_title(const string &in title)", asMETHOD(ScriptInterface::GUI::Window, setTitle), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Window", "void set_size(Size &in size)", asMETHOD(ScriptInterface::GUI::Window, setSize), asCALL_THISCALL); assert( r >= 0 );

    // VerticalLayout
    r = script.engine->RegisterObjectBehaviour("VerticalLayout", asBEHAVE_FACTORY, "VerticalLayout@ f()", asFUNCTION(ScriptInterface::GUI::createVerticalLayout), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("VerticalLayout", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::VerticalLayout, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("VerticalLayout", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::VerticalLayout, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(VerticalLayout @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(HorizontalLayout @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(LineEdit @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(Label @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(Button @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("VerticalLayout", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::VerticalLayout, setVisible), asCALL_THISCALL); assert( r >= 0 );

    // HorizontalLayout
    r = script.engine->RegisterObjectBehaviour("HorizontalLayout", asBEHAVE_FACTORY, "HorizontalLayout@ f()", asFUNCTION(ScriptInterface::GUI::createHorizontalLayout), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("HorizontalLayout", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::HorizontalLayout, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("HorizontalLayout", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::HorizontalLayout, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(VerticalLayout @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(HorizontalLayout @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(LineEdit @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(Label @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(Button @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("HorizontalLayout", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, setVisible), asCALL_THISCALL); assert( r >= 0 );

    // LineEdit
    // Register a simple funcdef for the callback
    r = script.engine->RegisterFuncdef("void LineEditCallback(LineEdit @self)");
    r = script.engine->RegisterObjectBehaviour("LineEdit", asBEHAVE_FACTORY, "LineEdit@ f()", asFUNCTION(ScriptInterface::GUI::createLineEdit), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("LineEdit", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::LineEdit, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("LineEdit", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::LineEdit, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("LineEdit", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::LineEdit, setVisible), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("LineEdit", "string &get_text()", asMETHOD(ScriptInterface::GUI::LineEdit, getText), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("LineEdit", "void set_text(const string &in text)", asMETHOD(ScriptInterface::GUI::LineEdit, setText), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("LineEdit", "void set_on_change(LineEditCallback @cb)", asMETHOD(ScriptInterface::GUI::LineEdit, setOnChange), asCALL_THISCALL); assert( r >= 0 );

    // Label
    r = script.engine->RegisterObjectBehaviour("Label", asBEHAVE_FACTORY, "Label@ f()", asFUNCTION(ScriptInterface::GUI::createLabel), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Label", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::Label, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("Label", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::Label, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Label", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::Label, setVisible), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Label", "string &get_text()", asMETHOD(ScriptInterface::GUI::Label, getText), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Label", "void set_text(const string &in text)", asMETHOD(ScriptInterface::GUI::Label, setText), asCALL_THISCALL); assert( r >= 0 );

    // Button
    // Register a simple funcdef for the callback
    r = script.engine->RegisterFuncdef("void ButtonCallback(Button @self)");
    r = script.engine->RegisterObjectBehaviour("Button", asBEHAVE_FACTORY, "Button@ f()", asFUNCTION(ScriptInterface::GUI::createButton), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Button", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::Button, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("Button", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::Button, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Button", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::Button, setVisible), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Button", "string &get_text()", asMETHOD(ScriptInterface::GUI::Button, getText), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Button", "void set_text(const string &in text)", asMETHOD(ScriptInterface::GUI::Button, setText), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Button", "void set_on_activate(ButtonCallback @cb)", asMETHOD(ScriptInterface::GUI::Button, setOnActivate), asCALL_THISCALL); assert( r >= 0 );
  }

  r = script.engine->SetDefaultNamespace(defaultNamespace); assert(r >= 0);

  // create context:
  script.context = script.engine->CreateContext();

  script.context->SetExceptionCallback(asMETHOD(ScriptInterface, exceptionCallback), &scriptInterface, asCALL_THISCALL);
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
