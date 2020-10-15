#ifndef DISABLE_HIRO
struct GUI {
  struct mSNESCanvas : hiro::mCanvas {
    mSNESCanvas() :
      hiro::mCanvas(),
      // Set 15-bit RGB format with 1-bit alpha for PPU-compatible images after lightTable mapping:
      img16(0, 16, 0x8000u, 0x7C00u, 0x03E0u, 0x001Fu),
      img32(0, 32, 0xFF000000u, 0x00FF0000u, 0x0000FF00u, 0x000000FFu)
    {
      construct();
    }

    // holds internal SNES 15-bit BGR colors
    image img16;
    // used for final display
    image img32;

    auto setSize(hiro::Size size) -> void {
      setGeometry(hiro::Geometry(
        geometry().position(),
        size
      ));
      //img16 = image(0, 16, 0x8000u, 0x7C00u, 0x03E0u, 0x001Fu);
      //img32 = image(0, 32, 0xFF000000u, 0x00FF0000u, 0x0000FF00u, 0x000000FFu);
      img16.allocate(size.width(), size.height());
      img32.allocate(size.width(), size.height());
      img16.fill(0x0000u);
      img32.fill(0x00000000u);
      setIcon(img32);
    }

    auto setPosition(float x, float y) -> void {
      setGeometry(hiro::Geometry(
        hiro::Position(x, y),
        geometry().size()
      ));
    }

    auto update() -> void {
      // translate to 24-bit or 30-bit RGB: (assuming 24-bit for now)
      for (int y = 0; y < img32.height(); y++) {
        uint8_t* p = img16.data() + (y * img16.pitch());
        for (int x = 0; x < img32.width(); x++, p += img16.stride()) {
          uint16_t bgr = *((uint16_t*)p);
          uint32_t rgb = emulatorPalette[bgr & 0x7FFFu] | ((bgr & 0x8000u) != 0u ? 0xFF000000u : 0u);
          img32.write(
            img32.data() + (y * img32.pitch()) + (x * img32.stride()),
            rgb
          );
        }
      }
      setIcon(img32);
    }

    uint8 mLuma = 0x0Fu;
    auto luma() -> uint8 { return mLuma; }
    auto set_luma(uint8 luma) -> void { mLuma = luma & 0x0Fu; }

    auto luma_adjust(uint16 color) -> uint16 {
      uint16 rgb = ppu.lightTable[mLuma & 0x0Fu][color & 0x7FFFu] | (color & 0x8000u);
      return rgb;
    }

    auto fill(uint16 color) -> void {
      img16.fill(luma_adjust(color));
    }

    static auto luma_adjust(uint16 color, uint8 luma) -> uint16 {
      // lightTable flips BGR to RGB as well as applying luma:
      uint16 bgr = ppu.lightTable[luma & 0x0Fu][color & 0x7FFFu];
      // flip the RGB back to BGR:
      //uint16 bgr =
      //    ((rgb >> 10u) & 0x001Fu)
      //  | ((rgb & 0x001Fu) << 10u)
      //  | (rgb & 0x03E0u);
      return bgr | (color & 0x8000u);
    }

    auto pixel(int x, int y, uint16 color) -> void {
      // bounds check:
      if (x < 0 || y < 0 || x >= img16.width() || y >= img16.height()) return;
      // set pixel with full alpha (1-bit on/off):
      img16.write(img16.data() + (y * img16.pitch()) + (x * img16.stride()), luma_adjust(color));
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
                auto color = palette_p[t] | 0x8000u;
                pixel(x + (tx << 3) + px, y + (ty << 3) + py, color);
              }
            }
          }
        }
      }
    }
  };

  using sSNESCanvas = shared_pointer<mSNESCanvas>;

  // shared pointer interface to mSNESCanvas:
  struct SNESCanvas : shared_pointer<mSNESCanvas> {
    SNESCanvas() : shared_pointer<mSNESCanvas>(new mSNESCanvas, [](auto p) {
      p->unbind();
      delete p;
    }) {
      (*this)->bind(*this);
    }

    SNESCanvas(const sSNESCanvas& source) : sSNESCanvas(source) { assert(source); }

    auto self() const -> mSNESCanvas& { return (mSNESCanvas&)operator*(); }
    auto ptr() const -> const sSNESCanvas& { return (const sSNESCanvas&)*this; }

    // Object:
    auto attribute(const string &name) const { return self().attribute(name); }
    auto enabled(bool recursive = false) const { return self().enabled(recursive); }
    auto focused() const { return self().focused(); }
    auto font(bool recursive = false) const { return self().font(recursive); }
    auto offset() const { return self().offset(); }
    auto parent() const {
      if(auto object = self().parent()) {
        if(auto instance = object->instance.acquire()) return hiro::Object(instance);
      }
      return hiro::Object();
    }
    auto remove() { return self().remove(), *this; }
    auto setAttribute(const string &name, const string &value) { return self().setAttribute(name, value), *this; }
    auto setEnabled(bool enabled = true) { return self().setEnabled(enabled), *this; }
    auto setFocused() { return self().setFocused(), *this; }
    auto setFont(const hiro::Font& font = {}) { return self().setFont(font), *this; }
    auto setVisible(bool visible = true) { return self().setVisible(visible), *this; }
    auto visible(bool recursive = false) const { return self().visible(recursive); }

    // Sizable:
    auto collapsible() const { return self().collapsible(); }
    auto layoutExcluded() const { return self().layoutExcluded(); }
    auto doSize() const { return self().doSize(); }
    auto geometry() const { return self().geometry(); }
    auto minimumSize() const { return self().minimumSize(); }
    auto onSize(const function<void ()>& callback = {}) { return self().onSize(callback), *this; }
    auto setCollapsible(bool collapsible = true) { return self().setCollapsible(collapsible), *this; }
    auto setLayoutExcluded(bool layoutExcluded = true) { return self().setLayoutExcluded(layoutExcluded), *this; }
    auto setGeometry(hiro::Geometry geometry) { return self().setGeometry(geometry), *this; }


    auto setAlignment(float horizontal, float vertical) {
      self().setAlignment(hiro::Alignment{horizontal, vertical});
    }

    auto alignment() -> hiro::Alignment* {
      return new hiro::Alignment(self().alignment());
    }

    auto setAlignment(hiro::Alignment& alignment) {
      return self().setAlignment(alignment), *this;
    }

    auto setSize(hiro::Size &size) {
      return self().setSize(size), *this;
    }

    auto setPosition(float x, float y) {
      self().setGeometry(hiro::Geometry(
        hiro::Position(x, y),
        self().geometry().size()
      ));
      return *this;
    }

    auto update() -> void {
      self().update();
    }

    auto luma() -> uint8 { return self().luma(); }
    auto set_luma(uint8 luma) -> void { self().set_luma(luma); }

    auto fill(uint16 color) -> void {
      self().fill(color);
    }

    auto pixel(int x, int y, uint16 color) -> void {
      self().pixel(x, y, color);
    }

    auto draw_sprite_4bpp(int x, int y, uint c, uint width, uint height, const CScriptArray *tile_data, const CScriptArray *palette_data) -> void {
      self().draw_sprite_4bpp(x, y, c, width, height, tile_data, palette_data);
    }
  };
};
#else
struct GUI {
  struct mSNESCanvas : hiro::mCanvas {
    mSNESCanvas() {}

