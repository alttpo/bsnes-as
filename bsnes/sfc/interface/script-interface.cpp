
#include <sfc/sfc.hpp>

#if !defined(PLATFORM_WINDOWS)
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/ioctl.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
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

namespace ScriptInterface {
  struct ExceptionHandler {
    void exceptionCallback(asIScriptContext *ctx) {
      asIScriptEngine *engine = ctx->GetEngine();

      // Determine the exception that occurred
      const asIScriptFunction *function = ctx->GetExceptionFunction();
      printf(
        "EXCEPTION `%s` occurred in `%s` (line %d)\n",
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
  } exceptionHandler;

  static auto message(const string *msg) -> void {
    platform->scriptMessage(msg);
  }

  static auto array_to_string(CScriptArray *array, uint offs, uint size) -> string* {
    // avoid index out of range exception:
    if (array->GetSize() <= offs) return new string();
    if (array->GetSize() < offs + size) return new string();

    // append bytes from array:
    auto appended = new string();
    appended->resize(size);
    for (auto i : range(size)) {
      appended->get()[i] = *(const char *)(array->At(offs + i));
    }
    return appended;
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

  static void uint8_array_append_string(CScriptArray *array, string *other) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      return;
    }
    if (other == nullptr) {
      return;
    }

    for (auto& c : *other) {
      array->InsertLast((void *)&c);
    }
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
        cb->AddRef();
      }
      write_interceptor(const write_interceptor& other) : cb(other.cb), ctx(other.ctx) {
        cb->AddRef();
      }
      write_interceptor(const write_interceptor&& other) : cb(other.cb), ctx(other.ctx) {
      }
      ~write_interceptor() {
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
        cb->AddRef();
      }
      dma_interceptor(const dma_interceptor& other) : cb(other.cb), ctx(other.ctx) {
        cb->AddRef();
      }
      dma_interceptor(const dma_interceptor&& other) : cb(other.cb), ctx(other.ctx) {
      }
      ~dma_interceptor() {
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
    const uint16 *lightTable_lookup(uint8 luma) {
      return ppu.lightTable[luma];
    }

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
          // TODO: update cache for ppufast?
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

  namespace Net {
    int last_error = 0;
    int last_error_gai = 0;
    const char *last_error_location = nullptr;

    // socket error codes:
    const string *strEWOULDBLOCK = new string("EWOULDBLOCK");
    const string *strUnknown     = new string("EUNKNOWN");

    // getaddrinfo error codes:
    const string *strEAI_ADDRFAMILY = new string("EAI_ADDRFAMILY");
    const string *strEAI_AGAIN      = new string("EAI_AGAIN");
    const string *strEAI_BADFLAGS   = new string("EAI_BADFLAGS");
    const string *strEAI_FAIL       = new string("EAI_FAIL");
    const string *strEAI_FAMILY     = new string("EAI_FAMILY");
    const string *strEAI_MEMORY     = new string("EAI_MEMORY");
    const string *strEAI_NODATA     = new string("EAI_NODATA");
    const string *strEAI_NONAME     = new string("EAI_NONAME");
    const string *strEAI_SERVICE    = new string("EAI_SERVICE");
    const string *strEAI_SOCKTYPE   = new string("EAI_SOCKTYPE");
    const string *strEAI_SYSTEM     = new string("EAI_SYSTEM");
    const string *strEAI_BADHINTS   = new string("EAI_BADHINTS");
    const string *strEAI_PROTOCOL   = new string("EAI_PROTOCOL");
    const string *strEAI_OVERFLOW   = new string("EAI_OVERFLOW");

    static auto get_is_error() -> bool {
      return last_error != 0;
    }

    // get last error as a standardized code string:
    static auto get_error_code() -> const string* {
      if (last_error_gai != 0) {
        switch (last_error_gai) {
          case EAI_AGAIN: return strEAI_AGAIN;
          case EAI_BADFLAGS: return strEAI_BADFLAGS;
          case EAI_FAIL: return strEAI_FAIL;
          case EAI_FAMILY: return strEAI_FAMILY;
          case EAI_MEMORY: return strEAI_MEMORY;

          case EAI_NONAME: return strEAI_NONAME;
          case EAI_SERVICE: return strEAI_SERVICE;
          case EAI_SOCKTYPE: return strEAI_SOCKTYPE;
#if !defined(PLATFORM_WINDOWS)
          case EAI_SYSTEM: return strEAI_SYSTEM;
          case EAI_OVERFLOW: return strEAI_OVERFLOW;
#endif
#if !defined(PLATFORM_WINDOWS) && !defined(_POSIX_C_SOURCE)
          case EAI_ADDRFAMILY: return strEAI_ADDRFAMILY;
          case EAI_NODATA: return strEAI_NODATA;
          case EAI_BADHINTS: return strEAI_BADHINTS;
          case EAI_PROTOCOL: return strEAI_PROTOCOL;
#endif
          default: return strUnknown;
        }
      }

#if !defined(PLATFORM_WINDOWS)
      if (last_error == EWOULDBLOCK || last_error == EAGAIN) return strEWOULDBLOCK;
      //switch (last_error) {
      //}
#else
      switch (last_error) {
        case WSAEWOULDBLOCK: return strEWOULDBLOCK;
      }
#endif
      return strUnknown;
    }

    static auto get_error_text() -> const string* {
      return new string(sock_error_string(last_error));
    }

    static auto throw_if_error() -> bool {
      if (last_error_gai) {
        // throw script exception:
        asGetActiveContext()->SetException(string{last_error_location, ": ", gai_strerror(last_error_gai)}, true);
        last_error_gai = 0;
        return true;
      }
      if (last_error == 0) return false;

      // Don't throw exception for EWOULDBLOCK:
      if (last_error == EWOULDBLOCK || last_error == EAGAIN) {
        last_error = 0;
        last_error_location = "((FIXME: no location!))";
        return false;
      }

      // throw script exception:
      asGetActiveContext()->SetException(string{last_error_location, ": ", sock_error_string(last_error)}, true);
      last_error = 0;
      last_error_location = "((FIXME: no location!))";
      return true;
    }

    struct Address {
      addrinfo *info;

      // constructor:
      Address(const string *host, int port, int family = AF_UNSPEC, int socktype = SOCK_STREAM) {
        info = nullptr;

        addrinfo hints;
        const char *addr;

        memset(&hints, 0, sizeof(addrinfo));
        hints.ai_family = family;
        hints.ai_socktype = socktype;

        if (host == nullptr || (*host) == "") {
          addr = (const char *)nullptr;
          // AI_PASSIVE hint is ignored otherwise.
          hints.ai_flags = AI_PASSIVE;
        } else {
          addr = (const char *)*host;
        }

        last_error_gai = 0;
        last_error = 0;
        int rc = ::getaddrinfo(addr, string(port), &hints, &info); last_error_location = LOCATION " getaddrinfo";
        if (rc != 0) {
#if !defined(PLATFORM_WINDOWS)
          if (rc == EAI_SYSTEM) {
            last_error = sock_capture_error();
          } else
#endif
          {
            last_error_gai = rc;
          }
          throw_if_error();
        }
      }

      ~Address() {
        if (info) {
          ::freeaddrinfo(info);
          // TODO: error checking?
        }
        info = nullptr;
      }

      operator bool() { return info; }
    };

    static auto resolve_tcp(const string *host, int port) -> Address* {
      return new Address(host, port, AF_INET, SOCK_STREAM);
    }

    static auto resolve_udp(const string *host, int port) -> Address* {
      return new Address(host, port, AF_INET, SOCK_DGRAM);
    }

    struct Socket {
      int fd = -1;

      // already-created socket:
      Socket(int fd) : fd(fd) {
      }

      // create a new socket:
      Socket(int family, int type, int protocol) {
        ref = 1;

        // create the socket:
#if !defined(PLATFORM_WINDOWS)
        fd = ::socket(family, type, protocol); last_error_location = LOCATION " socket";
#else
        fd = ::WSASocket(family, type, protocol, nullptr, 0, 0); last_error_location = LOCATION " WSASocket";
#endif
        last_error = 0;
        if (fd < 0) {
          last_error = sock_capture_error();
          throw_if_error();
          return;
        }

        // enable reuse address:
        int rc = 0;
        int yes = 1;
#if !defined(PLATFORM_WINDOWS)
        rc = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)); last_error_location = LOCATION " setsockopt";
#else
        rc = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(int)); last_error_location = LOCATION " setsockopt";
#endif
        last_error = 0;
        if (rc < 0) {
          last_error = sock_capture_error();
          close(false);
          throw_if_error();
          return;
        }

        // enable TCP_NODELAY for SOCK_STREAM type sockets:
        if (type == SOCK_STREAM) {
        //if (true) {
#if !defined(PLATFORM_WINDOWS)
          rc = ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)); last_error_location = LOCATION " setsockopt";
#else
          rc = ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&yes, sizeof(int)); last_error_location = LOCATION " setsockopt";
#endif
          last_error = 0;
          if (rc < 0) {
            last_error = sock_capture_error();
            close(false);
            throw_if_error();
            return;
          }
        }

        // set non-blocking:
