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

  // shared pointer interface to mSNESCanvas:
  struct SNESCanvas : shared_pointer<mSNESCanvas> {
    SNESCanvas() : shared_pointer<mSNESCanvas>(new mSNESCanvas, [](auto p) {
      p->unbind();
      delete p;
    }) {
      (*this)->bind(*this);
    }

    auto self() const -> mSNESCanvas& { return (mSNESCanvas&)operator*(); }

    // Object:
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
    auto remove() { self().remove(); }
    auto setEnabled(bool enabled = true) { self().setEnabled(enabled); }
    auto setFocused() { self().setFocused(); }
    auto setFont(const hiro::Font& font = {}) { self().setFont(font); }
    auto setVisible(bool visible = true) { self().setVisible(visible); }
    auto visible(bool recursive = false) const { return self().visible(recursive); }

    // Sizable:
    auto collapsible() const { return self().collapsible(); }
    auto layoutExcluded() const { return self().layoutExcluded(); }
    auto doSize() const { return self().doSize(); }
    auto geometry() const { return self().geometry(); }
    auto minimumSize() const { return self().minimumSize(); }
    auto onSize(const function<void ()>& callback = {}) { self().onSize(callback); }
    auto setCollapsible(bool collapsible = true) { self().setCollapsible(collapsible); }
    auto setLayoutExcluded(bool layoutExcluded = true) { self().setLayoutExcluded(layoutExcluded); }
    auto setGeometry(hiro::Geometry geometry) { self().setGeometry(geometry); }


    auto setAlignment(float horizontal, float vertical) -> void {
      self().setAlignment(hiro::Alignment{horizontal, vertical});
    }

    auto alignment() -> hiro::Alignment* {
      return new hiro::Alignment(self().alignment());
    }

    auto setAlignment(hiro::Alignment& alignment) -> void {
      self().setAlignment(alignment);
    }

    auto setSize(hiro::Size &size) -> void {
      self().setSize(size);
    }

    auto setPosition(float x, float y) -> void {
      self().setGeometry(hiro::Geometry(
        hiro::Position(x, y),
        self().geometry().size()
      ));
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

#ifndef DISABLE_HIRO
#  define GUI_EXPOSE_SHARED_PTR(name, className, mClassName) EXPOSE_SHARED_PTR(name, className, mClassName)
#else
#  define GUI_EXPOSE_SHARED_PTR(name, className, mClassName) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_ADDREF,  "void f()", asFUNCTION(+([](className& self){ self._addRef(); })), asCALL_CDECL_OBJFIRST); assert( r >= 0 ); \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_RELEASE, "void f()", asFUNCTION(+([](className& self){ self._releaseRef(); })), asCALL_CDECL_OBJFIRST); assert( r >= 0 )
#endif

auto RegisterGUI(asIScriptEngine *e) -> void {
  int r;

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

  // Register reference types:
  REG_REF_NOCOUNT(Attributes);
  REG_REF_TYPE(Window);
  REG_REF_TYPE(VerticalLayout);
  REG_REF_TYPE(HorizontalLayout);
  REG_REF_TYPE(Group);
  REG_REF_TYPE(LineEdit);
  REG_REF_TYPE(Label);
  REG_REF_TYPE(Button);
  REG_REF_TYPE(Canvas);
  REG_REF_TYPE(SNESCanvas);
  REG_REF_TYPE(CheckLabel);
  REG_REF_TYPE(ComboButtonItem);
  REG_REF_TYPE(ComboButton);
  REG_REF_TYPE(HorizontalSlider);

  // value types:
  REG_REF_SCOPED(Alignment, hiro::Alignment);
  REG_REF_SCOPED(Color, hiro::Color);
  REG_REF_SCOPED(Font, hiro::Font);
  REG_REF_SCOPED(Position, hiro::Position);
  REG_REF_SCOPED(Size, hiro::Size);
  REG_REF_SCOPED(Geometry, hiro::Geometry);

#define EXPOSE_HIRO(name) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_FACTORY, #name "@ f()", asFUNCTION( +([]{ return new hiro::name; }) ), asCALL_CDECL); assert(r >= 0); \
  GUI_EXPOSE_SHARED_PTR(name, hiro::name, hiro::m##name)

  // Object:
#define EXPOSE_OBJECT(name, className) \
  REG_LAMBDA(name, "Attributes &get_attributes() property",       ([](className* self) { return self; })); \
  REG_LAMBDA(name, "void set_font(const Font &in font) property", ([](className* self, hiro::Font &font){ self->setFont(font); })); \
  REG_LAMBDA(name, "void set_visible(bool visible) property",     ([](className* self, bool visible)    { self->setVisible(visible); })); \
  REG_LAMBDA(name, "bool get_enabled() property",                 ([](className* self) { return self->enabled(false); })); \
  REG_LAMBDA(name, "bool get_enabled_recursive() property",       ([](className* self) { return self->enabled(true); })); \
  REG_LAMBDA(name, "bool get_focused() property",                 ([](className* self) { return self->focused(); })); \
  REG_LAMBDA(name, "Font@ get_font() property",                   ([](className* self) { return new hiro::Font(self->font(false)); })); \
  REG_LAMBDA(name, "Font@ get_font_recursive() property",         ([](className* self) { return new hiro::Font(self->font(true)); })); \
  REG_LAMBDA(name, "int get_offset() property",                   ([](className* self) { return self->offset(); })); \
  REG_LAMBDA(name, "bool get_visible() property",                 ([](className* self) { return self->visible(false); })); \
  REG_LAMBDA(name, "bool get_visible_recursive() property",       ([](className* self) { return self->visible(true); })); \
  REG_LAMBDA(name, "void set_enabled(bool enabled) property",     ([](className* self, bool enabled) { self->setEnabled(enabled); })); \
  REG_LAMBDA(name, "void remove()",         ([](className* self) { return self->remove(); })); \
  REG_LAMBDA(name, "void setFocused()",     ([](className* self) { self->setFocused(); }))

  //REG_LAMBDA(name, "Object get_parent()",                        ([](className* self) { return self->parent(); })); \

#define EXPOSE_HIRO_OBJECT(name) EXPOSE_OBJECT(name, hiro::name)

  // Sizable:
#define EXPOSE_SIZABLE(name, className) \
  REG_LAMBDA(name, "Geometry@ get_geometry() property",              ([](className* self) { return new hiro::Geometry(self->geometry()); })); \
  REG_LAMBDA(name, "void set_geometry(const Geometry &in) property", ([](className* self, hiro::Geometry& geometry) { self->setGeometry(geometry); })); \
  REG_LAMBDA(name, "bool get_collapsible() property",                ([](className* self) { return self->collapsible(); })); \
  REG_LAMBDA(name, "void set_collapsible(bool) property",            ([](className* self, bool collapsible) { self->setCollapsible(collapsible); })); \
  REG_LAMBDA(name, "bool get_layoutExcluded() property",             ([](className* self) { return self->layoutExcluded(); })); \
  REG_LAMBDA(name, "void set_layoutExcluded(bool) property",         ([](className* self, bool layoutExcluded) { self->setLayoutExcluded(layoutExcluded); })); \
  REG_LAMBDA(name, "Size@ get_minimumSize() property",               ([](className* self) { return new hiro::Size(self->minimumSize()); })); \
  REG_LAMBDA(name, "void setPosition(float, float)",                 ([](className* self, float x, float y) { \
    self->setGeometry(hiro::Geometry(hiro::Position(x, y), self->geometry().size())); \
  })); \
  REG_LAMBDA(name, "void doSize()",                               ([](className* self) { self->doSize(); })); \
  REG_LAMBDA(name, "void onSize(Callback @callback)",             ([](className* self, asIScriptFunction *cb) { self->onSize(Callback(cb)); }));


#define EXPOSE_HIRO_SIZABLE(name) EXPOSE_SIZABLE(name, hiro::name)

  // Widget:
#define EXPOSE_WIDGET(name, className) \
  REG_LAMBDA(name, "string get_toolTip() property",                 ([](className* self) { return self->toolTip(); })); \
  REG_LAMBDA(name, "void   set_toolTip(const string &in) property", ([](className* self, string &value) { self->setToolTip(value); }))

#define EXPOSE_HIRO_WIDGET(name) EXPOSE_WIDGET(name, hiro::name)

  REG_LAMBDA(Attributes, "string get_opIndex(const string &in) property",
    ([](hiro::Object* self, const string &name) { return self->attribute(name); }));
  REG_LAMBDA(Attributes, "void set_opIndex(const string &in, const string &in) property",
    ([](hiro::Object* self, const string &name, const string &value) { self->setAttribute(name, value); }));

  // Alignment value type:
  REG_LAMBDA_BEHAVIOUR(Alignment, asBEHAVE_FACTORY, "Alignment@ f(float, float)", ([](float h, float v) { return new hiro::Alignment(h, v); }));
  REG_LAMBDA(Alignment, "float get_horizontal() property",      ([](hiro::Alignment& self) -> float { return self.horizontal(); }));
  REG_LAMBDA(Alignment, "void  set_horizontal(float) property", ([](hiro::Alignment& self, float value)    { self.setHorizontal(value); }));
  REG_LAMBDA(Alignment, "float get_vertical() property",        ([](hiro::Alignment& self) -> float { return self.vertical(); }));
  REG_LAMBDA(Alignment, "void  set_vertical(float) property",   ([](hiro::Alignment& self, float value)    { self.setVertical(value); }));

  // Position value type:
  REG_LAMBDA_BEHAVIOUR(Position, asBEHAVE_FACTORY, "Position@ f(float, float)", ([](float x, float y){ return new hiro::Position(x, y); }));
  REG_LAMBDA(Position, "float get_x() property",      ([](hiro::Position& self) -> float { return self.x(); }));
  REG_LAMBDA(Position, "void  set_x(float) property", ([](hiro::Position& self, float value)    { self.setX(value); }));
  REG_LAMBDA(Position, "float get_y() property",      ([](hiro::Position& self) -> float { return self.y(); }));
  REG_LAMBDA(Position, "void  set_y(float) property", ([](hiro::Position& self, float value)    { self.setY(value); }));

  // Size value type:
  REG_LAMBDA_BEHAVIOUR(Size, asBEHAVE_FACTORY, "Size@ f(float, float)", ([](float width, float height){ return new hiro::Size(width, height); }));
  REG_LAMBDA(Size, "float get_width() property",       ([](hiro::Size& self) -> float { return self.width(); }));
  REG_LAMBDA(Size, "void  set_width(float) property",  ([](hiro::Size& self, float value)    { self.setWidth(value); }));
  REG_LAMBDA(Size, "float get_height() property",      ([](hiro::Size& self) -> float { return self.height(); }));
  REG_LAMBDA(Size, "void  set_height(float) property", ([](hiro::Size& self, float value)    { self.setHeight(value); }));

  // Geometry value type:
  REG_LAMBDA_BEHAVIOUR(Geometry, asBEHAVE_FACTORY, "Geometry@ f(const Position &in, const Size &in)", ([](hiro::Position &position, hiro::Size &size){ return new hiro::Geometry(position, size); }));
  REG_LAMBDA(Geometry, "Position@ get_position() property",                   ([](hiro::Geometry* self) { return new hiro::Position(self->position()); }));
  REG_LAMBDA(Geometry, "void      set_position(const Position &in) property", ([](hiro::Geometry* self, hiro::Position& position) { self->setPosition(position); }));
  REG_LAMBDA(Geometry, "Size@ get_size() property",                           ([](hiro::Geometry* self) { return new hiro::Size(self->size()); }));
  REG_LAMBDA(Geometry, "void  set_size(const Size &in) property",             ([](hiro::Geometry* self, hiro::Size& size)     { self->setSize(size); }));

  // Color value type:
  REG_LAMBDA_BEHAVIOUR(Color, asBEHAVE_FACTORY, "Color@ f(int red, int green, int blue, int alpha = 255)", ([](int r, int g, int b, int a){ return new hiro::Color(r,g,b,a); }));
  //r = e->RegisterObjectMethod("Color", "Color@ setColor(int red, int green, int blue, int alpha = 255)", asMETHODPR(hiro::Color, setColor, (int, int, int, int), hiro::Color&), asCALL_THISCALL); assert(r >= 0);
  //r = e->RegisterObjectMethod("Color", "Color@ setValue(uint32 value)", asMETHOD(hiro::Color, setValue), asCALL_THISCALL); assert(r >= 0);
  //r = e->RegisterObjectMethod("Color", "uint8 get_alpha() property",    asMETHOD(hiro::Color, alpha), asCALL_THISCALL); assert(r >= 0);
  //r = e->RegisterObjectMethod("Color", "uint8 get_blue() property",     asMETHOD(hiro::Color, blue), asCALL_THISCALL); assert(r >= 0);
  //r = e->RegisterObjectMethod("Color", "uint8 get_green() property",    asMETHOD(hiro::Color, green), asCALL_THISCALL); assert(r >= 0);
  //r = e->RegisterObjectMethod("Color", "uint8 get_red() property",      asMETHOD(hiro::Color, red), asCALL_THISCALL); assert(r >= 0);
  //r = e->RegisterObjectMethod("Color", "Color@ set_alpha(int alpha)",   asMETHOD(hiro::Color, setAlpha), asCALL_THISCALL); assert(r >= 0);
  //r = e->RegisterObjectMethod("Color", "Color@ set_blue(int blue)",     asMETHOD(hiro::Color, setBlue), asCALL_THISCALL); assert(r >= 0);
  //r = e->RegisterObjectMethod("Color", "Color@ set_green(int green)",   asMETHOD(hiro::Color, setGreen), asCALL_THISCALL); assert(r >= 0);
  //r = e->RegisterObjectMethod("Color", "Color@ set_red(int red)",       asMETHOD(hiro::Color, setRed), asCALL_THISCALL); assert(r >= 0);

  // Font value type:
  REG_LAMBDA_BEHAVIOUR(Font, asBEHAVE_FACTORY, "Font@ f(const string &in family, float size = 0.0)", ([](string &family, float size){ return new hiro::Font(family, size); }));
  REG_LAMBDA(Font, "bool   get_bold() property",                          ([](hiro::Font* self) { return self->bold(); }));
  REG_LAMBDA(Font, "string get_family() property",                        ([](hiro::Font* self) { return self->family(); }));
  REG_LAMBDA(Font, "bool   get_italic() property",                        ([](hiro::Font* self) { return self->italic(); }));
  REG_LAMBDA(Font, "float  get_size() property",                          ([](hiro::Font* self) { return self->size(); }));
  REG_LAMBDA(Font, "void   set_bold(bool bold) property",                 ([](hiro::Font* self, bool bold) { self->setBold(bold); }));
  REG_LAMBDA(Font, "void   set_family(const string &in family) property", ([](hiro::Font* self, string &family) { self->setFamily(family); }));
  REG_LAMBDA(Font, "void   set_italic(bool italic) property",             ([](hiro::Font* self, bool italic) { self->setItalic(italic); }));
  REG_LAMBDA(Font, "void   set_size(float size) property",                ([](hiro::Font* self, float size) { self->setSize(size); }));

  REG_LAMBDA(Font, "Size@ measure(const string &in text)", ([](hiro::Font* self, string &text) { return new hiro::Size(self->size(text)); }));
  REG_LAMBDA(Font, "void reset()", ([](hiro::Font* self) { self->reset(); }));

  // Font names:
  r = e->RegisterGlobalProperty("string Sans",  (void *) &hiro::Font::Sans); assert(r >= 0);
  r = e->RegisterGlobalProperty("string Serif", (void *) &hiro::Font::Serif); assert(r >= 0);
  r = e->RegisterGlobalProperty("string Mono",  (void *) &hiro::Font::Mono); assert(r >= 0);

  // Window
  r = e->RegisterObjectBehaviour("Window", asBEHAVE_FACTORY, "Window@ f()", asFUNCTION( +([]{
    auto window = new hiro::Window;
#ifndef DISABLE_HIRO
    // keep a reference for later destruction when unloading script:
    ::SuperFamicom::script.windows.append(*window);
#endif
    return window;
  }) ), asCALL_CDECL); assert(r >= 0);
  GUI_EXPOSE_SHARED_PTR(Window, hiro::Window, hiro::mWindow);

  r = e->RegisterObjectBehaviour("Window", asBEHAVE_FACTORY, "Window@ f(float rx, float ry, bool relative)", asFUNCTION(+([](float x, float y, bool relative) {
    auto window = new hiro::Window;
#ifndef DISABLE_HIRO
    // keep a reference for later destruction when unloading script:
    ::SuperFamicom::script.windows.append(*window);
    if (relative) {
      window->setPosition(platform->presentationWindow(), hiro::Position{x, y});
    } else {
      window->setPosition(hiro::Position{x, y});
    }
#endif
    return window;
  })), asCALL_CDECL); assert(r >= 0);
  EXPOSE_HIRO_OBJECT(Window);
  REG_LAMBDA(Window, "void append(const ? &in sizable)", ([](hiro::Window* self, hiro::Sizable* sizable, int sizableTypeId){ self->append(*sizable); }));
  REG_LAMBDA(Window, "void remove(const ? &in sizable)", ([](hiro::Window* self, hiro::Sizable* sizable, int sizableTypeId){ self->remove(*sizable); }));
  REG_LAMBDA(Window, "void reset()",                     ([](hiro::Window* self){ self->reset(); }));

  REG_LAMBDA(Window, "Color@ get_backgroundColor() property", ([](hiro::Window* self) { return new hiro::Color(self->backgroundColor()); }));
  REG_LAMBDA(Window, "bool get_dismissable() property",       ([](hiro::Window* self) { return self->dismissable(); }));
  REG_LAMBDA(Window, "bool get_fullScreen() property",        ([](hiro::Window* self) { return self->fullScreen(); }));
  REG_LAMBDA(Window, "bool get_maximized() property",         ([](hiro::Window* self) { return self->maximized(); }));
  REG_LAMBDA(Window, "Size@ get_maximumSize() property",      ([](hiro::Window* self) { return new hiro::Size(self->maximumSize()); }));
  REG_LAMBDA(Window, "bool get_minimized() property",         ([](hiro::Window* self) { return self->minimized(); }));
  REG_LAMBDA(Window, "Size@ get_minimumSize() property",      ([](hiro::Window* self) { return new hiro::Size(self->minimumSize()); }));
  REG_LAMBDA(Window, "bool get_modal() property",             ([](hiro::Window* self) { return self->modal(); }));
  REG_LAMBDA(Window, "bool get_resizable() property",         ([](hiro::Window* self) { return self->resizable(); }));
  REG_LAMBDA(Window, "bool get_sizable() property",           ([](hiro::Window* self) { return self->sizable(); }));
  REG_LAMBDA(Window, "string get_title() property",           ([](hiro::Window* self) { return self->title(); }));

  REG_LAMBDA(Window, "Geometry@ get_frameGeometry() property",([](hiro::Window* self) { return new hiro::Geometry(self->frameGeometry()); }));
  REG_LAMBDA(Window, "Geometry@ get_geometry() property",     ([](hiro::Window* self) { return new hiro::Geometry(self->geometry()); }));

  REG_LAMBDA(Window, "void set_backgroundColor(const Color &in color) property", ([](hiro::Window* self, hiro::Color &color)  { self->setBackgroundColor(color); }));
  REG_LAMBDA(Window, "void set_dismissable(bool dismissable) property",          ([](hiro::Window* self, bool dismissable)    { self->setDismissable(dismissable); }));
  REG_LAMBDA(Window, "void set_fullScreen(bool fullScreen) property",            ([](hiro::Window* self, bool fullScreen)     { self->setFullScreen(fullScreen); }));
  REG_LAMBDA(Window, "void set_maximized(bool maximized) property",              ([](hiro::Window* self, bool maximized)      { self->setMaximized(maximized); }));
  REG_LAMBDA(Window, "void set_maximumSize(const Size &in size) property",       ([](hiro::Window* self, hiro::Size &size)    { self->setMaximumSize(size); }));
  REG_LAMBDA(Window, "void set_minimized(bool minimized) property",              ([](hiro::Window* self, bool minimized)      { self->setMinimized(minimized); }));
  REG_LAMBDA(Window, "void set_minimumSize(const Size &in size) property",       ([](hiro::Window* self, hiro::Size &size)    { self->setMinimumSize(size); }));
  REG_LAMBDA(Window, "void set_modal(bool modal) property",                      ([](hiro::Window* self, bool modal)          { self->setModal(modal); }));
  REG_LAMBDA(Window, "void set_resizable(bool resizable) property",              ([](hiro::Window* self, bool resizable)      { self->setResizable(resizable); }));
  REG_LAMBDA(Window, "void set_title(const string &in title) property",          ([](hiro::Window* self, const string &title) { self->setTitle(title); }));
  REG_LAMBDA(Window, "void set_size(const Size &in size) property",              ([](hiro::Window* self, hiro::Size &size)    { self->setSize(size); }));

  REG_LAMBDA(Window, "void onSize(Callback @cb)",             ([](hiro::Window *self, asIScriptFunction *cb) { self->onSize(Callback(cb)); }));

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
  EXPOSE_HIRO(VerticalLayout);
  EXPOSE_HIRO_OBJECT(VerticalLayout);
  EXPOSE_HIRO_SIZABLE(VerticalLayout);
  REG_LAMBDA(VerticalLayout, "void append(const ? &in sizable, Size &in size, float spacing = 5.0)",
    ([](hiro::VerticalLayout* self, hiro::Sizable *sizable, int sizableTypeId, hiro::Size *size, float spacing){ self->append(*sizable, *size, spacing); }));
  REG_LAMBDA(VerticalLayout, "void remove(const ? &in sizable)",        ([](hiro::VerticalLayout* p, hiro::Sizable* sizable, int sizableTypeId){ p->remove(*sizable); }));
  REG_LAMBDA(VerticalLayout, "void resize()",                           ([](hiro::VerticalLayout* p) { p->resize(); }));
  REG_LAMBDA(VerticalLayout, "void setAlignment(float alignment)",      ([](hiro::VerticalLayout* p, float alignment) { p->setAlignment(alignment); }));
  REG_LAMBDA(VerticalLayout, "void resetAlignment()",                   ([](hiro::VerticalLayout* p) { p->setAlignment(); }));
  REG_LAMBDA(VerticalLayout, "void setPadding(float x, float y)",       ([](hiro::VerticalLayout* p, float x, float y) { p->setPadding(x, y); }));
  REG_LAMBDA(VerticalLayout, "void setSpacing(float spacing = 5.0)",    ([](hiro::VerticalLayout* p, float spacing) { p->setSpacing(spacing); }));

  // HorizontalLayout
  EXPOSE_HIRO(HorizontalLayout);
  EXPOSE_HIRO_OBJECT(HorizontalLayout);
  EXPOSE_HIRO_SIZABLE(HorizontalLayout);
  REG_LAMBDA(HorizontalLayout, "void append(const ? &in sizable, Size &in size, float spacing = 5.0)",
    ([](hiro::HorizontalLayout* self, hiro::Sizable *sizable, int sizableTypeId, hiro::Size *size, float spacing){ self->append(*sizable, *size, spacing); }));
  REG_LAMBDA(HorizontalLayout, "void remove(const ? &in sizable)",        ([](hiro::HorizontalLayout* p, hiro::Sizable* sizable, int sizableTypeId){ p->remove(*sizable); }));
  REG_LAMBDA(HorizontalLayout, "void resize()",                           ([](hiro::HorizontalLayout* p) { p->resize(); }));
  REG_LAMBDA(HorizontalLayout, "void setAlignment(float alignment)",      ([](hiro::HorizontalLayout* p, float alignment) { p->setAlignment(alignment); }));
  REG_LAMBDA(HorizontalLayout, "void resetAlignment()",                   ([](hiro::HorizontalLayout* p) { p->setAlignment(); }));
  REG_LAMBDA(HorizontalLayout, "void setPadding(float x, float y)",       ([](hiro::HorizontalLayout* p, float x, float y) { p->setPadding(x, y); }));
  REG_LAMBDA(HorizontalLayout, "void setSpacing(float spacing = 5.0)",    ([](hiro::HorizontalLayout* p, float spacing) { p->setSpacing(spacing); }));

  // Group
  EXPOSE_HIRO(Group);
  REG_LAMBDA(Group, "void append(const ? &in object)",      ([](hiro::Group* self, hiro::Object *object, int objectTypeId){ self->append(*object); }));
  REG_LAMBDA(Group, "void remove(const ? &in object)",      ([](hiro::Group* self, hiro::Object *object, int objectTypeId){ self->remove(*object); }));
  //REG_LAMBDA(Group, "Group@ get_opIndex(uint i) property",  ([](hiro::Group *p, uint i) { return new hiro::ComboButtonItem(p->object(i)); }));
  REG_LAMBDA(Group, "uint count()",                         ([](hiro::Group *p) { return p->objectCount(); }));

  // LineEdit
  EXPOSE_HIRO(LineEdit);
  EXPOSE_HIRO_OBJECT(LineEdit);
  EXPOSE_HIRO_SIZABLE(LineEdit);
  EXPOSE_HIRO_WIDGET(LineEdit);
  REG_LAMBDA(LineEdit, "string get_text() property",                    ([](hiro::LineEdit* self){ return self->text(); }));
  REG_LAMBDA(LineEdit, "void set_text(const string &in text) property", ([](hiro::LineEdit* self, string &text){ self->setText(text); }));
  REG_LAMBDA(LineEdit, "void onChange(Callback @cb)",                   ([](hiro::LineEdit* self, asIScriptFunction* cb) { self->onChange(Callback(cb)); }));

  // Label
  EXPOSE_HIRO(Label);
  EXPOSE_HIRO_OBJECT(Label);
  EXPOSE_HIRO_SIZABLE(Label);
  EXPOSE_HIRO_WIDGET(Label);
  REG_LAMBDA(Label, "Alignment@ get_alignment() property",                ([](hiro::Label* self){ return new hiro::Alignment(self->alignment()); }));
  REG_LAMBDA(Label, "Color@ get_backgroundColor() property",              ([](hiro::Label* self){ return new hiro::Color(self->backgroundColor()); }));
  REG_LAMBDA(Label, "Color@ get_foregroundColor() property",              ([](hiro::Label* self){ return new hiro::Color(self->foregroundColor()); }));
  REG_LAMBDA(Label, "string get_text() property",                         ([](hiro::Label* self){ return self->text(); }));
  REG_LAMBDA(Label, "void set_alignment(const Alignment &in) property",   ([](hiro::Label* self, const hiro::Alignment &alignment){ self->setAlignment(alignment); }));
  REG_LAMBDA(Label, "void set_backgroundColor(const Color &in) property", ([](hiro::Label* self, const hiro::Color &color){ self->setBackgroundColor(color); }));
  REG_LAMBDA(Label, "void set_foregroundColor(const Color &in) property", ([](hiro::Label* self, const hiro::Color &color){ self->setForegroundColor(color); }));
  REG_LAMBDA(Label, "void set_text(const string &in) property",           ([](hiro::Label* self, string &text){ self->setText(text); }));

  // Button
  EXPOSE_HIRO(Button);
  EXPOSE_HIRO_OBJECT(Button);
  EXPOSE_HIRO_SIZABLE(Button);
  EXPOSE_HIRO_WIDGET(Button);
  REG_LAMBDA(Button, "string get_text() property",                    ([](hiro::Button* self){ return self->text(); }));
  REG_LAMBDA(Button, "void set_text(const string &in text) property", ([](hiro::Button* self, const string &text){ self->setText(text); }));
  REG_LAMBDA(Button, "void onActivate(Callback @cb)",                 ([](hiro::Button* self, asIScriptFunction* cb){ self->onActivate(Callback(cb)); }));

  // Canvas
  EXPOSE_HIRO(Canvas);
  EXPOSE_HIRO_OBJECT(Canvas);
  EXPOSE_HIRO_SIZABLE(Canvas);
  EXPOSE_HIRO_WIDGET(Canvas);
  REG_LAMBDA(Canvas, "bool loadPNG(const string &in)", ([](hiro::Canvas& self, const string &filename) -> bool {
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
  REG_LAMBDA(Canvas, "Alignment@ get_alignment() property",              ([](hiro::Canvas& self){ return new hiro::Alignment(self.alignment()); }));
  REG_LAMBDA(Canvas, "Color@ get_color() property",                      ([](hiro::Canvas& self){ return new hiro::Color(self.color()); }));
  REG_LAMBDA(Canvas, "void setAlignment(float, float)",                  ([](hiro::Canvas& self, float h, float v) { self.setAlignment(hiro::Alignment(h, v)); }));
  REG_LAMBDA(Canvas, "void set_alignment(const Alignment &in) property", ([](hiro::Canvas& self, hiro::Alignment* value){ self.setAlignment(*value); }));
  REG_LAMBDA(Canvas, "void set_color(const Color &in) property",         ([](hiro::Canvas& self, hiro::Color* value){ self.setColor(*value); }));
  // allocates a new icon:
  //REG_LAMBDA(Canvas, "void set_size(Size &in) property",         ([](hiro::Canvas& self, hiro::Size* size) { self.setSize(*size); }));

  // CheckLabel
  EXPOSE_HIRO(CheckLabel);
  EXPOSE_HIRO_OBJECT(CheckLabel);
  EXPOSE_HIRO_SIZABLE(CheckLabel);
  EXPOSE_HIRO_WIDGET(CheckLabel);
  REG_LAMBDA(CheckLabel, "void set_text(const string &in text) property", ([](hiro::CheckLabel *p, const string &text) { p->setText(text); }));
  REG_LAMBDA(CheckLabel, "string get_text() property",                    ([](hiro::CheckLabel *p) { return p->text(); }));
  REG_LAMBDA(CheckLabel, "void set_checked(bool checked) property",       ([](hiro::CheckLabel *p, bool checked) { p->setChecked(checked); }));
  REG_LAMBDA(CheckLabel, "bool get_checked() property",                   ([](hiro::CheckLabel *p) { return p->checked(); }));
  REG_LAMBDA(CheckLabel, "void doToggle()",                               ([](hiro::CheckLabel *p) { p->doToggle(); }));
  REG_LAMBDA(CheckLabel, "void onToggle(Callback @cb)",                   ([](hiro::CheckLabel *p, asIScriptFunction *cb) { p->onToggle(Callback(cb)); }));

  // ComboButtonItem
  EXPOSE_HIRO(ComboButtonItem);
  EXPOSE_HIRO_OBJECT(ComboButtonItem);
  REG_LAMBDA(ComboButtonItem, "void set_text(const string &in text) property", ([](hiro::ComboButtonItem *p, const string &text) { p->setText(text); }));
  REG_LAMBDA(ComboButtonItem, "string get_text() property",                    ([](hiro::ComboButtonItem *p) { return p->text(); }));
  REG_LAMBDA(ComboButtonItem, "string get_selected() property",                ([](hiro::ComboButtonItem *p) { return p->selected(); }));
  REG_LAMBDA(ComboButtonItem, "void setSelected()",                            ([](hiro::ComboButtonItem *self) { self->setSelected(); }));
  // auto icon() const -> image;
  // auto setIcon(const image& icon = {}) -> type&;

  // ComboButton
  EXPOSE_HIRO(ComboButton);
  EXPOSE_HIRO_OBJECT(ComboButton);
  EXPOSE_HIRO_SIZABLE(ComboButton);
  EXPOSE_HIRO_WIDGET(ComboButton);
  REG_LAMBDA(ComboButton, "void append(ComboButtonItem@ item)",             ([](hiro::ComboButton *p, hiro::ComboButtonItem *item) { p->append(*item); }));
  REG_LAMBDA(ComboButton, "ComboButtonItem@ get_opIndex(uint i) property",  ([](hiro::ComboButton *p, uint i) { return new hiro::ComboButtonItem(p->item(i)); }));
  REG_LAMBDA(ComboButton, "uint count()",                                   ([](hiro::ComboButton *p) { return p->itemCount(); }));
  REG_LAMBDA(ComboButton, "void doChange()",                                ([](hiro::ComboButton *p) { p->doChange(); }));
  REG_LAMBDA(ComboButton, "void remove(ComboButtonItem@ item)",             ([](hiro::ComboButton *p, hiro::ComboButtonItem *item) { p->remove(*item); }));
  REG_LAMBDA(ComboButton, "void onChange(Callback @cb)",                    ([](hiro::ComboButton *p, asIScriptFunction *cb) { p->onChange(Callback(cb)); }));
  REG_LAMBDA(ComboButton, "void reset()",                                   ([](hiro::ComboButton *p) { p->reset(); }));
  REG_LAMBDA(ComboButton, "ComboButtonItem@ get_selected() property",       ([](hiro::ComboButton *p) { return new hiro::ComboButtonItem(p->selected()); }));

  // HorizontalSlider
  EXPOSE_HIRO(HorizontalSlider);
  EXPOSE_HIRO_OBJECT(HorizontalSlider);
  EXPOSE_HIRO_SIZABLE(HorizontalSlider);
  EXPOSE_HIRO_WIDGET(HorizontalSlider);
  REG_LAMBDA(HorizontalSlider, "uint get_length() property",                ([](hiro::HorizontalSlider *p) { return p->length(); }));
  REG_LAMBDA(HorizontalSlider, "void set_length(uint length) property",     ([](hiro::HorizontalSlider *p, uint length) { p->setLength(length); }));
  REG_LAMBDA(HorizontalSlider, "uint get_position() property",              ([](hiro::HorizontalSlider *p) { return p->position(); }));
  REG_LAMBDA(HorizontalSlider, "void set_position(uint position) property", ([](hiro::HorizontalSlider *p, uint length) { p->setPosition(length); }));
  REG_LAMBDA(HorizontalSlider, "void doChange()",                           ([](hiro::HorizontalSlider *p) { p->doChange(); }));
  REG_LAMBDA(HorizontalSlider, "void onChange(Callback @cb)",               ([](hiro::HorizontalSlider *p, asIScriptFunction *cb) { p->onChange(Callback(cb)); }));

  // SNESCanvas
  r = e->RegisterObjectBehaviour("SNESCanvas", asBEHAVE_FACTORY, "SNESCanvas@ f()", asFUNCTION( +([]{ return new GUI::SNESCanvas(); }) ), asCALL_CDECL); assert(r >= 0);
  GUI_EXPOSE_SHARED_PTR(SNESCanvas, GUI::SNESCanvas, GUI::mSNESCanvas);
  EXPOSE_OBJECT(SNESCanvas, GUI::SNESCanvas);
  EXPOSE_SIZABLE(SNESCanvas, GUI::SNESCanvas);
  REG_LAMBDA(SNESCanvas, "void set_size(Size &in size) property",               ([](GUI::SNESCanvas& self, hiro::Size &size) { self.setSize(size); }));
  REG_LAMBDA(SNESCanvas, "uint8 get_luma() property",                           ([](GUI::SNESCanvas& self) { return self.luma(); }));
  REG_LAMBDA(SNESCanvas, "void set_luma(uint8 luma) property",                  ([](GUI::SNESCanvas& self, uint8 value) { self.set_luma(value); }));
  REG_LAMBDA(SNESCanvas, "Alignment@ get_alignment() property",                 ([](GUI::SNESCanvas& self) { return self.alignment(); }));
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