    auto setSize(hiro::Size size) -> void {}
    auto setPosition(float x, float y) -> void {}
    auto update() -> void {}
    auto luma() -> uint8 { return 0; }
    auto set_luma(uint8 luma) -> void { }
    auto luma_adjust(uint16 color) -> uint16 { return 0; }
    auto fill(uint16 color) -> void {}
    static auto luma_adjust(uint16 color, uint8 luma) -> uint16 { return 0; }
    auto pixel(int x, int y, uint16 color) -> void {}
    auto draw_sprite_4bpp(int x, int y, uint c, uint width, uint height, const CScriptArray *tile_data, const CScriptArray *palette_data) -> void {}
  };

  // shared pointer interface to mSNESCanvas:
  struct SNESCanvas : shared_pointer<mSNESCanvas>, mSNESCanvas {
    using type = SNESCanvas;

    auto setAlignment(hiro::Alignment alignment = {}) -> type& { return *this; }
    auto setAlignment(float horizontal, float vertical) -> void {}
  };
};
#endif

#ifndef DISABLE_HIRO
bool isGuiEnabled = true;
#else
bool isGuiEnabled = false;
#endif

auto RegisterGUI(asIScriptEngine *e) -> void {
  int r;

  ////////////////////////////////////////////////////////////////////////
  // Macros:

#define EXPOSE_VALUE_CA(name, shClass, sClass) \
  REG_LAMBDA_CTOR(name, "void f()", ([](void* address){ new (address) shClass((const sClass &)sClass()); })); \
  REG_LAMBDA(name, "void construct()", ([](shClass &p){ p.operator=(shClass()); })); \
  REG_LAMBDA(name, "void destruct()", ([](shClass *p){ value_destroy<shClass>(p); })); \
  REG_LAMBDA(name, "bool opImplConv() const", ([](shClass &p){ return p.operator bool(); })); \
  r = e->RegisterObjectMethod(#name, #name " &opAssign(const " #name " &in)", asFUNCTION(value_assign<shClass>), asCALL_CDECL_OBJFIRST); assert(r >= 0);

#define EXPOSE_VALUE_CAK(name, shClass, sClass) \
  EXPOSE_VALUE_CA(name, shClass, sClass) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_CONSTRUCT, "void f(const " #name " & in)", asFUNCTION(value_copy_construct<shClass>), asCALL_CDECL_OBJFIRST); assert(r >= 0);

#define EXPOSE_VALUE_CDAK(name, shClass, sClass) \
  EXPOSE_VALUE_CAK(name, shClass, sClass) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_DESTRUCT, "void f()", asFUNCTION(value_destroy<shClass>), asCALL_CDECL_OBJFIRST); assert(r >= 0);

#define EXPOSE_VALUE_CDA(name, shClass, sClass) \
  EXPOSE_VALUE_CA(name, shClass, sClass) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_DESTRUCT, "void f()", asFUNCTION(value_destroy<shClass>), asCALL_CDECL_OBJFIRST); assert(r >= 0);

#define EXPOSE_HIRO_VALUE(name) EXPOSE_VALUE_CDAK(name, hiro::name, hiro::s##name)

#define REG_SH_VOID0(name, shClass, defn, method) REG_FUNC(name, defn, (Deref<auto (shClass::*)(void) -> void>::f<&shClass::method>))
#define REG_SH_CVOID0(name, shClass, defn, method) REG_FUNC(name, defn, (Deref<auto (shClass::*)(void) const -> void>::f<&shClass::method>))
#define REG_SH_SELF0(name, shClass, defn, method) REG_FUNC(name, defn, (Deref<auto (shClass::*)(void) -> shClass>::d<&shClass::method>))
#define HIRO_SELF0(name, defn, method) REG_SH_SELF0(name, hiro::name, defn, method)
#define REG_SH_SELF1(name, shClass, defn, method, a0) REG_FUNC(name, defn, (Deref<auto (shClass::*)(a0) -> shClass>::d<&shClass::method>))
#define HIRO_SELF1(name, defn, method, a0) REG_SH_SELF1(name, hiro::name, defn, method, a0)
#define REG_SH_SELF2(name, shClass, defn, method, a0, a1) REG_FUNC(name, defn, (Deref<auto (shClass::*)(a0, a1) -> shClass>::d<&shClass::method>))
#define HIRO_SELF2(name, defn, method, a0, a1) REG_SH_SELF2(name, hiro::name, defn, method, a0, a1)
#define REG_SH_GETTER0(name, shClass, defn, fieldType, getterMethod) REG_FUNC(name, defn, (Deref<auto (shClass::*)(void) const -> fieldType>::f<&shClass::getterMethod>))
#define HIRO_GETTER0(name, defn, fieldType, getterMethod) REG_SH_GETTER0(name, hiro::name, defn, fieldType, getterMethod)
#define REG_SH_GETTER1(name, shClass, defn, fieldType, getterMethod, a0) REG_FUNC(name, defn, (Deref<auto (shClass::*)(a0) const -> fieldType>::f<&shClass::getterMethod>))
#define HIRO_GETTER1(name, defn, fieldType, getterMethod, a0) REG_SH_GETTER1(name, hiro::name, defn, fieldType, getterMethod, a0)
#define REG_SH_SETTER0(name, shClass, defn, fieldType, setterMethod) REG_FUNC(name, defn, (Deref<auto (shClass::*)(fieldType) -> shClass>::d<&shClass::setterMethod>))
#define HIRO_SETTER0(name, defn, fieldType, setterMethod) REG_SH_SETTER0(name, hiro::name, defn, fieldType, setterMethod)
#define REG_SH_SETTER1(name, shClass, defn, fieldType, setterMethod, a0) REG_FUNC(name, defn, (Deref<auto (shClass::*)(fieldType, a0) -> shClass>::d<&shClass::setterMethod>))
#define HIRO_SETTER1(name, defn, fieldType, setterMethod, a0) REG_SH_SETTER1(name, hiro::name, defn, fieldType, setterMethod, aa0)

  // Object:
#define EXPOSE_OBJECT(name, shClass) \
  REG_SH_GETTER1(name, shClass, "string get_attributes(const string &in) property",                 string,            attribute,    const string&); \
  REG_SH_GETTER1(name, shClass, "bool get_enabled(bool recursive = false) property",                bool,              enabled,      bool); \
  REG_SH_GETTER0(name, shClass, "bool get_focused() property",                                      bool,              focused); \
  REG_SH_GETTER1(name, shClass, "Font get_font(bool recursive = false) property",                   hiro::Font,        font,         bool); \
  REG_SH_GETTER0(name, shClass, "int get_offset() property",                                        int,               offset); \
  REG_SH_GETTER1(name, shClass, "bool get_visible(bool recursive = false) property",                bool,              visible,      bool); \
  REG_SH_SETTER1(name, shClass, "void set_attributes(const string &in, const string &in) property", const string &,    setAttribute, const string &); \
  REG_SH_SETTER0(name, shClass, "void set_font(const Font &in font) property",                      const hiro::Font&, setFont); \
  REG_SH_SETTER0(name, shClass, "void set_visible(bool visible) property",                          bool,              setVisible); \
  REG_SH_SETTER0(name, shClass, "void set_enabled(bool enabled) property",                          bool,              setEnabled); \
  REG_SH_SELF0  (name, shClass, "void remove()",     remove); \
  REG_SH_SELF0  (name, shClass, "void setFocused()", setFocused)

  //REG_LAMBDA(name, "Object get_parent()",                        ([](shClass& self) { return self.parent(); })); \

#define EXPOSE_HIRO_OBJECT(name) EXPOSE_OBJECT(name, hiro::name)

  // Sizable:
#define EXPOSE_SIZABLE(name, shClass) \
  REG_SH_GETTER0(name, shClass, "Geometry get_geometry() property",       hiro::Geometry, geometry); \
  REG_SH_GETTER0(name, shClass, "bool get_collapsible() property",        bool,           collapsible); \
  REG_SH_GETTER0(name, shClass, "bool get_layoutExcluded() property",     bool,           layoutExcluded); \
  REG_SH_GETTER0(name, shClass, "Size get_minimumSize() property",        hiro::Size,     minimumSize); \
  REG_SH_SETTER0(name, shClass, "void set_geometry(Geometry) property",   hiro::Geometry, setGeometry); \
  REG_SH_SETTER0(name, shClass, "void set_collapsible(bool) property",    bool,           setCollapsible); \
  REG_SH_SETTER0(name, shClass, "void set_layoutExcluded(bool) property", bool,           setLayoutExcluded); \
  REG_LAMBDA(name, "void setPosition(float, float)", ([](shClass& self, float x, float y) { \
    CHECK_ALIVE(self); \
    self.setGeometry(hiro::Geometry(hiro::Position(x, y), self.geometry().size())); \
  })); \
  REG_SH_CVOID0 (name, shClass, "void doSize()", doSize); \
  REG_LAMBDA(name, "void onSize(Callback @callback)", ([](shClass& self, asIScriptFunction *cb) {    \
    CHECK_ALIVE(self); \
    self.onSize(Callback(cb));       \
  }));

#define EXPOSE_HIRO_SIZABLE(name) EXPOSE_SIZABLE(name, hiro::name)

  // Widget:
#define EXPOSE_WIDGET(name, shClass) \
  REG_SH_GETTER0(name, shClass, "string get_toolTip() property",                 string, toolTip); \
  REG_SH_SETTER0(name, shClass, "void   set_toolTip(const string &in) property", const string &, setToolTip)