#if !defined(PLATFORM_WINDOWS)
        rc = ioctl(fd, FIONBIO, &yes); last_error_location = LOCATION " ioctl";
#else
        rc = ioctlsocket(fd, FIONBIO, (u_long *)&yes); last_error_location = LOCATION " ioctlsocket";
#endif
        last_error = 0;
        if (rc < 0) {
          last_error = sock_capture_error();
          close(false);
          throw_if_error();
          return;
        }
      }

      ~Socket() {
        if (operator bool()) {
          close();
        }

        fd = -1;
      }

      int ref;
      void addRef() {
        ref++;
      }
      void release() {
        if (--ref == 0)
          delete this;
      }

      auto close(bool set_last_error = true) -> void {
        int rc;
#if !defined(PLATFORM_WINDOWS)
        rc = ::close(fd); set_last_error && (last_error_location = LOCATION " close");
#else
        rc = ::closesocket(fd); set_last_error && (last_error_location = LOCATION " closesocket");
#endif
        if (set_last_error) {
          last_error = 0;
          if (rc < 0) {
            last_error = sock_capture_error();
            throw_if_error();
          }
        }

        fd = -1;
      }

      operator bool() { return fd >= 0; }

      // indicates data is ready to be read:
      bool ready_in = false;
      auto set_ready_in(bool value) -> void {
        ready_in = value;
      }

      // indicates data is ready to be written:
      bool ready_out = false;
      auto set_ready_out(bool value) -> void {
        ready_out = value;
      }

      // bind to an address:
      auto bind(const Address *addr) -> int {
        int rc = ::bind(fd, addr->info->ai_addr, addr->info->ai_addrlen); last_error_location = LOCATION " bind";
        last_error = 0;
        if (rc < 0) {
          last_error = sock_capture_error();
          throw_if_error();
        }
        return rc;
      }

      // start listening for connections:
      auto listen(int backlog = 32) -> int {
        int rc = ::listen(fd, backlog); last_error_location = LOCATION " listen";
        last_error = 0;
        if (rc < 0) {
          last_error = sock_capture_error();
          throw_if_error();
        }
        return rc;
      }

      // accept a connection:
      auto accept() -> Socket* {
        // non-blocking sockets don't require a poll() because accept() handles non-blocking scenario itself.

        // accept incoming connection, discard client address:
        int afd = ::accept(fd, nullptr, nullptr); last_error_location = LOCATION " accept";
        last_error = 0;
        if (afd < 0) {
          last_error = sock_capture_error();
          throw_if_error();
          return nullptr;
        }

        return new Socket(afd);
      }

      // attempt to receive data:
      auto recv(int offs, int size, CScriptArray* buffer) -> int {
#if !defined(PLATFORM_WINDOWS)
        int rc = ::recv(fd, buffer->At(offs), size, 0); last_error_location = LOCATION " recv";
#else
        int rc = ::recv(fd, (char *)buffer->At(offs), size, 0); last_error_location = LOCATION " recv";
#endif
        last_error = 0;
        if (rc == -1) {
          last_error = sock_capture_error();
          throw_if_error();
        }
        return rc;
      }

      // attempt to send data:
      auto send(int offs, int size, CScriptArray* buffer) -> int {
#if !defined(PLATFORM_WINDOWS)
        int rc = ::send(fd, buffer->At(offs), size, 0); last_error_location = LOCATION " send";
#else
        int rc = ::send(fd, (const char*)buffer->At(offs), size, 0); last_error_location = LOCATION " send";
#endif
        last_error = 0;
        if (rc == -1) {
          last_error = sock_capture_error();
          throw_if_error();
        }
        return rc;
      }

      // TODO: expose this to scripts somehow.
      struct sockaddr_storage recvaddr;
      socklen_t recvaddrlen;

      // attempt to receive data and record address of sender (e.g. for UDP):
      auto recvfrom(int offs, int size, CScriptArray* buffer) -> int {
#if !defined(PLATFORM_WINDOWS)
        int rc = ::recvfrom(fd, buffer->At(offs), size, 0, (struct sockaddr *)&recvaddr, &recvaddrlen); last_error_location = LOCATION " recvfrom";
#else
        int rc = ::recvfrom(fd, (char *)buffer->At(offs), size, 0, (struct sockaddr *)&recvaddr, &recvaddrlen); last_error_location = LOCATION " recvfrom";
#endif
        last_error = 0;
        if (rc == -1) {
          last_error = sock_capture_error();
          throw_if_error();
        }
        return rc;
      }

      // attempt to send data to specific address (e.g. for UDP):
      auto sendto(int offs, int size, CScriptArray* buffer, const Address* addr) -> int {
#if !defined(PLATFORM_WINDOWS)
        int rc = ::sendto(fd, buffer->At(offs), size, 0, addr->info->ai_addr, addr->info->ai_addrlen); last_error_location = LOCATION " sendto";
#else
        int rc = ::sendto(fd, (const char *)buffer->At(offs), size, 0, addr->info->ai_addr, addr->info->ai_addrlen); last_error_location = LOCATION " sendto";
#endif
        last_error = 0;
        if (rc == -1) {
          last_error = sock_capture_error();
          throw_if_error();
        }
        return rc;
      }

      auto recv_append(string &s) -> uint64_t {
        uint8_t rawbuf[4096];
        uint64_t total = 0;

        for (;;) {
#if !defined(PLATFORM_WINDOWS)
          int rc = ::recv(fd, rawbuf, 4096, 0); last_error_location = LOCATION " recv";
#else
          int rc = ::recv(fd, (char *)rawbuf, 4096, 0); last_error_location = LOCATION " recv";
#endif
          last_error = 0;
          if (rc < 0) {
            last_error = sock_capture_error();
            if (last_error == EWOULDBLOCK || last_error == EAGAIN) {
              last_error = 0;
              return total;
            }
            throw_if_error();
            return total;
          }
          if (rc == 0) {
            // remote peer closed the connection
            close();
            return total;
          }

          // append to string:
          uint64_t to = s.size();
          s.resize(s.size() + rc);
          memory::copy(s.get() + to, rawbuf, rc);
          total += rc;
        }
      }

      auto recv_buffer(vector<uint8_t>& buffer) -> uint64_t {
        uint8_t rawbuf[4096];
        uint64_t total = 0;

        for (;;) {
#if !defined(PLATFORM_WINDOWS)
          int rc = ::recv(fd, rawbuf, 4096, 0); last_error_location = LOCATION " recv";
#else
          int rc = ::recv(fd, (char *)rawbuf, 4096, 0); last_error_location = LOCATION " recv";
#endif
          last_error = 0;
          if (rc < 0) {
            last_error = sock_capture_error();
            if (last_error == EWOULDBLOCK || last_error == EAGAIN) {
              last_error = 0;
              return total;
            }
            throw_if_error();
            return total;
          }
          if (rc == 0) {
            close();
            return total;
          }

          // append to buffer:
          buffer.appends({rawbuf, (uint)rc});
          total += rc;
        }
      }

      auto send_buffer(array_view<uint8_t> buffer) -> int {
#if !defined(PLATFORM_WINDOWS)
        int rc = ::send(fd, buffer.data(), buffer.size(), 0); last_error_location = LOCATION " send";
#else
        int rc = ::send(fd, (const char *)buffer.data(), buffer.size(), 0); last_error_location = LOCATION " send";
#endif
        last_error = 0;
        if (rc == 0) {
          close();
          return rc;
        }
        if (rc < 0) {
          last_error = sock_capture_error();
          throw_if_error();
        }
        return rc;
      }
    };

    static auto create_socket(Address *addr) -> Socket* {
      return new Socket(addr->info->ai_family, addr->info->ai_socktype, addr->info->ai_protocol);
    }

