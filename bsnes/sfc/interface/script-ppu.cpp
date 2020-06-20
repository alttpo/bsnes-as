
struct PPUAccess {
  const uint16 *lightTable_lookup(uint8 luma) {
    return ppu.lightTable[luma & 0x0F];
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

  auto cgram_read(uint8 addr) -> uint16 {
    if (system.fastPPU()) {
      return ppufast.cgram[addr];
    }
    return ppu.screen.cgram[addr];
  }

  auto cgram_write(uint8 addr, uint16 value) {
    if (system.fastPPU()) {
      ppufast.cgram[addr] = value;
    }
    ppu.screen.cgram[addr] = value;
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

  static auto oam_get_object(PPUAccess *local, uint8 index) -> PPUAccess::OAMObject* {
    PPUAccess::OAMObject oam_object;
    if (system.fastPPU()) {
      auto po = ppufast.objects[index];
      local->oam_objects[index].x = po.x;
      local->oam_objects[index].y = po.y - 1;
      local->oam_objects[index].character = uint16(po.character) | (uint16(po.nameselect) << 8u);
      local->oam_objects[index].vflip = po.vflip;
      local->oam_objects[index].hflip = po.hflip;
      local->oam_objects[index].priority = po.priority;
      local->oam_objects[index].palette = po.palette;
      local->oam_objects[index].size = po.size;
    } else {
      auto po = ppu.obj.oam.object[index];
      local->oam_objects[index].x = po.x;
      local->oam_objects[index].y = po.y - 1;
      local->oam_objects[index].character = uint16(po.character) | (uint16(po.nameselect) << 8u);
      local->oam_objects[index].vflip = po.vflip;
      local->oam_objects[index].hflip = po.hflip;
      local->oam_objects[index].priority = po.priority;
      local->oam_objects[index].palette = po.palette;
      local->oam_objects[index].size = po.size;
    }
    return &local->oam_objects[index];
  }

  static auto oam_set_object(PPUAccess *local, uint8 index, PPUAccess::OAMObject *obj) -> void {
    if (system.fastPPU()) {
      auto &po = ppufast.objects[index];
      po.x = obj->x;
      po.y = obj->y + 1;
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
      po.y = obj->y + 1;
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

auto RegisterPPU(asIScriptEngine *e) -> void {
  int r;

  // create ppu namespace
  r = e->SetDefaultNamespace("ppu"); assert(r >= 0);

  // Font
  r = e->RegisterObjectType("Font", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
  r = e->RegisterObjectMethod("Font", "string get_displayName() property", asMETHOD(PixelFonts::Font, displayName), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Font", "uint measureText(const string &in)", asMETHOD(PixelFonts::Font, measureText), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Font", "uint get_height() property", asMETHOD(PixelFonts::Font, height), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Font", "uint get_width(uint r) property", asMETHOD(PixelFonts::Font, width), asCALL_THISCALL); assert(r >= 0);

  r = e->RegisterGlobalFunction("uint get_fonts_count() property", asFUNCTION(+([]{ return PixelFonts::fonts.count(); })), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterGlobalFunction("Font &get_fonts(uint i) property", asFUNCTION(+([](uint i){ return PixelFonts::fonts[i]; })), asCALL_CDECL); assert(r >= 0);

  r = e->RegisterGlobalFunction("uint16 rgb(uint8 r, uint8 g, uint8 b)", asFUNCTION(PPUAccess::ppu_rgb), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterGlobalFunction("uint8 get_luma() property", asFUNCTION(PPUAccess::ppu_get_luma), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterGlobalFunction("uint8 sprite_width(uint8 baseSize, uint8 size)", asFUNCTION(PPUAccess::ppu_sprite_width), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterGlobalFunction("uint8 sprite_height(uint8 baseSize, uint8 size)", asFUNCTION(PPUAccess::ppu_sprite_height), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterGlobalFunction("uint8 sprite_base_size()", asFUNCTION(PPUAccess::ppu_sprite_base_size), asCALL_CDECL); assert(r >= 0);

  // define ppu::VRAM object type for opIndex convenience:
  r = e->RegisterObjectType("VRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = e->RegisterObjectMethod("VRAM", "uint16 opIndex(uint16 addr)", asMETHOD(PPUAccess, vram_read), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("VRAM", "uint16 chr_address(uint16 chr)", asMETHOD(PPUAccess, vram_chr_address), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("VRAM", "void read_block(uint16 addr, uint offs, uint16 size, array<uint16> &inout output)", asMETHOD(PPUAccess, vram_read_block), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("VRAM", "void write_block(uint16 addr, uint offs, uint16 size, const array<uint16> &in data)", asMETHOD(PPUAccess, vram_write_block), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterGlobalProperty("VRAM vram", &ppuAccess); assert(r >= 0);

  // define ppu::CGRAM object type for opIndex convenience:
  r = e->RegisterObjectType("CGRAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = e->RegisterObjectMethod("CGRAM", "uint16 get_opIndex(uint8 addr) property", asMETHOD(PPUAccess, cgram_read), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("CGRAM", "void set_opIndex(uint8 addr, uint16 value) property", asMETHOD(PPUAccess, cgram_write), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterGlobalProperty("CGRAM cgram", &ppuAccess); assert(r >= 0);

  r = e->RegisterObjectType    ("OAMSprite", sizeof(PPUAccess::OAMObject), asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
  r = e->RegisterObjectMethod  ("OAMSprite", "bool   get_is_enabled() property", asMETHOD(PPUAccess::OAMObject, get_is_enabled), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectProperty("OAMSprite", "uint16 x", asOFFSET(PPUAccess::OAMObject, x)); assert(r >= 0);
  r = e->RegisterObjectProperty("OAMSprite", "uint8  y", asOFFSET(PPUAccess::OAMObject, y)); assert(r >= 0);
  r = e->RegisterObjectProperty("OAMSprite", "uint16 character", asOFFSET(PPUAccess::OAMObject, character)); assert(r >= 0);
  r = e->RegisterObjectProperty("OAMSprite", "bool   vflip", asOFFSET(PPUAccess::OAMObject, vflip)); assert(r >= 0);
  r = e->RegisterObjectProperty("OAMSprite", "bool   hflip", asOFFSET(PPUAccess::OAMObject, hflip)); assert(r >= 0);
  r = e->RegisterObjectProperty("OAMSprite", "uint8  priority", asOFFSET(PPUAccess::OAMObject, priority)); assert(r >= 0);
  r = e->RegisterObjectProperty("OAMSprite", "uint8  palette", asOFFSET(PPUAccess::OAMObject, palette)); assert(r >= 0);
  r = e->RegisterObjectProperty("OAMSprite", "uint8  size", asOFFSET(PPUAccess::OAMObject, size)); assert(r >= 0);
  r = e->RegisterObjectMethod  ("OAMSprite", "uint8  get_width() property", asMETHOD(PPUAccess::OAMObject, get_width), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod  ("OAMSprite", "uint8  get_height() property", asMETHOD(PPUAccess::OAMObject, get_height), asCALL_THISCALL); assert(r >= 0);

  r = e->RegisterObjectType  ("OAM", 0, asOBJ_REF | asOBJ_NOHANDLE); assert(r >= 0);
  r = e->RegisterObjectMethod("OAM", "OAMSprite @get_opIndex(uint8 chr) property", asFUNCTION(PPUAccess::oam_get_object), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("OAM", "void set_opIndex(uint8 chr, OAMSprite @sprite) property", asFUNCTION(PPUAccess::oam_set_object), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  r = e->RegisterGlobalProperty("OAM oam", &ppuAccess); assert(r >= 0);
}