#define EXPOSE_HIRO_WIDGET(name) EXPOSE_WIDGET(name, hiro::name)

  ////////////////////////////////////////////////////////////////////////
  // GUI
  r = e->SetDefaultNamespace("GUI"); assert(r >= 0);

  // GUI::enabled
  r = e->RegisterGlobalProperty("bool enabled", (void*) &isGuiEnabled);

  REG_LAMBDA_GLOBAL("float get_dpiX() property", ([]() -> float {
    // round DPI scalar to increments of 0.5 (eg 1.0, 1.5, 2.0, ...)
    auto scale = round(hiro::Monitor::dpi().x() / 96.0 * 2.0) / 2.0;
    return scale;
  }));

  REG_LAMBDA_GLOBAL("float get_dpiY() property", ([]() -> float {
    // round DPI scalar to increments of 0.5 (eg 1.0, 1.5, 2.0, ...)
    auto scale = round(hiro::Monitor::dpi().y() / 96.0 * 2.0) / 2.0;
    return scale;
  }));

  // function types:
  r = e->RegisterFuncdef("void Callback()"); assert(r >= 0);

  // Register shared_pointer types:
  REG_VALUE_TYPE(Window,           hiro::Window,           asOBJ_APP_CLASS_CDAK);
  REG_VALUE_TYPE(VerticalLayout,   hiro::VerticalLayout,   asOBJ_APP_CLASS_CDAK);
  REG_VALUE_TYPE(HorizontalLayout, hiro::HorizontalLayout, asOBJ_APP_CLASS_CDAK);
  REG_VALUE_TYPE(Group,            hiro::Group,            asOBJ_APP_CLASS_CDA);
  REG_VALUE_TYPE(LineEdit,         hiro::LineEdit,         asOBJ_APP_CLASS_CDAK);
  REG_VALUE_TYPE(Label,            hiro::Label,            asOBJ_APP_CLASS_CDAK);
  REG_VALUE_TYPE(Button,           hiro::Button,           asOBJ_APP_CLASS_CDAK);
  REG_VALUE_TYPE(Canvas,           hiro::Canvas,           asOBJ_APP_CLASS_CDAK);
  REG_VALUE_TYPE(SNESCanvas,       GUI::SNESCanvas,        asOBJ_APP_CLASS_CDAK);
  REG_VALUE_TYPE(CheckLabel,       hiro::CheckLabel,       asOBJ_APP_CLASS_CDAK);
  REG_VALUE_TYPE(ComboButtonItem,  hiro::ComboButtonItem,  asOBJ_APP_CLASS_CDAK);
  REG_VALUE_TYPE(ComboButton,      hiro::ComboButton,      asOBJ_APP_CLASS_CDAK);
  REG_VALUE_TYPE(HorizontalSlider, hiro::HorizontalSlider, asOBJ_APP_CLASS_CDAK);

  // value types:
  REG_VALUE_TYPE(Alignment, hiro::Alignment, asOBJ_APP_CLASS_CDK);
  REG_VALUE_TYPE(Color,     hiro::Color,     asOBJ_APP_CLASS_CDK);
  REG_VALUE_TYPE(Font,      hiro::Font,      asOBJ_APP_CLASS_CDK);
  REG_VALUE_TYPE(Position,  hiro::Position,  asOBJ_APP_CLASS_CDK);
  REG_VALUE_TYPE(Size,      hiro::Size,      asOBJ_APP_CLASS_CDK);
  REG_VALUE_TYPE(Geometry,  hiro::Geometry,  asOBJ_APP_CLASS_CDK);

  // Alignment value type:
  REG_LAMBDA_CTOR(Alignment, "void f(float, float)", ([](void * address, float h, float v) { new (address) hiro::Alignment(h, v); }));
  REG_LAMBDA_CTOR(Alignment, "void f(const Alignment &in)", ([](void * address, const hiro::Alignment &other){ new (address) hiro::Alignment(other); }));
  REG_LAMBDA_DTOR(Alignment, "void f()", ([](hiro::Alignment &self){ self.~Alignment(); }));
  REG_LAMBDA(Alignment, "float get_horizontal() property",      ([](hiro::Alignment& self) -> float { return self.horizontal(); }));
  REG_LAMBDA(Alignment, "void  set_horizontal(float) property", ([](hiro::Alignment& self, float value)    { self.setHorizontal(value); }));
  REG_LAMBDA(Alignment, "float get_vertical() property",        ([](hiro::Alignment& self) -> float { return self.vertical(); }));
  REG_LAMBDA(Alignment, "void  set_vertical(float) property",   ([](hiro::Alignment& self, float value)    { self.setVertical(value); }));

  // Position value type:
  REG_LAMBDA_CTOR(Position, "void f(float, float)", ([](void * address, float x, float y){ new (address) hiro::Position(x, y); }));
  REG_LAMBDA_CTOR(Position, "void f(const Position &in)", ([](void * address, const hiro::Position &other){ new (address) hiro::Position(other); }));
  REG_LAMBDA_DTOR(Position, "void f()", ([](hiro::Position &self){ self.~Position(); }));
  REG_LAMBDA(Position, "float get_x() property",      ([](hiro::Position& self) -> float { return self.x(); }));
  REG_LAMBDA(Position, "void  set_x(float) property", ([](hiro::Position& self, float value)    { self.setX(value); }));
  REG_LAMBDA(Position, "float get_y() property",      ([](hiro::Position& self) -> float { return self.y(); }));
  REG_LAMBDA(Position, "void  set_y(float) property", ([](hiro::Position& self, float value)    { self.setY(value); }));

  // Size value type:
  REG_LAMBDA_CTOR(Size, "void f(float, float)", ([](void * address, float width, float height){ new (address) hiro::Size(width, height); }));
  REG_LAMBDA_CTOR(Size, "void f(const Size &in)", ([](void * address, const hiro::Size &other){ new (address) hiro::Size(other); }));
  REG_LAMBDA_DTOR(Size, "void f()", ([](hiro::Size &self){ self.~Size(); }));
  REG_LAMBDA(Size, "float get_width() property",       ([](hiro::Size& self) -> float { return self.width(); }));
  REG_LAMBDA(Size, "void  set_width(float) property",  ([](hiro::Size& self, float value)    { self.setWidth(value); }));
  REG_LAMBDA(Size, "float get_height() property",      ([](hiro::Size& self) -> float { return self.height(); }));
  REG_LAMBDA(Size, "void  set_height(float) property", ([](hiro::Size& self, float value)    { self.setHeight(value); }));

  // Geometry value type:
  REG_LAMBDA_CTOR(Geometry, "void f(Position, Size)", ([](void * address, hiro::Position position, hiro::Size size){ new (address) hiro::Geometry(position, size); }));
  REG_LAMBDA_CTOR(Geometry, "void f(const Geometry &in)", ([](void * address, const hiro::Geometry &other){ new (address) hiro::Geometry(other); }));
  REG_LAMBDA_DTOR(Geometry, "void f()", ([](hiro::Geometry &self){ self.~Geometry(); }));
  REG_LAMBDA(Geometry, "Position get_position() property",         ([](hiro::Geometry& self) { return self.position(); }));
  REG_LAMBDA(Geometry, "void     set_position(Position) property", ([](hiro::Geometry& self, hiro::Position position) { self.setPosition(position); }));
  REG_LAMBDA(Geometry, "Size get_size() property",                 ([](hiro::Geometry& self) { return self.size(); }));
  REG_LAMBDA(Geometry, "void set_size(Size) property",             ([](hiro::Geometry& self, hiro::Size size) { self.setSize(size); }));

  // Color value type:
  REG_LAMBDA_CTOR(Color, "void f(int red, int green, int blue, int alpha = 255)", ([](void * address, int r, int g, int b, int a){ new (address) hiro::Color(r,g,b,a); }));
  REG_LAMBDA_CTOR(Color, "void f(const Color &in)", ([](void * address, const hiro::Color &other){ new (address) hiro::Color(other); }));
  REG_LAMBDA_DTOR(Color, "void f()", ([](hiro::Color &self){ self.~Color(); }));
  //REG_LAMBDA(Color, "void setColor(int red, int green, int blue, int alpha = 255)", asMETHODPR(hiro::Color, setColor, (int, int, int, int), hiro::Color&), asCALL_THISCALL); assert(r >= 0);
  //REG_LAMBDA(Color, "void setValue(uint32 value)",   asMETHOD(hiro::Color, setValue), asCALL_THISCALL); assert(r >= 0);
  REG_LAMBDA(Color, "uint8 get_alpha() property",         ([](hiro::Color& self) -> uint8 { return self.alpha(); }));
  REG_LAMBDA(Color, "uint8 get_blue() property",          ([](hiro::Color& self) -> uint8 { return self.blue(); }));
  REG_LAMBDA(Color, "uint8 get_green() property",         ([](hiro::Color& self) -> uint8 { return self.green(); }));
  REG_LAMBDA(Color, "uint8 get_red() property",           ([](hiro::Color& self) -> uint8 { return self.red(); }));
  REG_LAMBDA(Color, "void set_alpha(uint8 alpha) property", ([](hiro::Color& self, uint8 alpha){ self.setAlpha(alpha); }));
  REG_LAMBDA(Color, "void set_blue(uint8 blue) property",   ([](hiro::Color& self, uint8 blue){ self.setBlue(blue); }));
  REG_LAMBDA(Color, "void set_green(uint8 green) property", ([](hiro::Color& self, uint8 green){ self.setGreen(green); }));
  REG_LAMBDA(Color, "void set_red(uint8 red) property",     ([](hiro::Color& self, uint8 red){ self.setRed(red); }));

  REG_LAMBDA_GLOBAL("Color colorFromSNES(uint16 bgr)", ([](uint16_t bgr) -> hiro::Color {
    uint16 rgb = GUI::mSNESCanvas::luma_adjust(bgr, 0x0F);
    uint64_t r = (rgb      ) & 0x1F;
    uint64_t g = (rgb >>  5) & 0x1F;
    uint64_t b = (rgb >> 10) & 0x1F;
    r = image::normalize(r, 5, 8);
    g = image::normalize(g, 5, 8);
    b = image::normalize(b, 5, 8);
    return hiro::Color(r, g, b, 255);
  }));

  // Font value type:
  REG_LAMBDA_CTOR(Font, "void f(const string &in family, float size = 0.0)", ([](void * address, string &family, float size){ new (address) hiro::Font(family, size); }));
  REG_LAMBDA_CTOR(Font, "void f(const Font &in)", ([](void * address, const hiro::Font &other){ new (address) hiro::Font(other); }));
  REG_LAMBDA_DTOR(Font, "void f()", ([](hiro::Font &font){ font.~Font(); }));
  REG_LAMBDA(Font, "bool   get_bold() property",                          ([](hiro::Font& self) { return self.bold(); }));
  REG_LAMBDA(Font, "string get_family() property",                        ([](hiro::Font& self) { return self.family(); }));
  REG_LAMBDA(Font, "bool   get_italic() property",                        ([](hiro::Font& self) { return self.italic(); }));
  REG_LAMBDA(Font, "float  get_size() property",                          ([](hiro::Font& self) { return self.size(); }));
  REG_LAMBDA(Font, "void   set_bold(bool bold) property",                 ([](hiro::Font& self, bool bold) { self.setBold(bold); }));
  REG_LAMBDA(Font, "void   set_family(const string &in family) property", ([](hiro::Font& self, string &family) { self.setFamily(family); }));
  REG_LAMBDA(Font, "void   set_italic(bool italic) property",             ([](hiro::Font& self, bool italic) { self.setItalic(italic); }));
  REG_LAMBDA(Font, "void   set_size(float size) property",                ([](hiro::Font& self, float size) { self.setSize(size); }));

  REG_LAMBDA(Font, "Size measure(const string &in text)", ([](hiro::Font& self, string &text) { return self.size(text); }));
  REG_LAMBDA(Font, "void reset()", ([](hiro::Font& self) { self.reset(); }));

  // Font names:
  r = e->RegisterGlobalProperty("string Sans",  (void *) &hiro::Font::Sans); assert(r >= 0);
  r = e->RegisterGlobalProperty("string Serif", (void *) &hiro::Font::Serif); assert(r >= 0);
  r = e->RegisterGlobalProperty("string Mono",  (void *) &hiro::Font::Mono); assert(r >= 0);

  ///////////////////////////////////////////////////////////////////////////////////

  // Window
  REG_VALUE_TYPE(Window, hiro::Window, asOBJ_APP_CLASS_CDAK);
  EXPOSE_VALUE_CAK(Window, hiro::Window, hiro::sWindow);
  REG_LAMBDA_DTOR(Window, "void f()", ([](hiro::Window& self){
    if (!((bool)self.ptr())) {
      return;
    }
    self->setVisible(false);
    self->setDismissable(true);
    self->destruct();
    self.reset();
  }));

  REG_LAMBDA_CTOR(Window, "void f(float rx, float ry, bool relative)", ([](void *address, float x, float y, bool relative) {
    auto window = new (address) hiro::Window;
#ifndef DISABLE_HIRO
    if (relative) {
      window->setPosition(platform->presentationWindow(), hiro::Position{x, y});
    } else {
      window->setPosition(hiro::Position{x, y});
    }
#endif
  }));

  EXPOSE_HIRO_OBJECT(Window);
  REG_LAMBDA(Window, "void append(const ? &in sizable)", ([](hiro::Window& self, hiro::Sizable* sizable, int sizableTypeId){
    CHECK_ALIVE(self);
    self.append(*sizable);
  }));
  REG_LAMBDA(Window, "void remove(const ? &in sizable)", ([](hiro::Window& self, hiro::Sizable* sizable, int sizableTypeId){
    CHECK_ALIVE(self);
    self.remove(*sizable);
  }));
  HIRO_SELF0(Window, "void reset()", reset);

  HIRO_GETTER0(Window, "Color get_backgroundColor() property",  hiro::Color, backgroundColor);
  HIRO_GETTER0(Window, "bool get_dismissable() property",       bool, dismissable);
  HIRO_GETTER0(Window, "bool get_fullScreen() property",        bool, fullScreen);
  HIRO_GETTER0(Window, "bool get_maximized() property",         bool, maximized);
  HIRO_GETTER0(Window, "Size get_maximumSize() property",       hiro::Size, maximumSize);
  HIRO_GETTER0(Window, "bool get_minimized() property",         bool, minimized);
  HIRO_GETTER0(Window, "Size get_minimumSize() property",       hiro::Size, minimumSize);
  HIRO_GETTER0(Window, "bool get_modal() property",             bool, modal);
  HIRO_GETTER0(Window, "bool get_resizable() property",         bool, resizable);
  //HIRO_GETTER0(Window, "Sizable get_sizable() property",        Sizable, sizable);
  HIRO_GETTER0(Window, "string get_title() property",           string, title);
  HIRO_GETTER0(Window, "Geometry get_frameGeometry() property", hiro::Geometry, frameGeometry);
  HIRO_GETTER0(Window, "Geometry get_geometry() property",      hiro::Geometry, geometry);

  HIRO_SETTER0(Window, "void set_backgroundColor(Color color) property",        hiro::Color, setBackgroundColor);
  HIRO_SETTER0(Window, "void set_dismissable(bool dismissable) property",       bool,               setDismissable);
  HIRO_SETTER0(Window, "void set_fullScreen(bool fullScreen) property",         bool,           setFullScreen);
  HIRO_SETTER0(Window, "void set_maximized(bool maximized) property",           bool,           setMaximized);
  HIRO_SETTER0(Window, "void set_maximumSize(Size) property",    hiro::Size,   setMaximumSize);
  HIRO_SETTER0(Window, "void set_minimized(bool minimized) property",           bool,           setMinimized);
  HIRO_SETTER0(Window, "void set_minimumSize(Size) property",    hiro::Size,   setMinimumSize);
  HIRO_SETTER0(Window, "void set_modal(bool modal) property",                   bool,           setModal);
  HIRO_SETTER0(Window, "void set_resizable(bool resizable) property",           bool,           setResizable);
  HIRO_SETTER0(Window, "void set_title(const string &in title) property",       const string &, setTitle);
  HIRO_SETTER0(Window, "void set_size(Size) property",           hiro::Size,   setSize);

  REG_LAMBDA(Window, "void onSize(Callback @cb)", ([](hiro::Window& self, asIScriptFunction *cb) {
    CHECK_ALIVE(self);
    self.onSize(Callback(cb));
  }));