#if 0
    // we don't need poll() for non-blocking sockets:
    static auto poll_in(CScriptArray *sockets) -> int {
      const char *err_location;
      struct pollfd fds[200];
      int    nfds = 0;

      // transform array<Socket@> from script into pollfd[] for poll() call:
      memset(fds, 0, sizeof(fds));
      auto len = sockets->GetSize();
      for (int i = 0; i < 200 && i < len; i++) {
        auto socket = static_cast<const Socket*>(sockets->At(i));
        fds[nfds].fd = socket->fd;
        fds[nfds].events = POLLIN;
        nfds++;
      }

      // poll for read availability on all sockets:
      int rc = ::poll(fds, nfds, 0); last_error_location = LOCATION " poll";
      last_error = sock_capture_error();
      //printf("poll(fds, nfds=%d, 0) result=%d\n", nfds, rc);
      if (rc <= 0) {
        return false;
      }

      for (int i = 0; i < 200 && i < len; i++) {
        auto socket = static_cast<Socket *>(sockets->At(i));
        if (socket == nullptr) {
          continue;
        }

        printf("fds[%d].revents = %d\n", i, fds[i].revents);
        if (fds[i].revents & POLLIN) {
          socket->set_ready_in(true);
          //printf("fds[%d].ready_in = true\n", i);
        }
        if (fds[i].revents & POLLOUT) {
          socket->set_ready_out(true);
          //printf("fds[%d].ready_out = true\n", i);
        }
      }

      return rc;
    }
