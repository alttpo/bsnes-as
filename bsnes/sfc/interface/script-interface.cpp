
#include <sfc/sfc.hpp>

#if !defined(PLATFORM_WINDOWS)
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #define SEND_BUF_CAST(t) ((const void *)(t))
  #define RECV_BUF_CAST(t) ((void *)(t))

int sock_capture_error() {
  return errno;
}

char* sock_error_string(int err) {
  return strerror(err);
}

bool sock_has_error(int err) {
  return err != 0;
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

char* sock_error_string(int errcode) {
  DWORD len = FormatMessageA(FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errcode, 0, errmsg, 255, NULL);
  if (len != 0)
    errmsg[len] = 0;
  else
    sprintf(errmsg, "error %d", errcode);
  return errmsg;
}

int sock_capture_error() {
  return WSAGetLastError();
}

bool sock_has_error(int err) {
  return FAILED(err);
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

  static auto message(const string *msg) -> void {
    platform->scriptMessage(msg);
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
        //printf("(%p) [%p]->AddRef()\n", this, cb);
        cb->AddRef();
      }
      write_interceptor(const write_interceptor& other) : cb(other.cb), ctx(other.ctx) {
        //printf("(%p) [%p]->AddRef()\n", this, cb);
        cb->AddRef();
      }
      write_interceptor(const write_interceptor&& other) : cb(other.cb), ctx(other.ctx) {
        //printf("(%p) moved in [%p]\n", this, cb);
      }
      ~write_interceptor() {
        //printf("(%p) [%p]->Release()\n", this, cb);
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
        //printf("(%p) [%p]->AddRef()\n", this, cb);
        cb->AddRef();
      }
      dma_interceptor(const dma_interceptor& other) : cb(other.cb), ctx(other.ctx) {
        //printf("(%p) [%p]->AddRef()\n", this, cb);
        cb->AddRef();
      }
      dma_interceptor(const dma_interceptor&& other) : cb(other.cb), ctx(other.ctx) {
        //printf("(%p) moved in [%p]\n", this, cb);
      }
      ~dma_interceptor() {
        //printf("(%p) [%p]->Release()\n", this, cb);
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

    auto vram_chr_address(uint16 chr) -> uint16 {
      return 0x4000u + (chr << 4u);
    }

    auto vram_read_block(uint16 addr, uint offs, uint16 size, CScriptArray *output) -> void {
      if (output == nullptr) {
        asGetActiveContext()->SetException("output array cannot be null", true);
        return;
      }
      if (output->GetElementTypeId() != asTYPEID_UINT16) {
        asGetActiveContext()->SetException("output array must be of type uint16[]", true);
        return;
      }

      auto vram = system.fastPPU() ? (uint16 *)ppufast.vram : (uint16 *)ppu.vram.data;

      for (uint i = 0; i < size; i++) {
        auto tmp = vram[addr + i];
        output->SetValue(offs + i, &tmp);
      }
    }

    auto vram_write_block(uint16 addr, uint offs, uint16 size, CScriptArray *data) -> void {
      if (data == nullptr) {
        asGetActiveContext()->SetException("input array cannot be null", true);
        return;
      }
      if (data->GetElementTypeId() != asTYPEID_UINT16) {
        asGetActiveContext()->SetException("input array must be of type uint16[]", true);
        return;
      }
      if (data->GetSize() < (offs + size)) {
        asGetActiveContext()->SetException("input array size not big enough to contain requested block", true);
        return;
      }

      auto p = reinterpret_cast<const uint16 *>(data->At(offs));
      if (system.fastPPU()) {
        auto vram = (uint16 *)ppufast.vram;
        for (uint a = 0; a < size; a++) {
          auto word = *p++;
          vram[(addr + a) & 0x7fff] = word;
          ppufast.updateTiledata((addr + a));
        }
      } else {
        auto vram = (uint16 *)ppu.vram.data;
        for (uint a = 0; a < size; a++) {
          auto word = *p++;
          vram[(addr + a) & 0x7fff] = word;
        }
      }
    }

    struct OAMObject {
      uint9  x;
      uint8  y;
      uint16 character;
      bool   vflip;
      bool   hflip;
      uint2  priority;
      uint3  palette;
      uint1  size;

      auto get_is_enabled() -> bool {
        uint sprite_width = get_width();
        uint sprite_height = get_height();

        bool inX = !(x > 256 && x + sprite_width - 1 < 512);
        bool inY =
          // top part of sprite is in view
          (y >= 0 && y < 240) ||
          // OR bottom part of sprite is in view
          (y >= 240 && (y + sprite_height - 1 & 255) > 0 && (y + sprite_height - 1 & 255) < 240);
        return inX && inY;
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
        local->oam_objects[index].character = uint16(po.character) | (uint16(po.nameselect) << 8u);
        local->oam_objects[index].vflip = po.vflip;
        local->oam_objects[index].hflip = po.hflip;
        local->oam_objects[index].priority = po.priority;
        local->oam_objects[index].palette = po.palette;
        local->oam_objects[index].size = po.size;
      } else {
        auto po = ppu.obj.oam.object[index];
        local->oam_objects[index].x = po.x;
        local->oam_objects[index].y = po.y;
        local->oam_objects[index].character = uint16(po.character) | (uint16(po.nameselect) << 8u);
        local->oam_objects[index].vflip = po.vflip;
        local->oam_objects[index].hflip = po.hflip;
        local->oam_objects[index].priority = po.priority;
        local->oam_objects[index].palette = po.palette;
        local->oam_objects[index].size = po.size;
      }
      return &local->oam_objects[index];
    }

    static auto oam_set_object(ScriptInterface::PPUAccess *local, uint8 index, ScriptInterface::PPUAccess::OAMObject *obj) -> void {
      if (system.fastPPU()) {
        auto &po = ppufast.objects[index];
        po.x = obj->x;
        po.y = obj->y;
        po.character = (obj->character & 0xFFu);
        po.nameselect = (obj->character >> 8u) & 1u;
        po.vflip = obj->vflip;
        po.hflip = obj->hflip;
        po.priority = obj->priority;
        po.palette = obj->palette;
        po.size = obj->size;
      } else {
        auto &po = ppu.obj.oam.object[index];
        po.x = obj->x;
        po.y = obj->y;
        po.character = (obj->character & 0xFFu);
        po.nameselect = (obj->character >> 8u) & 1u;
        po.vflip = obj->vflip;
        po.hflip = obj->hflip;
        po.priority = obj->priority;
        po.palette = obj->palette;
        po.size = obj->size;
      }
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

  struct Net {
    struct Address {
      addrinfo *info;
      int rc;   // getaddrinfo return value
      int err;  // platform-specific error code

      // constructor:
      Address(const string *host, int port) {
        info = nullptr;
        rc = -1;
        err = 0;

        addrinfo hints;
        const char *addr;

        memset(&hints, 0, sizeof(addrinfo));
        hints.ai_family = AF_INET;

        if (host == nullptr || (*host) == "") {
          addr = (const char *)nullptr;
          // AI_PASSIVE hint is ignored otherwise.
          hints.ai_flags = AI_PASSIVE;
        } else {
          addr = (const char *)*host;
        }

        rc = ::getaddrinfo(addr, string(port), &hints, &info);
        if (rc != 0) {
          if (rc == EAI_SYSTEM) {
            err = sock_capture_error();
          } else {
            err = 0;
          }
        }
      }

      ~Address() {
        if (info) {
          ::freeaddrinfo(info);
        }
        info = nullptr;
      }

      operator bool() {
        return info && (rc == 0);
      }

      auto throw_if_invalid() -> bool {
        if (rc == 0) return false;

        // throw script exception:

        // system error occurred so rely on errno functions here:
        if (rc == EAI_SYSTEM) {
          asGetActiveContext()->SetException(string{LOCATION " getaddrinfo: errno=", err, "; ", sock_error_string(err)}, true);
          return true;
        }

        // regular getaddrinfo error occurred, so use gai_strerror:
        asGetActiveContext()->SetException(string{LOCATION " getaddrinfo: ", gai_strerror(rc)}, true);
        return true;
      }
    };

    static auto resolve(const string *host, int port) -> Address* {
      return new Address(host, port);
    }

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
          int err = sock_capture_error();
          asGetActiveContext()->SetException(string{LOCATION " getaddrinfo: ", sock_error_string(err)}, true);
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
          int err = sock_capture_error();
          asGetActiveContext()->SetException(string{LOCATION " sendto: ", sock_error_string(err)}, true);
          return 0;
        }

        freeaddrinfo(targetaddr);

        return num;
#else
        addrinfo *targetaddr;
        int status = getaddrinfo(server, string(port), &hints, &targetaddr);
        if (status != 0) {
          int err = sock_capture_error();
          asGetActiveContext()->SetException(string{LOCATION " getaddrinfo: err=", err, "; ", sock_error_string(err)}, true);
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
          int err = sock_capture_error();
          switch (err) {
            default:
              asGetActiveContext()->SetException(string{LOCATION " WSASendTo: err=", err, "; ", sock_error_string(err)}, true);
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
          int err = sock_capture_error();
          asGetActiveContext()->SetException(string{LOCATION " poll: ", sock_error_string(err)}, true);
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
          int err = sock_capture_error();
          if (sock_has_error(err)) {
            asGetActiveContext()->SetException(string{LOCATION " recvfrom: ", sock_error_string(err)}, true);
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
            int err = sock_capture_error();
            switch (err) {
                default:
                    asGetActiveContext()->SetException(string{LOCATION " WSAPoll: err=", err, "; ", sock_error_string(err)}, true);
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
          int err = sock_capture_error();
          switch (err) {
            case WSAEMSGSIZE:
              return -1;

            default:
              asGetActiveContext()->SetException(string{LOCATION " WSARecvFrom: err=", err, "; ", sock_error_string(err)}, true);
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
        int err = sock_capture_error();
        asGetActiveContext()->SetException(string{LOCATION " getaddrinfo: ", sock_error_string(err)}, true);
        return nullptr;
      }

      int sockfd = socket(serverinfo->ai_family, serverinfo->ai_socktype, serverinfo->ai_protocol);
      if (sockfd == -1) {
        int err = sock_capture_error();
        asGetActiveContext()->SetException(string{LOCATION " socket: ", sock_error_string(err)}, true);
        return nullptr;
      }

      int yes = 1;
      if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
        int err = sock_capture_error();
        asGetActiveContext()->SetException(string{LOCATION " setsockopt: ", sock_error_string(err)}, true);
        return nullptr;
      }

      if (server) {
        if (bind(sockfd, serverinfo->ai_addr, serverinfo->ai_addrlen) == -1) {
          int err = sock_capture_error();
          close(sockfd);
          asGetActiveContext()->SetException(string{LOCATION " bind: ", sock_error_string(err)}, true);
          return nullptr;
        }
      }

      return new UDPSocket(sockfd, serverinfo);
#else
      SOCKET sock = WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, 0);
      if (sock == INVALID_SOCKET) {
        int err = sock_capture_error();
        asGetActiveContext()->SetException(string{LOCATION " WSASocket: err=", err, "; ", sock_error_string(err)}, true);
        return nullptr;
      }

      int yes = 1;
      int rc = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
      if (rc == SOCKET_ERROR) {
        int err = sock_capture_error();
        asGetActiveContext()->SetException(string{LOCATION " setsockopt: err=", err, "; ", sock_error_string(err)}, true);
        return nullptr;
      }

      addrinfo *serverinfo;
      int status = getaddrinfo(server, string(port), &hints, &serverinfo);
      if (status != 0) {
        int err = sock_capture_error();
        asGetActiveContext()->SetException(string{LOCATION " getaddrinfo: err=", err, "; ", sock_error_string(err)}, true);
        return nullptr;
      }

      if (server) {
        rc = bind(sock, serverinfo->ai_addr, serverinfo->ai_addrlen);
        if (rc == SOCKET_ERROR) {
          int err = sock_capture_error();
          closesocket(sock);
          asGetActiveContext()->SetException(string{LOCATION " bind: err=", err, "; ", sock_error_string(err)}, true);
          return nullptr;
        }
      }

      return new UDPSocket(sock, serverinfo);
#endif
    }

    struct WebSocketServer {
    };

    static WebSocketServer *create_web_socket_server(const string *host, int port) {
      return new WebSocketServer();
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

      auto resize() -> void {
        self->resize();
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

      auto resize() -> void {
        self->resize();
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

    struct Canvas : Sizable {
      BindShared(Canvas)
      BindObject(Canvas)
      BindSizable(Canvas)

      Canvas() {
        self = new hiro::mCanvas();
        self->construct();
      }

      auto setSize(hiro::Size *size) -> void {
        // Set 15-bit BGR format for PPU-compatible images:
        image img(0, 16, 0x8000u, 0x001Fu, 0x03E0u, 0x7C00u);
        img.allocate(size->width(), size->height());
        img.fill(0x0000u);
        self->setIcon(img);
      }

      auto update() -> void {
        self->update();
      }

      auto fill(uint16 color) -> void {
        auto& img = self->iconRef();
        img.fill(color);
      }

      auto pixel(int x, int y, uint16 color) -> void {
        auto& img = self->iconRef();
        // bounds check:
        if (x < 0 || y < 0 || x >= img.width() || y >= img.height()) return;
        // set pixel with full alpha (1-bit on/off):
        img.write(img.data() + (y * img.pitch()) + (x * img.stride()), color | 0x8000u);
      }

      auto draw_sprite_4bpp(int x, int y, uint c, uint width, uint height, const CScriptArray *tile_data, const CScriptArray *palette_data) -> void {
        // Check validity of array inputs:
        if (tile_data == nullptr) {
          asGetActiveContext()->SetException("tile_data array cannot be null", true);
          return;
        }
        if (tile_data->GetElementTypeId() != asTYPEID_UINT16) {
          asGetActiveContext()->SetException("tile_data array must be uint16[]", true);
          return;
        }
        if (tile_data->GetSize() < 16) {
          asGetActiveContext()->SetException("tile_data array must have at least 16 elements", true);
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
          asGetActiveContext()->SetException("palette_data array must have 16 elements", true);
          return;
        }

        auto tile_data_p = static_cast<const uint16 *>(tile_data->At(0));
        if (tile_data_p == nullptr) {
          asGetActiveContext()->SetException("tile_data array value pointer must not be null", true);
          return;
        }
        auto palette_p = static_cast<const b5g5r5 *>(palette_data->At(0));
        if (palette_p == nullptr) {
          asGetActiveContext()->SetException("palette_data array value pointer must not be null", true);
          return;
        }

        auto tileWidth = width >> 3u;
        auto tileHeight = height >> 3u;
        for (int ty = 0; ty < tileHeight; ty++) {
          for (int tx = 0; tx < tileWidth; tx++) {
            // data is stored in runs of 8x8 pixel tiles:
            auto p = tile_data_p + (((((c >> 4u) + ty) << 4u) + (tx + c & 15)) << 4u);

            for (int py = 0; py < 8; py++) {
              // consume 8 pixel columns:
              uint32 tile = uint32(p[py]) | (uint32(p[py+8]) << 16u);
              for (int px = 0; px < 8; px++) {
                uint32 t = 0u, shift = 7u - px;
                t += tile >> (shift + 0u) & 1u;
                t += tile >> (shift + 7u) & 2u;
                t += tile >> (shift + 14u) & 4u;
                t += tile >> (shift + 21u) & 8u;
                if (t) {
                  auto color = palette_p[t];
                  pixel(x + (tx << 3) + px, y + (ty << 3) + py, color);
                }
              }
            }
          }
        }
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
    Constructor(Canvas)

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
  r = script.engine->RegisterGlobalFunction("void message(const string &in msg)", asFUNCTION(ScriptInterface::message), asCALL_CDECL); assert(r >= 0);

  // default bus memory functions:
  {
    r = script.engine->SetDefaultNamespace("bus"); assert(r >= 0);

    // read functions:
    r = script.engine->RegisterGlobalFunction("uint8 read_u8(uint32 addr)",  asFUNCTION(ScriptInterface::Bus::read_u8), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("uint16 read_u16(uint32 addr0, uint32 addr1)", asFUNCTION(ScriptInterface::Bus::read_u16), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void read_block_u8(uint32 addr, uint offs, uint16 size, const array<uint8> &in output)",  asFUNCTION(ScriptInterface::Bus::read_block_u8), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void read_block_u16(uint32 addr, uint offs, uint16 size, const array<uint16> &in output)",  asFUNCTION(ScriptInterface::Bus::read_block_u16), asCALL_CDECL); assert(r >= 0);

    // write functions:
    r = script.engine->RegisterGlobalFunction("void write_u8(uint32 addr, uint8 data)", asFUNCTION(ScriptInterface::Bus::write_u8), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void write_u16(uint32 addr0, uint32 addr1, uint16 data)", asFUNCTION(ScriptInterface::Bus::write_u16), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void write_block_u8(uint32 addr, uint offs, uint16 size, const array<uint8> &in output)", asFUNCTION(ScriptInterface::Bus::write_block_u8), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void write_block_u16(uint32 addr, uint offs, uint16 size, const array<uint16> &in output)", asFUNCTION(ScriptInterface::Bus::write_block_u16), asCALL_CDECL); assert(r >= 0);

    r = script.engine->RegisterFuncdef("void WriteInterceptCallback(uint32 addr, uint8 value)"); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void add_write_interceptor(const string &in addr, uint32 size, WriteInterceptCallback @cb)", asFUNCTION(ScriptInterface::Bus::add_write_interceptor), asCALL_CDECL); assert(r >= 0);
  }

  {
    r = script.engine->SetDefaultNamespace("cpu"); assert(r >= 0);
    r = script.engine->RegisterObjectType    ("DMAIntercept", sizeof(CPU::DMAIntercept), asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  channel", asOFFSET(CPU::DMAIntercept, channel)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  transferMode", asOFFSET(CPU::DMAIntercept, transferMode)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  fixedTransfer", asOFFSET(CPU::DMAIntercept, fixedTransfer)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  reverseTransfer", asOFFSET(CPU::DMAIntercept, reverseTransfer)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  unused", asOFFSET(CPU::DMAIntercept, unused)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  indirect", asOFFSET(CPU::DMAIntercept, indirect)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  direction", asOFFSET(CPU::DMAIntercept, direction)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  targetAddress", asOFFSET(CPU::DMAIntercept, targetAddress)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint16 sourceAddress", asOFFSET(CPU::DMAIntercept, sourceAddress)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  sourceBank", asOFFSET(CPU::DMAIntercept, sourceBank)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint16 transferSize", asOFFSET(CPU::DMAIntercept, transferSize)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint16 indirectAddress", asOFFSET(CPU::DMAIntercept, indirectAddress)); assert(r >= 0);
    r = script.engine->RegisterObjectProperty("DMAIntercept", "uint8  indirectBank", asOFFSET(CPU::DMAIntercept, indirectBank)); assert(r >= 0);

    r = script.engine->RegisterFuncdef("void DMAInterceptCallback(DMAIntercept @dma)"); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("void register_dma_interceptor(DMAInterceptCallback @cb)", asFUNCTION(ScriptInterface::Bus::register_dma_interceptor), asCALL_CDECL); assert(r >= 0);
  }

  // create ppu namespace
  r = script.engine->SetDefaultNamespace("ppu"); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint16 rgb(uint8 r, uint8 g, uint8 b)", asFUNCTION(ScriptInterface::PPUAccess::ppu_rgb), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 get_luma()", asFUNCTION(ScriptInterface::PPUAccess::ppu_get_luma), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 sprite_width(uint8 baseSize, uint8 size)", asFUNCTION(ScriptInterface::PPUAccess::ppu_sprite_width), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 sprite_height(uint8 baseSize, uint8 size)", asFUNCTION(ScriptInterface::PPUAccess::ppu_sprite_height), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("uint8 sprite_base_size()", asFUNCTION(ScriptInterface::PPUAccess::ppu_sprite_base_size), asCALL_CDECL); assert(r >= 0);

  // define ppu::VRAM object type for opIndex convenience:
  r = script.engine->RegisterObjectType("VRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("VRAM", "uint16 opIndex(uint16 addr)", asMETHOD(ScriptInterface::PPUAccess, vram_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("VRAM", "uint16 chr_address(uint16 chr)", asMETHOD(ScriptInterface::PPUAccess, vram_chr_address), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("VRAM", "void read_block(uint16 addr, uint offs, uint16 size, array<uint16> &inout output)", asMETHOD(ScriptInterface::PPUAccess, vram_read_block), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("VRAM", "void write_block(uint16 addr, uint offs, uint16 size, const array<uint16> &in data)", asMETHOD(ScriptInterface::PPUAccess, vram_write_block), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("VRAM vram", &scriptInterface.ppuAccess); assert(r >= 0);

  // define ppu::CGRAM object type for opIndex convenience:
  r = script.engine->RegisterObjectType("CGRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("CGRAM", "uint16 opIndex(uint16 addr)", asMETHOD(ScriptInterface::PPUAccess, cgram_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("CGRAM cgram", &scriptInterface.ppuAccess); assert(r >= 0);

  r = script.engine->RegisterObjectType    ("OAMSprite", sizeof(ScriptInterface::PPUAccess::OAMObject), asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
  r = script.engine->RegisterObjectMethod  ("OAMSprite", "bool   get_is_enabled()", asMETHOD(ScriptInterface::PPUAccess::OAMObject, get_is_enabled), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint16 x", asOFFSET(ScriptInterface::PPUAccess::OAMObject, x)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  y", asOFFSET(ScriptInterface::PPUAccess::OAMObject, y)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint16 character", asOFFSET(ScriptInterface::PPUAccess::OAMObject, character)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "bool   vflip", asOFFSET(ScriptInterface::PPUAccess::OAMObject, vflip)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "bool   hflip", asOFFSET(ScriptInterface::PPUAccess::OAMObject, hflip)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  priority", asOFFSET(ScriptInterface::PPUAccess::OAMObject, priority)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  palette", asOFFSET(ScriptInterface::PPUAccess::OAMObject, palette)); assert(r >= 0);
  r = script.engine->RegisterObjectProperty("OAMSprite", "uint8  size", asOFFSET(ScriptInterface::PPUAccess::OAMObject, size)); assert(r >= 0);
  r = script.engine->RegisterObjectMethod  ("OAMSprite", "uint8  get_width()", asMETHOD(ScriptInterface::PPUAccess::OAMObject, get_width), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod  ("OAMSprite", "uint8  get_height()", asMETHOD(ScriptInterface::PPUAccess::OAMObject, get_height), asCALL_THISCALL); assert(r >= 0);

  r = script.engine->RegisterObjectType  ("OAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "OAMSprite @get_opIndex(uint8 chr)", asFUNCTION(ScriptInterface::PPUAccess::oam_get_object), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("OAM", "void set_opIndex(uint8 chr, OAMSprite @sprite)", asFUNCTION(ScriptInterface::PPUAccess::oam_set_object), asCALL_CDECL_OBJFIRST); assert(r >= 0);

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
    r = script.engine->SetDefaultNamespace("net"); assert(r >= 0);

    // Address type:
    r = script.engine->RegisterObjectType("Address", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Address", asBEHAVE_FACTORY, "Address@ f(const string &in host, const int port)", asFUNCTION(ScriptInterface::Net::resolve), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectMethod("Address", "bool get_is_valid()", asMETHOD(ScriptInterface::Net::Address, operator bool), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Address", "bool throw_if_invalid()", asMETHOD(ScriptInterface::Net::Address, throw_if_invalid), asCALL_THISCALL); assert( r >= 0 );

    r = script.engine->RegisterObjectType("UDPSocket", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("UDPSocket", asBEHAVE_FACTORY, "UDPSocket@ f(const string &in host, const int port)", asFUNCTION(ScriptInterface::Net::create_udp_socket), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("UDPSocket", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::UDPSocket, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("UDPSocket", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::UDPSocket, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("UDPSocket", "int sendto(const array<uint8> &in msg, const string &in host, const int port)", asMETHOD(ScriptInterface::Net::UDPSocket, sendto), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("UDPSocket", "int recv(const array<uint8> &in msg)", asMETHOD(ScriptInterface::Net::UDPSocket, recv), asCALL_THISCALL); assert( r >= 0 );

    //r = script.engine->RegisterObjectType("WebSocketServer", 0, asOBJ_REF); assert(r >= 0);
    //r = script.engine->RegisterObjectBehaviour("WebSocketServer", asBEHAVE_FACTORY, "WebSocketServer@ f(const string &in listen_address, const int port)", asFUNCTION(ScriptInterface::Net::create_web_socket_server), asCALL_CDECL); assert(r >= 0);
    //r = script.engine->RegisterObjectBehaviour("WebSocketServer", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::WebSocketServer, addRef), asCALL_THISCALL); assert( r >= 0 );
    //r = script.engine->RegisterObjectBehaviour("WebSocketServer", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::WebSocketServer, release), asCALL_THISCALL); assert( r >= 0 );
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
    r = script.engine->RegisterObjectType("Canvas", 0, asOBJ_REF); assert(r >= 0);
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
    r = script.engine->RegisterObjectMethod("Window", "void append(Canvas @sizable)", asMETHOD(ScriptInterface::GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
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
    r = script.engine->RegisterObjectMethod("VerticalLayout", "void append(Canvas @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("VerticalLayout", "void resize()", asMETHOD(ScriptInterface::GUI::VerticalLayout, resize), asCALL_THISCALL); assert( r >= 0 );
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
    r = script.engine->RegisterObjectMethod("HorizontalLayout", "void append(Canvas @sizable, Size &in size)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("HorizontalLayout", "void resize()", asMETHOD(ScriptInterface::GUI::HorizontalLayout, resize), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("HorizontalLayout", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::HorizontalLayout, setVisible), asCALL_THISCALL); assert( r >= 0 );

    // LineEdit
    // Register a simple funcdef for the callback
    r = script.engine->RegisterFuncdef("void LineEditCallback(LineEdit @self)"); assert(r >= 0);
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
    r = script.engine->RegisterFuncdef("void ButtonCallback(Button @self)"); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Button", asBEHAVE_FACTORY, "Button@ f()", asFUNCTION(ScriptInterface::GUI::createButton), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Button", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::Button, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("Button", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::Button, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Button", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::Button, setVisible), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Button", "string &get_text()", asMETHOD(ScriptInterface::GUI::Button, getText), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Button", "void set_text(const string &in text)", asMETHOD(ScriptInterface::GUI::Button, setText), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Button", "void set_on_activate(ButtonCallback @cb)", asMETHOD(ScriptInterface::GUI::Button, setOnActivate), asCALL_THISCALL); assert( r >= 0 );

    // Canvas
    r = script.engine->RegisterObjectBehaviour("Canvas", asBEHAVE_FACTORY, "Canvas@ f()", asFUNCTION(ScriptInterface::GUI::createCanvas), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Canvas", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::GUI::Canvas, ref_add), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("Canvas", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::GUI::Canvas, ref_release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Canvas", "void set_visible(bool visible)", asMETHOD(ScriptInterface::GUI::Canvas, setVisible), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Canvas", "void set_size(Size &in size)", asMETHOD(ScriptInterface::GUI::Canvas, setSize), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Canvas", "void update()", asMETHOD(ScriptInterface::GUI::Canvas, update), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Canvas", "void fill(uint16 color)", asMETHOD(ScriptInterface::GUI::Canvas, fill), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Canvas", "void pixel(int x, int y, uint16 color)", asMETHOD(ScriptInterface::GUI::Canvas, pixel), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Canvas", "void draw_sprite_4bpp(int x, int y, uint c, uint width, uint height, const array<uint16> &in tiledata, const array<uint16> &in palette)", asMETHOD(ScriptInterface::GUI::Canvas, draw_sprite_4bpp), asCALL_THISCALL); assert( r >= 0 );
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
  // free any references to script callbacks:
  ::SuperFamicom::bus.reset_interceptors();
  ::SuperFamicom::cpu.reset_dma_interceptor();
  // TODO: GUI callbacks

  if (script.module) {
    script.module->Discard();
    script.module = nullptr;
  }

  script.funcs.init = nullptr;
  script.funcs.pre_frame = nullptr;
  script.funcs.post_frame = nullptr;
}