#if 0
  // more Window properties and functions to add:

  // probably don't want these just yet but eventually:
  //auto droppable() const { return self().droppable(); }
  //auto handle() const { return self().handle(); }
  //auto menuBar() const { return self().menuBar(); }
  //auto monitor() const { return self().monitor(); }
  //auto statusBar() const { return self().statusBar(); }

  //auto append(sMenuBar menuBar) { return self().append(menuBar), *this; }
  //auto append(sStatusBar statusBar) { return self().append(statusBar), *this; }
  //auto doDrop(vector<string> names) const { return self().doDrop(names); }
  //auto doKeyPress(signed key) const { return self().doKeyPress(key); }
  //auto doKeyRelease(signed key) const { return self().doKeyRelease(key); }
  //auto remove(sMenuBar menuBar) { return self().remove(menuBar), *this; }
  //auto remove(sStatusBar statusBar) { return self().remove(statusBar), *this; }

  //auto setDroppable(bool droppable = true) { return self().setDroppable(droppable), *this; }
  //auto setFrameGeometry(Geometry geometry) { return self().setFrameGeometry(geometry), *this; }
  //auto setGeometry(Geometry geometry) { return self().setGeometry(geometry), *this; }
  //auto setGeometry(Alignment alignment, Size size) { return self().setGeometry(alignment, size), *this; }

  // want to add these:
  auto setAlignment(Alignment alignment = Alignment::Center) { return self().setAlignment(alignment), *this; }
  auto setAlignment(sWindow relativeTo, Alignment alignment = Alignment::Center) { return self().setAlignment(relativeTo, alignment), *this; }
  auto setFramePosition(Position position) { return self().setFramePosition(position), *this; }
  auto setFrameSize(Size size) { return self().setFrameSize(size), *this; }
  auto setPosition(Position position) { return self().setPosition(position), *this; }
  auto setPosition(sWindow relativeTo, Position position) { return self().setPosition(relativeTo, position), *this; }

  auto doClose() const { return self().doClose(); }
  auto doMove() const { return self().doMove(); }
  auto doSize() const { return self().doSize(); }

  auto onClose(const function<void ()>& callback = {}) { return self().onClose(callback), *this; }
  auto onDrop(const function<void (vector<string>)>& callback = {}) { return self().onDrop(callback), *this; }
  auto onKeyPress(const function<void (signed)>& callback = {}) { return self().onKeyPress(callback), *this; }
  auto onKeyRelease(const function<void (signed)>& callback = {}) { return self().onKeyRelease(callback), *this; }
  auto onMove(const function<void ()>& callback = {}) { return self().onMove(callback), *this; }
  auto remove(sSizable sizable) { return self().remove(sizable), *this; }
  auto reset() { return self().reset(), *this; }