#endif

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

    struct WebSocketMessage {
      uint8           opcode;
      vector<uint8_t> bytes;

      WebSocketMessage(uint8 opcode) : opcode(opcode)
      {
        ref = 1;
      }
      ~WebSocketMessage() = default;

      int ref;
      void addRef() {
        ref++;
      }
      void release() {
        if (--ref == 0)
          delete this;
      }

      auto get_opcode() -> uint8 { return opcode; }
      auto set_opcode(uint8 value) -> void { opcode = value; }

      auto as_string() -> string* {
        auto s = new string();
        s->resize(bytes.size());
        memory::copy(s->get(), bytes.data(), bytes.size());
        return s;
      }

      auto as_array() -> CScriptArray* {
        asIScriptContext *ctx = asGetActiveContext();
        if (!ctx) return nullptr;

        asIScriptEngine* engine = ctx->GetEngine();
        asITypeInfo* t = engine->GetTypeInfoByDecl("array<uint8>");

        uint64_t size = bytes.size();
        auto a = CScriptArray::Create(t, size);
        if (size > 0) {
          memory::copy(a->At(0), bytes.data(), size);
        }
        return a;
      }

      auto set_payload_as_string(string *s) -> void {
        bytes.resize(s->size());
        memory::copy(bytes.data(), s->data(), s->size());
      }

      auto set_payload_as_array(CScriptArray *a) -> void {
        bytes.resize(a->GetSize());
        memory::copy(bytes.data(), a->At(0), a->GetSize());
      }
    };

    auto create_web_socket_message(uint8 opcode) -> WebSocketMessage* {
      return new WebSocketMessage(opcode);
    }

    struct WebSocket {
      Socket* socket;

      WebSocket(Socket* socket) : socket(socket) {
        socket->addRef();
        ref = 1;
      }
      ~WebSocket() {
        socket->release();
      }

      int ref;
      void addRef() {
        ref++;
      }
      void release() {
        if (--ref == 0)
          delete this;
      }

      operator bool() { return *socket; }

      vector<uint8_t> frame;
      WebSocketMessage *message = nullptr;

      // attempt to receive data:
      auto process() -> WebSocketMessage* {
        if (!*socket) {
          // TODO: throw exception if socket closed?
          printf("[%d] invalid socket!\n", socket->fd);
          return nullptr;
        }

        // Receive data and append to current frame:
        auto rc = socket->recv_buffer(frame);
        // No data ready to recv:
        if (rc == 0) {
          printf("[%d] no data\n", socket->fd);
          return nullptr;
        }

        // check start of frame:
        uint minsize = 2;
        if (frame.size() < minsize) {
          printf("[%d] frame too small (%d) to read opcode and payload length bytes (%d)\n", socket->fd, frame.size(), minsize);
          return nullptr;
        }

        uint i = 0;

        bool fin = frame[i] & 0x80;
        // 3 other reserved bits here
        uint8_t opcode = frame[i] & 0x0F;
        i++;

        bool mask = frame[i] & 0x80;
        uint64_t len = frame[i] & 0x7F;
        i++;

        // determine minimum size of frame to parse:
        if (len == 126) minsize += 2;
        else if (len == 127) minsize += 8;

        // need more data?
        if (frame.size() < minsize) {
          printf("[%d] frame too small (%d) to read additional payload length bytes (%d)\n", socket->fd, frame.size(), minsize);
          return nullptr;
        }

        if (len == 126) {
          // len is 16-bit
          len = 0;
          for (int j = 0; j < 2; j++) {
            len <<= 8u;
            len |= frame[i++];
          }
        } else if (len == 127) {
          // len is 64-bit
          len = 0;
          for (int j = 0; j < 8; j++) {
            len <<= 8u;
            len |= frame[i++];
          }
        }

        uint8_t mask_key[4] = {0};

        if (mask) {
          minsize += 4;
          if (frame.size() < minsize) {
            printf("[%d] frame too small (%d) to read mask key bytes (%d)\n", socket->fd, frame.size(), minsize);
            return nullptr;
          }

          // read mask key:
          for (unsigned char &mask_byte : mask_key) {
            mask_byte = frame[i++];
          }
          //printf("mask_key is %02x%02x%02x%02x\n", mask_key[0], mask_key[1], mask_key[2], mask_key[3]);
        } else {
          //printf("frame is not masked!\n");
          asGetActiveContext()->SetException("WebSocket frame received from client must be masked", true);
          return nullptr;
        }

        // not enough data in frame yet?
        if (frame.size() - i < len) {
          printf("[%d] frame too small (%d) to read full payload (%d)\n", socket->fd, frame.size(), i + len);
          return nullptr;
        }

        // For any opcode but 0 (continuation), start a new message:
        if (opcode != 0) {
          //printf("new message with opcode = %d\n", opcode);
          message = new WebSocketMessage(opcode);
        }

        // append payload data to message buffer:
        auto payload = frame.view(i, len);
        if (mask) {
          // Unmask the payload with the mask_key and append to message:
          //printf("append masked payload of size %d\n", payload.size());
          uint k = 0;
          for (uint8_t byte : payload) {
            message->bytes.append(byte ^ mask_key[k]);
            k = (k + 1) & 3;
          }
        } else {
          // Append unmasked payload:
          //printf("append unmasked payload of size %d\n", payload.size());
          message->bytes.appends(payload);
        }

        // clear out already-read data:
        frame.removeLeft(i + len);

        // if no FIN flag set, wait for more frames:
        if (!fin) {
          printf("[%d] FIN not set\n", socket->fd);
          return nullptr;
        }

        // return final message:
        printf("[%d] FIN set; returning message\n", socket->fd);
        auto tmp = message;
        message = nullptr;
        return tmp;
      }

      // send a message:
      auto send(WebSocketMessage* msg) -> void {
        vector<uint8_t> outframe;

        uint64_t len = msg->bytes.size();
        bool fin = true;

        outframe.append((fin << 7) | (msg->opcode & 0x0F));

        bool masked = false;
        if (len > 65536) {
          outframe.append((masked << 7) | 127);
          // send big-endian 64-bit payload length:
          for (int j = 0; j < 8; j++) {
            outframe.append((len >> (7-j)*8) & 0xFF);
          }
        } else if (len > 125) {
          outframe.append((masked << 7) | 126);
          // send big-endian 16-bit payload length:
          for (int j = 0; j < 2; j++) {
            outframe.append((len >> (1-j)*8) & 0xFF);
          }
        } else {
          // 7-bit (ish) length:
          outframe.append((masked << 7) | (len & 0x7F));
        }

        uint8_t mask_key[4] = {0};
        if (masked) {
          // TODO: build and send mask_key
        }

        // append payload to frame:
        outframe.appends(msg->bytes);

        // try to send frame:
        int rc = socket->send_buffer(outframe);
        if (rc == 0) {
          // remote peer closed the connection:
          return;
        }
        if (rc < 0) {
          // TODO: maintain sending state machine
          printf("[%d] send() error!\n", socket->fd);
          return;
        }
        if (rc < outframe.size()) {
          printf("[%d] send() failed to send entire frame!\n", socket->fd);
          return;
        }
      }
    };

    // accepts incoming GET requests with websocket upgrade headers and completes the handshake, finally
    // passing the socket over to WebSocket which handles regular websocket data framing protocol.
    struct WebSocketHandshaker {
      Socket* socket = nullptr;

      string request;
      vector<string> request_lines;

      string ws_key;

      enum {
        EXPECT_GET_REQUEST = 0,
        PARSE_GET_REQUEST,
        SEND_HANDSHAKE,
        OPEN,
        CLOSED
      } state;

      WebSocketHandshaker(Socket* socket) : socket(socket) {
        state = EXPECT_GET_REQUEST;
        ref = 1;
        socket->addRef();
      }
      ~WebSocketHandshaker() {
        reset();
        socket->release();
      }

      int ref;
      void addRef() {
        ref++;
      }
      void release() {
        if (--ref == 0)
          delete this;
      }

      operator bool() { return *socket; }

      auto get_socket() -> Socket* {
        return socket;
      }

      auto reset() -> void {
        request.reset();
        request_lines.reset();
        ws_key.reset();
      }

      auto base64_decode(const string& text) -> vector<uint8_t> {
        static const unsigned char pr2six[256] =
          {
            /* ASCII table */
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
            52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
            64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
            64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
            41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
          };

        vector<uint8_t> out;

        int nbytesdecoded;
        const unsigned char *bufin;
        int nprbytes;

        bufin = (const unsigned char *) text.data();
        while (pr2six[*(bufin++)] <= 63);
        nprbytes = (bufin - (const unsigned char *) text.data()) - 1;
        nbytesdecoded = ((nprbytes + 3) / 4) * 3;

        bufin = (const unsigned char *) text.data();

        while (nprbytes > 4) {
          out.append((unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4));
          out.append((unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2));
          out.append((unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]));
          bufin += 4;
          nprbytes -= 4;
        }

        /* Note: (nprbytes == 1) would be an error, so just ingore that case */
        if (nprbytes > 1) {
          out.append((unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4));
        }
        if (nprbytes > 2) {
          out.append((unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2));
        }
        if (nprbytes > 3) {
          out.append((unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]));
        }

        nbytesdecoded -= (4 - nprbytes) & 3;
        out.resize(nbytesdecoded);

        return out;
      }

      auto handshake() -> WebSocket* {
        if (state == CLOSED) {
          return nullptr;
        }

        if (state == EXPECT_GET_REQUEST) {
          // build up GET request from client:
          if (socket->recv_append(request) == 0) {
            //printf("socket closed!\n");
            reset();
            state = CLOSED;
            socket->close(false);
            return nullptr;
          }

          auto s = request.size();
          if (s >= 65536) {
            // too big, toss out.
            //printf("request too big!\n");
            reset();
            state = EXPECT_GET_REQUEST;
            return nullptr;
          }

          // Find the end of the request headers:
          auto found = request.find("\r\n\r\n");
          if (found) {
            auto length = found.get();
            request_lines = slice(request, 0, length).split("\r\n");
            state = PARSE_GET_REQUEST;
          }
        }

        if (state == PARSE_GET_REQUEST) {
          string response = "HTTP/1.1 400 Bad Request\r\n\r\n";

          // parse HTTP request:
          vector<string> headers;
          vector<string> line;
          int len;

          bool req_host = false;
          bool req_connection = false;
          bool req_upgrade = false;
          bool req_ws_key = false;
          bool req_ws_version = false;

          // if no lines in request, bail:
          if (request_lines.size() == 0) {
            response = "HTTP/1.1 400 Bad Request\r\n\r\nMissing HTTP request line";
            goto bad_request;
          }

          // check first line is like `GET ... HTTP/1.1`:
          line = request_lines[0].split(" ");
          if (line.size() != 3) {
            response = "HTTP/1.1 400 Bad Request\r\n\r\nInvalid HTTP request line; must be `[method] [resource] HTTP/1.1`";
            goto bad_request;
          }
          // really want to check if version >= 1.1
          if (line[2] != "HTTP/1.1") {
            response = "HTTP/1.1 400 Bad Request\r\n\r\nInvalid HTTP request line; must be HTTP/1.1 version";
            goto bad_request;
          }
          if (line[0] != "GET") {
            response = "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET\r\n\r\n";
            goto bad_request;
          }

          // check all headers for websocket upgrade requirements:
          len = request_lines.size();
          for (int i = 1; i < len; i++) {
            auto split = request_lines[i].split(":", 1);
            auto header = split[0].downcase().strip();
            auto value = split[1].strip();

            if (header == "host") {
              req_host = true;
            } else if (header == "upgrade") {
              if (!value.contains("websocket")) {
                response = "HTTP/1.1 400 Bad Request\r\n\r\nUpgrade header must be 'websocket'";
                goto bad_request;
              }
              req_upgrade = true;
            } else if (header == "connection") {
              if (!value.contains("Upgrade")) {
                response = "HTTP/1.1 400 Bad Request\r\n\r\nConnection header must be 'Upgrade'";
                goto bad_request;
              }
              req_connection = true;
            } else if (header == "sec-websocket-key") {
              ws_key = value;
              // We don't _need_ to do this but it's nice to check:
              auto decoded = base64_decode(ws_key);
              if (decoded.size() != 16) {
                response = "HTTP/1.1 400 Bad Request\r\n\r\nSec-WebSocket-Key header must base64 decode to 16 bytes";
                goto bad_request;
              }
              req_ws_key = true;
            } else if (header == "sec-websocket-version") {
              if (value != "13") {
                response = "HTTP/1.1 426 Upgrade Required\r\nSec-WebSocket-Version: 13\r\n\r\nSec-WebSocket-Version header must be '13'";
                goto response_sent;
              }
              req_ws_version = true;
            }
          }

          // make sure we have the minimal set of HTTP headers for upgrading to websocket:
          if (!req_host || !req_upgrade || !req_connection || !req_ws_key || !req_ws_version) {
            vector<string> missing;
            if (!req_host) missing.append("Host");
            if (!req_upgrade) missing.append("Upgrade");
            if (!req_connection) missing.append("Connection");
            if (!req_ws_key) missing.append("Sec-WebSocket-Key");
            if (!req_ws_version) missing.append("Sec-WebSocket-Version");
            response = string{
              string("HTTP/1.1 426 Upgrade Required\r\n"
                     "Sec-WebSocket-Version: 13\r\n\r\n"
                     "Missing one or more required HTTP headers for WebSocket upgrade: "),
              missing.merge(", ")
            };
            goto bad_request;
          }

          // we have all the required headers and they are valid:
          goto next_state;

        bad_request:
          socket->send_buffer(response);
        response_sent:
          reset();
          state = EXPECT_GET_REQUEST;
          return nullptr;

        next_state:
          state = SEND_HANDSHAKE;
        }

        if (state == SEND_HANDSHAKE) {
          auto concat = string{ws_key, string("258EAFA5-E914-47DA-95CA-C5AB0DC85B11")};
          auto sha1 = nall::Hash::SHA1(concat).output();
          auto enc = nall::Encode::Base64(sha1);
          auto buf = string{
            string(
              "HTTP/1.1 101 Switching Protocols\r\n"
              "Connection: Upgrade\r\n"
              "Upgrade: websocket\r\n"
              "Sec-WebSocket-Accept: "
            ),
            // base64 encoded sha1 hash here
            enc,
            string("\r\n\r\n")
          };
          socket->send_buffer(buf);

          state = OPEN;
          return new WebSocket(socket);
        }

        return nullptr;
      }
    };

    static WebSocketHandshaker *create_web_socket_handshaker(Socket* socket) {
      return new WebSocketHandshaker(socket);
    }

    struct WebSocketServer {
      Socket* socket = nullptr;
      string host;
      int port;
      string resource;

      bool valid;

      WebSocketServer(string uri) {
        ref = 1;
        valid = false;

        // make sure we have a script context:
        asIScriptContext *ctx = asGetActiveContext();
        if (!ctx) {
          printf("WebSocketServer requires a current script context\n");
          return;
        }
        asIScriptEngine* engine = ctx->GetEngine();
        if (!engine) {
          printf("WebSocketServer requires a current script engine instance\n");
          return;
        }
        asITypeInfo* typeInfo = engine->GetTypeInfoByDecl("array<net::WebSocket@>");
        if (!typeInfo) {
          printf("WebSocketServer could not find script TypeInfo for `array<net::WebSocket@>`\n");
          return;
        }

        // use a default uri:
        if (!uri) uri = "ws://localhost:8080";

        // Split the absolute URI `ws://host:port/path.../path...` into `ws:` and `host:port/path.../path...`
        auto scheme_rest = uri.split("//", 1);
        if (scheme_rest.size() == 0) {
          asGetActiveContext()->SetException("uri must be an absolute URI");
          return;
        }
        string scheme = scheme_rest[0];
        if (scheme != "ws:") {
          asGetActiveContext()->SetException("uri scheme must be `ws:`");
          return;
        }

        // Split the remaining `host:port/path.../path...` by the first '/':
        auto rest = scheme_rest[1];
        auto host_port_path = rest.split("/", 1);
        if (host_port_path.size() == 0) host_port_path.append(rest);

        // TODO: accommodate IPv6 addresses (e.g. `[::1]`)
        host = host_port_path[0];
        auto host_port = host_port_path[0].split(":");
        if (host_port.size() == 2) {
          host = host_port[0];
          port = host_port[1].integer();
        } else {
          port = 80;
        }

        resource = "/";
        if (host_port_path.size() == 2) {
          resource = string{"/", host_port_path[1]};
        }

        // resolve listening address:
        auto addr = resolve_tcp(&host, port);
        if (!*addr) {
          delete addr;
          return;
        }

        // create listening socket to accept TCP connections on:
        socket = create_socket(addr);
        if (!*socket) return;

        // start listening for connections:
        if (socket->bind(addr) < 0) return;
        if (socket->listen(32) < 0) return;

        // create `array<WebSocket@>` for clients:
        clients = CScriptArray::Create(typeInfo);
        valid = true;
      }
      ~WebSocketServer() {
        if (socket) delete socket;
      }

      int ref;
      void addRef() {
        ref++;
      }
      void release() {
        if (--ref == 0)
          delete this;
      }

      operator bool() { return valid; }

      vector<WebSocketHandshaker*> handshakers;
      CScriptArray* clients;

      auto process() -> int {
        int count = 0;

        // accept new connections:
        Socket* accepted = nullptr;
        while ((accepted = socket->accept()) != nullptr) {
          handshakers.append(new WebSocketHandshaker(accepted));
        }

        // handle requests for each open connection:
        for (auto it = handshakers.rbegin(); it != handshakers.rend(); ++it) {
          // if socket closed, remove it:
          if (!*(*it)) {
            handshakers.removeByIndex(it.offset());
            continue;
          }

          // advance the websocket handshake / upgrade process from HTTP requests to WebSockets:
          WebSocket* ws = nullptr;
          // TODO: accept only resource path
          // TODO: if allowing multiple resources, need a shared instance per host:port
          if ((ws = (*it)->handshake()) != nullptr) {
            clients->InsertLast(&ws);
            handshakers.removeByIndex(it.offset());
            count++;
          }
        }

        return count;
      }

      auto get_clients() -> CScriptArray* { return clients; }
    };

    auto create_web_socket_server(string *uri) -> WebSocketServer* {
      return new WebSocketServer(*uri);
    }
  }

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