#endif

  // VerticalLayout
  EXPOSE_HIRO_VALUE(VerticalLayout);
  EXPOSE_HIRO_OBJECT(VerticalLayout);
  EXPOSE_HIRO_SIZABLE(VerticalLayout);
  REG_LAMBDA(VerticalLayout, "void append(const ? &in sizable, Size &in size, float spacing = 5.0)",
    ([](hiro::VerticalLayout& self, hiro::Sizable *sizable, int sizableTypeId, hiro::Size *size, float spacing){
      CHECK_ALIVE(self);
      self.append(*sizable, *size, spacing);
    }));
  REG_LAMBDA(VerticalLayout, "void remove(const ? &in sizable)",        ([](hiro::VerticalLayout& self, hiro::Sizable* sizable, int sizableTypeId){
    CHECK_ALIVE(self);
    self.remove(*sizable);
  }));
  HIRO_SELF0(VerticalLayout, "void resize()",                        resize);
  HIRO_SELF2(VerticalLayout, "void setPadding(float x, float y)",    setPadding, float, float);
  HIRO_SELF1(VerticalLayout, "void setSpacing(float spacing = 5.0)", setSpacing, float);
  REG_LAMBDA(VerticalLayout, "void setAlignment(float alignment)",      ([](hiro::VerticalLayout& self, float alignment) {
    CHECK_ALIVE(self);
    self.setAlignment(alignment);
  }));
  REG_LAMBDA(VerticalLayout, "void resetAlignment()",                   ([](hiro::VerticalLayout& self) {
    CHECK_ALIVE(self);
    self.setAlignment();
  }));

  // HorizontalLayout
  EXPOSE_HIRO_VALUE(HorizontalLayout);
  EXPOSE_HIRO_OBJECT(HorizontalLayout);
  EXPOSE_HIRO_SIZABLE(HorizontalLayout);
  REG_LAMBDA(HorizontalLayout, "void append(const ? &in sizable, Size &in size, float spacing = 5.0)",
    ([](hiro::HorizontalLayout& self, hiro::Sizable *sizable, int sizableTypeId, hiro::Size *size, float spacing){
      CHECK_ALIVE(self);
      self.append(*sizable, *size, spacing);
    }));
  REG_LAMBDA(HorizontalLayout, "void remove(const ? &in sizable)",        ([](hiro::HorizontalLayout& self, hiro::Sizable* sizable, int sizableTypeId){
    CHECK_ALIVE(self);
    self.remove(*sizable);
  }));
  HIRO_SELF0(HorizontalLayout, "void resize()",                        resize);
  HIRO_SELF2(HorizontalLayout, "void setPadding(float x, float y)",    setPadding, float, float);
  HIRO_SELF1(HorizontalLayout, "void setSpacing(float spacing = 5.0)", setSpacing, float);
  REG_LAMBDA(HorizontalLayout, "void setAlignment(float alignment)", ([](hiro::HorizontalLayout& self, float alignment) {
    CHECK_ALIVE(self);
    self.setAlignment(alignment);
  }));
  REG_LAMBDA(HorizontalLayout, "void resetAlignment()", ([](hiro::HorizontalLayout& self) {
    CHECK_ALIVE(self);
    self.setAlignment();
  }));

  // Group
  EXPOSE_VALUE_CDA(Group, hiro::Group, hiro::sGroup);
  REG_LAMBDA(Group, "void append(const ? &in object)",      ([](hiro::Group& self, hiro::Object *object, int objectTypeId){
    CHECK_ALIVE(self);
    self.append(*object);
  }));
  REG_LAMBDA(Group, "void remove(const ? &in object)",      ([](hiro::Group& self, hiro::Object *object, int objectTypeId){
    CHECK_ALIVE(self);
    self.remove(*object);
  }));
  //REG_LAMBDA(Group, "Group get_opIndex(uint i) property",  ([](hiro::Group& self, uint i) { return new hiro::ComboButtonItem(self.object(i)); }));
  HIRO_GETTER0(Group, "uint count()", uint, objectCount);

  // LineEdit
  EXPOSE_HIRO_VALUE(LineEdit);
  EXPOSE_HIRO_OBJECT(LineEdit);
  EXPOSE_HIRO_SIZABLE(LineEdit);
  EXPOSE_HIRO_WIDGET(LineEdit);
  HIRO_GETTER0(LineEdit, "string get_text() property",                    string, text);
  HIRO_SETTER0(LineEdit, "void set_text(const string &in text) property", const string&, setText);
  REG_LAMBDA  (LineEdit, "void onChange(Callback @cb)", ([](hiro::LineEdit& self, asIScriptFunction* cb) {
    CHECK_ALIVE(self);
    self.onChange(Callback(cb));
  }));

  // Label
  EXPOSE_HIRO_VALUE(Label);
  EXPOSE_HIRO_OBJECT(Label);
  EXPOSE_HIRO_SIZABLE(Label);
  EXPOSE_HIRO_WIDGET(Label);
  HIRO_GETTER0(Label, "Alignment get_alignment() property",    hiro::Alignment, alignment);
  HIRO_GETTER0(Label, "Color get_backgroundColor() property",  hiro::Color,     backgroundColor);
  HIRO_GETTER0(Label, "Color get_foregroundColor() property",  hiro::Color,     foregroundColor);
  HIRO_GETTER0(Label, "string get_text() property",            string,    text);
  HIRO_SETTER0(Label, "void set_alignment(const Alignment &in) property",   hiro::Alignment, setAlignment);
  HIRO_SETTER0(Label, "void set_backgroundColor(const Color &in) property", hiro::Color,     setBackgroundColor);
  HIRO_SETTER0(Label, "void set_foregroundColor(const Color &in) property", hiro::Color,     setForegroundColor);
  HIRO_SETTER0(Label, "void set_text(const string &in) property",           const string&,   setText);

  // Button
  EXPOSE_HIRO_VALUE(Button);
  EXPOSE_HIRO_OBJECT(Button);
  EXPOSE_HIRO_SIZABLE(Button);
  EXPOSE_HIRO_WIDGET(Button);
  HIRO_GETTER0(Button, "string get_text() property",                    string, text);
  HIRO_SETTER0(Button, "void set_text(const string &in text) property", const string &, setText);
  REG_LAMBDA  (Button, "void onActivate(Callback @cb)", ([](hiro::Button& self, asIScriptFunction* cb){
    CHECK_ALIVE(self);
    self.onActivate(Callback(cb));
  }));

  // Canvas
  EXPOSE_HIRO_VALUE(Canvas);
  EXPOSE_HIRO_OBJECT(Canvas);
  EXPOSE_HIRO_SIZABLE(Canvas);
  EXPOSE_HIRO_WIDGET(Canvas);
  REG_LAMBDA(Canvas, "bool loadPNG(const string &in)", ([](hiro::Canvas& self, const string &filename) -> bool {
    CHECK_ALIVE_RET(self, false);
    string path;
    path.append(platform->scriptEngineState.directory);
    path.append(filename);

    auto img = nall::image();
    if (!img.loadPNG(path)) {
      return false;
    }
    self.moveIcon(std::move(img));
    return true;
  }));
  HIRO_GETTER0(Canvas, "Alignment get_alignment() property",     hiro::Alignment, alignment);
  HIRO_GETTER0(Canvas, "Color get_color() property",             hiro::Color,     color);
  HIRO_SETTER0(Canvas, "void set_alignment(Alignment) property", hiro::Alignment, setAlignment);
  HIRO_SETTER0(Canvas, "void set_color(Color) property",         hiro::Color, setColor);
  // allocates a new icon:
  //REG_LAMBDA(Canvas, "void set_size(Size &in) property",         ([](hiro::Canvas& self, hiro::Size* size) { self.setSize(*size); }));

  // CheckLabel
  EXPOSE_HIRO_VALUE(CheckLabel);
  EXPOSE_HIRO_OBJECT(CheckLabel);
  EXPOSE_HIRO_SIZABLE(CheckLabel);
  EXPOSE_HIRO_WIDGET(CheckLabel);
  REG_LAMBDA(CheckLabel, "void set_text(const string &in text) property", ([](hiro::CheckLabel& self, const string &text) { self.setText(text); }));
  REG_LAMBDA(CheckLabel, "string get_text() property",                    ([](hiro::CheckLabel& self) { return self.text(); }));
  REG_LAMBDA(CheckLabel, "void set_checked(bool checked) property",       ([](hiro::CheckLabel& self, bool checked) { self.setChecked(checked); }));
  REG_LAMBDA(CheckLabel, "bool get_checked() property",                   ([](hiro::CheckLabel& self) { return self.checked(); }));
  REG_LAMBDA(CheckLabel, "void doToggle()",                               ([](hiro::CheckLabel& self) { self.doToggle(); }));
  REG_LAMBDA(CheckLabel, "void onToggle(Callback @cb)",                   ([](hiro::CheckLabel& self, asIScriptFunction *cb) { self.onToggle(Callback(cb)); }));

  // ComboButtonItem
  EXPOSE_HIRO_VALUE(ComboButtonItem);
  EXPOSE_HIRO_OBJECT(ComboButtonItem);
  REG_LAMBDA(ComboButtonItem, "void set_text(const string &in text) property", ([](hiro::ComboButtonItem& self, const string &text) { self.setText(text); }));
  REG_LAMBDA(ComboButtonItem, "string get_text() property",                    ([](hiro::ComboButtonItem& self) { return self.text(); }));
  REG_LAMBDA(ComboButtonItem, "string get_selected() property",                ([](hiro::ComboButtonItem& self) { return self.selected(); }));
  REG_LAMBDA(ComboButtonItem, "void setSelected()",                            ([](hiro::ComboButtonItem& self) { self.setSelected(); }));
  // auto icon() const -> image;
  // auto setIcon(const image& icon = {}) -> type&;

  // ComboButton
  EXPOSE_HIRO_VALUE(ComboButton);
  EXPOSE_HIRO_OBJECT(ComboButton);
  EXPOSE_HIRO_SIZABLE(ComboButton);
  EXPOSE_HIRO_WIDGET(ComboButton);
  REG_LAMBDA(ComboButton, "void append(const ComboButtonItem &in item)",    ([](hiro::ComboButton& self, hiro::ComboButtonItem *item) { self.append(*item); }));
  REG_LAMBDA(ComboButton, "ComboButtonItem get_opIndex(uint i) property",   ([](hiro::ComboButton& self, uint i) { return hiro::ComboButtonItem(self.item(i)); }));
  REG_LAMBDA(ComboButton, "uint count()",                                   ([](hiro::ComboButton& self) { return self.itemCount(); }));
  REG_LAMBDA(ComboButton, "void doChange()",                                ([](hiro::ComboButton& self) { self.doChange(); }));
  REG_LAMBDA(ComboButton, "void remove(const ComboButtonItem &in item)",    ([](hiro::ComboButton& self, hiro::ComboButtonItem *item) { self.remove(*item); }));
  REG_LAMBDA(ComboButton, "void onChange(Callback @cb)",                    ([](hiro::ComboButton& self, asIScriptFunction *cb) { self.onChange(Callback(cb)); }));
  REG_LAMBDA(ComboButton, "void reset()",                                   ([](hiro::ComboButton& self) { self.reset(); }));
  REG_LAMBDA(ComboButton, "ComboButtonItem get_selected() property",        ([](hiro::ComboButton& self) { return hiro::ComboButtonItem(self.selected()); }));

  // HorizontalSlider
  EXPOSE_HIRO_VALUE(HorizontalSlider);
  EXPOSE_HIRO_OBJECT(HorizontalSlider);
  EXPOSE_HIRO_SIZABLE(HorizontalSlider);
  EXPOSE_HIRO_WIDGET(HorizontalSlider);
  REG_LAMBDA(HorizontalSlider, "uint get_length() property",                ([](hiro::HorizontalSlider& self) { return self.length(); }));
  REG_LAMBDA(HorizontalSlider, "void set_length(uint length) property",     ([](hiro::HorizontalSlider& self, uint length) { self.setLength(length); }));
  REG_LAMBDA(HorizontalSlider, "uint get_position() property",              ([](hiro::HorizontalSlider& self) { return self.position(); }));
  REG_LAMBDA(HorizontalSlider, "void set_position(uint position) property", ([](hiro::HorizontalSlider& self, uint length) { self.setPosition(length); }));
  REG_LAMBDA(HorizontalSlider, "void doChange()",                           ([](hiro::HorizontalSlider& self) { self.doChange(); }));
  REG_LAMBDA(HorizontalSlider, "void onChange(Callback @cb)",               ([](hiro::HorizontalSlider& self, asIScriptFunction *cb) { self.onChange(Callback(cb)); }));

  // SNESCanvas
  EXPOSE_VALUE_CDA(SNESCanvas, GUI::SNESCanvas, GUI::sSNESCanvas);
  EXPOSE_OBJECT(SNESCanvas, GUI::SNESCanvas);
  EXPOSE_SIZABLE(SNESCanvas, GUI::SNESCanvas);
  REG_LAMBDA(SNESCanvas, "void set_size(Size &in size) property",               ([](GUI::SNESCanvas& self, hiro::Size &size) { self.setSize(size); }));
  REG_LAMBDA(SNESCanvas, "uint8 get_luma() property",                           ([](GUI::SNESCanvas& self) { return self.luma(); }));
  REG_LAMBDA(SNESCanvas, "void set_luma(uint8 luma) property",                  ([](GUI::SNESCanvas& self, uint8 value) { self.set_luma(value); }));
  REG_LAMBDA(SNESCanvas, "Alignment get_alignment() property",                 ([](GUI::SNESCanvas& self) { return self.alignment(); }));
  REG_LAMBDA(SNESCanvas, "void set_alignment(const Alignment &in) property",    ([](GUI::SNESCanvas& self, hiro::Alignment &alignment) { self.setAlignment(alignment); }));
  REG_LAMBDA(SNESCanvas, "void setAlignment(float horizontal, float vertical)", ([](GUI::SNESCanvas& self, float horizontal, float vertical) { self.setAlignment(horizontal, vertical); }));
  REG_LAMBDA(SNESCanvas, "void setCollapsible(bool)",    ([](GUI::SNESCanvas& self, bool value) { self.setCollapsible(value); }));
  REG_LAMBDA(SNESCanvas, "void update()",                ([](GUI::SNESCanvas& self) { self.update(); }));
  REG_LAMBDA(SNESCanvas, "void fill(uint16)",            ([](GUI::SNESCanvas& self, uint16 color) { self.fill(color); }));
  REG_LAMBDA(SNESCanvas, "void pixel(int, int, uint16)", ([](GUI::SNESCanvas& self, int x, int y, uint16 color) { self.pixel(x, y, color); }));
  REG_LAMBDA(SNESCanvas, "void draw_sprite_4bpp(int, int, uint, uint, uint, const array<uint16> &in, const array<uint16> &in)",
             ([](GUI::SNESCanvas& self, int x, int y, uint c, uint width, uint height, const CScriptArray *tile_data, const CScriptArray *palette_data) {
               self.draw_sprite_4bpp(x, y, c, width, height, tile_data, palette_data);
             })
  );
  // TODO: draw_sprite_8bpp
}