auto Interface::registerScriptDefs() -> void {
  int r;

  script.engine = platform->scriptEngine();

  // register global functions for the script to use:
  auto defaultNamespace = script.engine->GetDefaultNamespace();

  // register array type:
  RegisterScriptArray(script.engine, true /* bool defaultArrayType */);

  // register string type:
  registerScriptString();

  // additional array functions for serialization purposes:
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const uint16 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint16), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const uint32 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint32), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const string &in other)", asFUNCTION(ScriptInterface::uint8_array_append_string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const ? &in other)", asFUNCTION(ScriptInterface::uint8_array_append_array), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  r = script.engine->RegisterObjectMethod("array<T>", "string &toString(uint offs, uint size) const", asFUNCTION(ScriptInterface::array_to_string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

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
  r = script.engine->RegisterGlobalProperty("VRAM vram", &ScriptInterface::ppuAccess); assert(r >= 0);

  // define ppu::CGRAM object type for opIndex convenience:
  r = script.engine->RegisterObjectType("CGRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("CGRAM", "uint16 opIndex(uint16 addr)", asMETHOD(ScriptInterface::PPUAccess, cgram_read), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterGlobalProperty("CGRAM cgram", &ScriptInterface::ppuAccess); assert(r >= 0);

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

  r = script.engine->RegisterGlobalProperty("OAM oam", &ScriptInterface::ppuAccess); assert(r >= 0);

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
    r = script.engine->RegisterGlobalProperty("Frame frame", &ScriptInterface::postFrame); assert(r >= 0);
  }

  {
    r = script.engine->SetDefaultNamespace("net"); assert(r >= 0);

    // error reporting functions:
    r = script.engine->RegisterGlobalFunction("string get_is_error()", asFUNCTION(ScriptInterface::Net::get_is_error), asCALL_CDECL); assert( r >= 0 );
    r = script.engine->RegisterGlobalFunction("string get_error_code()", asFUNCTION(ScriptInterface::Net::get_error_code), asCALL_CDECL); assert( r >= 0 );
    r = script.engine->RegisterGlobalFunction("string get_error_text()", asFUNCTION(ScriptInterface::Net::get_error_text), asCALL_CDECL); assert( r >= 0 );
    r = script.engine->RegisterGlobalFunction("bool throw_if_error()", asFUNCTION(ScriptInterface::Net::throw_if_error), asCALL_CDECL); assert( r >= 0 );

    // Address type:
    r = script.engine->RegisterObjectType("Address", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("Address@ resolve_tcp(const string &in host, const int port)", asFUNCTION(ScriptInterface::Net::resolve_tcp), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterGlobalFunction("Address@ resolve_udp(const string &in host, const int port)", asFUNCTION(ScriptInterface::Net::resolve_udp), asCALL_CDECL); assert(r >= 0);
    // TODO: try opImplCast
    r = script.engine->RegisterObjectMethod("Address", "bool get_is_valid()", asMETHOD(ScriptInterface::Net::Address, operator bool), asCALL_THISCALL); assert( r >= 0 );

    r = script.engine->RegisterObjectType("Socket", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Socket", asBEHAVE_FACTORY, "Socket@ f(Address @addr)", asFUNCTION(ScriptInterface::Net::create_socket), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("Socket", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::Socket, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("Socket", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::Socket, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "bool get_is_valid()", asMETHOD(ScriptInterface::Net::Socket, operator bool), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "void bind(Address@ addr)", asMETHOD(ScriptInterface::Net::Socket, bind), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "void listen(int backlog)", asMETHOD(ScriptInterface::Net::Socket, listen), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "Socket@ accept()", asMETHOD(ScriptInterface::Net::Socket, accept), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "int recv(int offs, int size, array<uint8> &inout buffer)", asMETHOD(ScriptInterface::Net::Socket, recv), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "int send(int offs, int size, array<uint8> &inout buffer)", asMETHOD(ScriptInterface::Net::Socket, send), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "int recvfrom(int offs, int size, array<uint8> &inout buffer)", asMETHOD(ScriptInterface::Net::Socket, recvfrom), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("Socket", "int sendto(int offs, int size, array<uint8> &inout buffer, const Address@ addr)", asMETHOD(ScriptInterface::Net::Socket, sendto), asCALL_THISCALL); assert( r >= 0 );

#if 0
    // we don't need poll() for non-blocking sockets:
    r = script.engine->RegisterGlobalFunction("bool poll_in(const array<Socket@> &in sockets)", asFUNCTION(ScriptInterface::Net::poll_in), asCALL_CDECL);
#endif

    // to be deprecated:
    r = script.engine->RegisterObjectType("UDPSocket", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("UDPSocket", asBEHAVE_FACTORY, "UDPSocket@ f(const string &in host, const int port)", asFUNCTION(ScriptInterface::Net::create_udp_socket), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("UDPSocket", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::UDPSocket, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("UDPSocket", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::UDPSocket, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("UDPSocket", "int sendto(const array<uint8> &in msg, const string &in host, const int port)", asMETHOD(ScriptInterface::Net::UDPSocket, sendto), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("UDPSocket", "int recv(const array<uint8> &in msg)", asMETHOD(ScriptInterface::Net::UDPSocket, recv), asCALL_THISCALL); assert( r >= 0 );

    r = script.engine->RegisterObjectType("WebSocketMessage", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocketMessage", asBEHAVE_FACTORY, "WebSocketMessage@ f(uint8 opcode)", asFUNCTION(ScriptInterface::Net::create_web_socket_message), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocketMessage", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::WebSocketMessage, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("WebSocketMessage", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::WebSocketMessage, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketMessage", "uint8 get_opcode()", asMETHOD(ScriptInterface::Net::WebSocketMessage, get_opcode), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketMessage", "void set_opcode(uint8 value)", asMETHOD(ScriptInterface::Net::WebSocketMessage, set_opcode), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketMessage", "string &as_string()", asMETHOD(ScriptInterface::Net::WebSocketMessage, as_string), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketMessage", "array<uint8> &as_array()", asMETHOD(ScriptInterface::Net::WebSocketMessage, as_array), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketMessage", "void set_payload_as_string(string &in value)", asMETHOD(ScriptInterface::Net::WebSocketMessage, set_payload_as_string), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketMessage", "void set_payload_as_array(array<uint8> &in value)", asMETHOD(ScriptInterface::Net::WebSocketMessage, set_payload_as_array), asCALL_THISCALL); assert( r >= 0 );

    r = script.engine->RegisterObjectType("WebSocket", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocket", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::WebSocket, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("WebSocket", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::WebSocket, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocket", "WebSocketMessage@ process()", asMETHOD(ScriptInterface::Net::WebSocket, process), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocket", "void send(WebSocketMessage@ msg)", asMETHOD(ScriptInterface::Net::WebSocket, send), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocket", "bool get_is_valid()", asMETHOD(ScriptInterface::Net::WebSocket, operator bool), asCALL_THISCALL); assert( r >= 0 );

    r = script.engine->RegisterObjectType("WebSocketHandshaker", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocketHandshaker", asBEHAVE_FACTORY, "WebSocketHandshaker@ f(Socket@ socket)", asFUNCTION(ScriptInterface::Net::create_web_socket_handshaker), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocketHandshaker", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::WebSocketHandshaker, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("WebSocketHandshaker", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::WebSocketHandshaker, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketHandshaker", "Socket@ get_socket()", asMETHOD(ScriptInterface::Net::WebSocketHandshaker, get_socket), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketHandshaker", "WebSocket@ handshake()", asMETHOD(ScriptInterface::Net::WebSocketHandshaker, handshake), asCALL_THISCALL); assert( r >= 0 );    r = script.engine->RegisterObjectType("WebSocketHandshaker", 0, asOBJ_REF); assert(r >= 0);

    r = script.engine->RegisterObjectType("WebSocketServer", 0, asOBJ_REF); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocketServer", asBEHAVE_FACTORY, "WebSocketServer@ f(string &in uri)", asFUNCTION(ScriptInterface::Net::create_web_socket_server), asCALL_CDECL); assert(r >= 0);
    r = script.engine->RegisterObjectBehaviour("WebSocketServer", asBEHAVE_ADDREF, "void f()", asMETHOD(ScriptInterface::Net::WebSocketServer, addRef), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectBehaviour("WebSocketServer", asBEHAVE_RELEASE, "void f()", asMETHOD(ScriptInterface::Net::WebSocketServer, release), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketServer", "array<WebSocket@> &get_clients()", asMETHOD(ScriptInterface::Net::WebSocketServer, get_clients), asCALL_THISCALL); assert( r >= 0 );
    r = script.engine->RegisterObjectMethod("WebSocketServer", "int process()", asMETHOD(ScriptInterface::Net::WebSocketServer, process), asCALL_THISCALL); assert( r >= 0 );
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

  script.context->SetExceptionCallback(asMETHOD(ScriptInterface::ExceptionHandler, exceptionCallback), &ScriptInterface::exceptionHandler, asCALL_THISCALL);
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
