
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

  struct Canvas : Sizable {
    BindShared(Canvas)
    BindObject(Canvas)
    BindSizable(Canvas)

    Canvas() :
      // Set 15-bit RGB format with 1-bit alpha for PPU-compatible images after lightTable mapping:
      img16(0, 16, 0x8000u, 0x7C00u, 0x03E0u, 0x001Fu),
      img32(0, 32, 0xFF000000u, 0x00FF0000u, 0x0000FF00u, 0x000000FFu)
    {
      self = new hiro::mCanvas();
      self->construct();
    }

    // holds internal SNES 15-bit BGR colors
    image img16;
    // used for final display
    image img32;

    auto setSize(hiro::Size *size) -> void {
      //img16 = image(0, 16, 0x8000u, 0x7C00u, 0x03E0u, 0x001Fu);
      //img32 = image(0, 32, 0xFF000000u, 0x00FF0000u, 0x0000FF00u, 0x000000FFu);
      img16.allocate(size->width(), size->height());
      img32.allocate(size->width(), size->height());
      img16.fill(0x0000u);
      img32.fill(0x00000000u);
      self->setIcon(img32);
    }

    auto setPosition(float x, float y) -> void {
      auto sizable = (operator hiro::mSizable *)();
      sizable->setGeometry(hiro::Geometry(
        hiro::Position(x, y),
        sizable->geometry().size()
      ));
    }

    auto setAlignment(float horizontal, float vertical) -> void {
      self->setAlignment(hiro::Alignment{horizontal, vertical});
    }

    auto setCollapsible(bool collapsible) -> void {
      self->setCollapsible(collapsible);
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
      self->setIcon(img32);
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


#define Constructor(Name) \
    static auto create##Name() -> Name* { return new Name(); }

  // Size value type:
  static auto createSize(void *memory) -> void { new(memory) hiro::Size(); }
  static auto createSizeWH(float width, float height, void *memory) -> void { new(memory) hiro::Size(width, height); }
  static auto destroySize(void *memory) -> void { ((hiro::Size*)memory)->~Size(); }

  // Alignment value type:
  static auto createAlignment(void *memory) -> void { new(memory) hiro::Alignment(); }
  static auto createAlignmentWH(float horizontal, float vertical, void *memory) -> void { new(memory) hiro::Alignment(horizontal, vertical); }
  static auto destroyAlignment(void *memory) -> void { ((hiro::Alignment*)memory)->~Alignment(); }

  // Color value type:
  static auto createColor(void *memory) -> void { new(memory) hiro::Color(); }
  static auto createColorRGBA(int red, int green, int blue, int alpha, void *memory) -> void { new(memory) hiro::Color(red, green, blue, alpha); }
  static auto destroyColor(void *memory) -> void { ((hiro::Color*)memory)->~Color(); }

  // Font value type:
  static auto createFont(string *family, float size, void *memory) -> void { new(memory) hiro::Font(*family, size); }
  static auto destroyFont(void *memory) -> void { ((hiro::Font*)memory)->~Font(); }

  Constructor(Canvas)

#undef Constructor

  static auto newCheckLabel() -> hiro::CheckLabel* {
    return new hiro::CheckLabel;
  }

  struct any{};

  static auto hiroAddRef(shared_pointer<any> &p) {
    ++p.manager->strong;
    //printf("%p ++ -> %d\n", (void*)&p, p.references());
  }

  static auto hiroRelease(shared_pointer<any> &p) {
    //printf("%p -- -> %d\n", (void*)&p, p.references() - 1);
    if (p.manager && p.manager->strong) {
      if (p.manager->strong == 1) {
        if(p.manager->deleter) {
          p.manager->deleter(p.manager->pointer);
        } else {
          delete p.manager->pointer;
        }
        p.manager->pointer = nullptr;
      }
      if (--p.manager->strong == 0) {
        delete p.manager;
        p.manager = nullptr;
      }
    }
  }

  struct Callback {
    asIScriptFunction *cb;

    Callback(asIScriptFunction *cb) : cb(cb) {
      cb->AddRef();
    }
    Callback(const Callback& other) : cb(other.cb) {
      cb->AddRef();
    }
    ~Callback() {
      cb->Release();
      cb = nullptr;
    }

    auto operator()() -> void {
      auto ctx = ::SuperFamicom::script.context;
      ctx->Prepare(cb);
      ctx->Execute();
    }
  };

};

auto RegisterGUI(asIScriptEngine *e) -> void {
  int r;

#define REG_LAMBDA(name, defn, lambda) r = e->RegisterObjectMethod(#name, defn, asFUNCTION(+lambda), asCALL_CDECL_OBJFIRST); assert( r >= 0 )

#define EXPOSE_HIRO(name) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_FACTORY, #name "@ f()", asFUNCTION( +([]{ return new hiro::name; }) ), asCALL_CDECL); assert(r >= 0); \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_ADDREF, "void f()", asFUNCTION(GUI::hiroAddRef), asCALL_CDECL_OBJFIRST); assert( r >= 0 ); \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_RELEASE, "void f()", asFUNCTION(GUI::hiroRelease), asCALL_CDECL_OBJFIRST); assert( r >= 0 )

#define EXPOSE_HIRO_OBJECT(name) \
  REG_LAMBDA(name, "void set_font(const Font &in font) property", ([](hiro::name* self, hiro::Font &font){ self->setFont(font); })); \
  REG_LAMBDA(name, "void set_visible(bool visible) property",     ([](hiro::name* self, bool visible)    { self->setVisible(visible); })); \
  REG_LAMBDA(name, "bool get_enabled() property",             ([](hiro::name* self) { return self->enabled(false); })); \
  REG_LAMBDA(name, "bool get_enabled_recursive() property",   ([](hiro::name* self) { return self->enabled(true); })); \
  REG_LAMBDA(name, "bool get_focused() property",             ([](hiro::name* self) { return self->focused(); })); \
  REG_LAMBDA(name, "Font &get_font() property",               ([](hiro::name* self) { return &self->font(false); })); \
  REG_LAMBDA(name, "Font &get_font_recursive() property",     ([](hiro::name* self) { return &self->font(true); })); \
  REG_LAMBDA(name, "int get_offset() property",               ([](hiro::name* self) { return self->offset(); })); \
  REG_LAMBDA(name, "bool get_visible() property",             ([](hiro::name* self) { return self->visible(false); })); \
  REG_LAMBDA(name, "bool get_visible_recursive() property",   ([](hiro::name* self) { return self->visible(true); })); \
  REG_LAMBDA(name, "void set_enabled(bool enabled) property", ([](hiro::name* self, bool enabled) { self->setEnabled(enabled); })); \
  REG_LAMBDA(name, "void remove()",         ([](hiro::name* self) { return self->remove(); })); \
  REG_LAMBDA(name, "void setFocused()",     ([](hiro::name* self, bool focused) { self->setFocused(); }))

  //REG_LAMBDA(name, "Object &get_parent()",                        ([](hiro::name* self) { return self->parent(); })); \

#define EXPOSE_HIRO_SIZABLE(name) \
  REG_LAMBDA(name, "bool get_collapsible() property",                         ([](hiro::name* self) { return self->collapsible(); })); \
  REG_LAMBDA(name, "void doSize()",                                           ([](hiro::name* self) { self->doSize(); })); \
  REG_LAMBDA(name, "Size &get_minimumSize() property",                        ([](hiro::name* self) { return &self->minimumSize(); })); \
  REG_LAMBDA(name, "void onSize(Callback @callback)",                         ([](hiro::name* self, asIScriptFunction *cb) { \
    self->onSize([=] { \
      auto ctx = ::SuperFamicom::script.context; \
      ctx->Prepare(cb); \
      ctx->Execute(); \
    }); \
  })); \
  REG_LAMBDA(name, "void set_collapsible(bool collapsible = true) property",  ([](hiro::name* self, bool collapsible) { self->setCollapsible(collapsible); })); \

  // GUI
  r = e->SetDefaultNamespace("GUI"); assert(r >= 0);

  // function types:
  r = e->RegisterFuncdef("void Callback()"); assert(r >= 0);

#define REG_REF_TYPE(name) r = e->RegisterObjectType(#name, 0, asOBJ_REF); assert( r >= 0 )
#define REG_VALUE_TYPE(name) r = e->RegisterObjectType(#name, sizeof(hiro::name), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<hiro::name>()); assert( r >= 0 )

  // Register reference types:
  REG_REF_TYPE(Window);
  REG_REF_TYPE(VerticalLayout);
  REG_REF_TYPE(HorizontalLayout);
  REG_REF_TYPE(LineEdit);
  REG_REF_TYPE(Label);
  REG_REF_TYPE(Button);
  REG_REF_TYPE(Canvas);
  REG_REF_TYPE(CheckLabel);

  // value types:
  REG_VALUE_TYPE(Alignment);
  REG_VALUE_TYPE(Size);
  REG_VALUE_TYPE(Color);
  REG_VALUE_TYPE(Font);

  // Alignment value type:
  r = e->RegisterObjectBehaviour("Alignment", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(GUI::createAlignment), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Alignment", asBEHAVE_CONSTRUCT, "void f(float horizontal, float vertical)", asFUNCTION(GUI::createAlignmentWH), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Alignment", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(GUI::destroyAlignment), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectMethod("Alignment", "float get_horizontal() property", asMETHOD(hiro::Alignment, horizontal), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Alignment", "void set_horizontal(float horizontal) property", asMETHOD(hiro::Alignment, setHorizontal), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Alignment", "float get_vertical() property", asMETHOD(hiro::Alignment, vertical), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Alignment", "void set_vertical(float vertical) property", asMETHOD(hiro::Alignment, setVertical), asCALL_THISCALL); assert(r >= 0);

  // Size value type:
  r = e->RegisterObjectBehaviour("Size", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(GUI::createSize), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Size", asBEHAVE_CONSTRUCT, "void f(float width, float height)", asFUNCTION(GUI::createSizeWH), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Size", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(GUI::destroySize), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectMethod("Size", "float get_width() property", asMETHOD(hiro::Size, width), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Size", "void set_width(float width) property", asMETHOD(hiro::Size, setWidth), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Size", "float get_height() property", asMETHOD(hiro::Size, height), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Size", "void set_height(float height) property", asMETHOD(hiro::Size, setHeight), asCALL_THISCALL); assert(r >= 0);

  // Color value type:
  r = e->RegisterObjectBehaviour("Color", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(GUI::createColor), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Color", asBEHAVE_CONSTRUCT, "void f(int red, int green, int blue, int alpha = 255)", asFUNCTION(GUI::createColorRGBA), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Color", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(GUI::destroyColor), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "Color& setColor(int red, int green, int blue, int alpha = 255)", asMETHODPR(hiro::Color, setColor, (int, int, int, int), hiro::Color&), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "Color& setValue(uint32 value)", asMETHOD(hiro::Color, setValue), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "uint8 get_alpha() property",    asMETHOD(hiro::Color, alpha), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "uint8 get_blue() property",     asMETHOD(hiro::Color, blue), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "uint8 get_green() property",    asMETHOD(hiro::Color, green), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "uint8 get_red() property",      asMETHOD(hiro::Color, red), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "Color& set_alpha(int alpha)",   asMETHOD(hiro::Color, setAlpha), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "Color& set_blue(int blue)",     asMETHOD(hiro::Color, setBlue), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "Color& set_green(int green)",   asMETHOD(hiro::Color, setGreen), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "Color& set_red(int red)",       asMETHOD(hiro::Color, setRed), asCALL_THISCALL); assert(r >= 0);

  // Font value type:
  r = e->RegisterObjectBehaviour("Font", asBEHAVE_CONSTRUCT, "void f(const string &in family, float size = 0.0)", asFUNCTION(GUI::createFont), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Font", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(GUI::destroyFont), asCALL_CDECL_OBJLAST); assert(r >= 0);

  r = e->RegisterGlobalProperty("string Sans", (void *) &hiro::Font::Sans); assert(r >= 0);
  r = e->RegisterGlobalProperty("string Serif", (void *) &hiro::Font::Serif); assert(r >= 0);
  r = e->RegisterGlobalProperty("string Mono", (void *) &hiro::Font::Mono); assert(r >= 0);

  // Window
  EXPOSE_HIRO(Window);
  r = e->RegisterObjectBehaviour("Window", asBEHAVE_FACTORY, "Window@ f(float rx, float ry, bool relative)", asFUNCTION(+([](float x, float y, bool relative) {
    auto self = new hiro::Window;
    if (relative) {
      self->setPosition(platform->presentationWindow(), hiro::Position{x, y});
    } else {
      self->setPosition(hiro::Position{x, y});
    }
    return self;
  })), asCALL_CDECL); assert(r >= 0);
  EXPOSE_HIRO_OBJECT(Window);
  REG_LAMBDA(Window, "void set_title(const string &in title) property",    ([](hiro::Window* self, const string &title){ self->setTitle(title); }));
  REG_LAMBDA(Window, "void set_size(Size &in size) property",              ([](hiro::Window* self, hiro::Size &size){ self->setSize(size); }));
  REG_LAMBDA(Window, "Color& get_backgroundColor() property",              ([](hiro::Window* self){ return &self->backgroundColor(); }));
  REG_LAMBDA(Window, "void set_backgroundColor(Color &in color) property", ([](hiro::Window* self, hiro::Color &color){ self->setBackgroundColor(color); }));
  REG_LAMBDA(Window, "void append(const ? &in sizable)",                   ([](hiro::Window* self, hiro::Sizable* sizable, int sizableTypeId){ self->append(*sizable); }));

  // VerticalLayout
  EXPOSE_HIRO(VerticalLayout);
  EXPOSE_HIRO_OBJECT(VerticalLayout);
  EXPOSE_HIRO_SIZABLE(VerticalLayout);
  REG_LAMBDA(VerticalLayout, "void append(const ? &in sizable, Size &in size)", ([](hiro::VerticalLayout* self, hiro::Sizable *sizable, int sizableTypeId, hiro::Size *size){ self->append(*sizable, *size); }));
  REG_LAMBDA(VerticalLayout, "void resize()",                                   ([](hiro::VerticalLayout* self){ self->resize(); }));

  // HorizontalLayout
  EXPOSE_HIRO(HorizontalLayout);
  EXPOSE_HIRO_OBJECT(HorizontalLayout);
  EXPOSE_HIRO_SIZABLE(HorizontalLayout);
  REG_LAMBDA(HorizontalLayout, "void append(const ? &in sizable, Size &in size)", ([](hiro::HorizontalLayout* self, hiro::Sizable *sizable, int sizableTypeId, hiro::Size *size){ self->append(*sizable, *size); }));
  REG_LAMBDA(HorizontalLayout, "void resize()",                                   ([](hiro::HorizontalLayout* self){ self->resize(); }));

  // LineEdit
  EXPOSE_HIRO(LineEdit);
  EXPOSE_HIRO_OBJECT(LineEdit);
  EXPOSE_HIRO_SIZABLE(LineEdit);
  //EXPOSE_HIRO_WIDGET(LineEdit);
  REG_LAMBDA(LineEdit, "string &get_text() property",                   ([](hiro::LineEdit* self){ return &self->text(); }));
  REG_LAMBDA(LineEdit, "void set_text(const string &in text) property", ([](hiro::LineEdit* self, const string &text){ self->setText(text); }));
  REG_LAMBDA(LineEdit, "void set_on_change(Callback @cb) property",     ([](hiro::LineEdit* self, asIScriptFunction* cb){
    self->onChange([=]{
      auto ctx = ::SuperFamicom::script.context;
      ctx->Prepare(cb);
      ctx->Execute();
    });
  }));

  // Label
  EXPOSE_HIRO(Label);
  EXPOSE_HIRO_OBJECT(Label);
  EXPOSE_HIRO_SIZABLE(Label);
  //EXPOSE_HIRO_WIDGET(Label);
  REG_LAMBDA(Label, "Alignment& get_alignment() property",                        ([](hiro::Label* self){ return &self->alignment(); }));
  REG_LAMBDA(Label, "void set_alignment(const Alignment &in alignment) property", ([](hiro::Label* self, const hiro::Alignment &alignment){ self->setAlignment(alignment); }));
  REG_LAMBDA(Label, "Color& get_backgroundColor() property",                      ([](hiro::Label* self){ return &self->backgroundColor(); }));
  REG_LAMBDA(Label, "void set_backgroundColor(const Color &in color) property",   ([](hiro::Label* self, const hiro::Color &color){ self->setBackgroundColor(color); }));
  REG_LAMBDA(Label, "Color& get_foregroundColor() property",                      ([](hiro::Label* self){ return &self->foregroundColor(); }));
  REG_LAMBDA(Label, "void set_foregroundColor(const Color &in color) property",   ([](hiro::Label* self, const hiro::Color &color){ self->setForegroundColor(color); }));
  REG_LAMBDA(Label, "string &get_text() property",                                ([](hiro::Label* self){ return &self->text(); }));
  REG_LAMBDA(Label, "void set_text(const string &in text) property",              ([](hiro::Label* self, string &text){ self->setText(text); }));

  // Button
  EXPOSE_HIRO(Button);
  EXPOSE_HIRO_OBJECT(Button);
  EXPOSE_HIRO_SIZABLE(Button);
  //EXPOSE_HIRO_WIDGET(Button);
  REG_LAMBDA(Button, "string &get_text() property",                   ([](hiro::Button* self){ return &self->text(); }));
  REG_LAMBDA(Button, "void set_text(const string &in text) property", ([](hiro::Button* self, const string &text){ self->setText(text); }));
  REG_LAMBDA(Button, "void set_on_activate(Callback @cb) property",   ([](hiro::Button* self, asIScriptFunction* cb){
    self->onActivate([=]{
      auto ctx = ::SuperFamicom::script.context;
      ctx->Prepare(cb);
      ctx->Execute();
    });
  }));

  // Canvas
  EXPOSE_HIRO(Canvas);
  EXPOSE_HIRO_OBJECT(Canvas);
  r = e->RegisterObjectMethod("Canvas", "void set_size(Size &in size) property", asMETHOD(GUI::Canvas, setSize), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "uint8 get_luma() property", asMETHOD(GUI::Canvas, luma), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void set_luma(uint8 luma) property", asMETHOD(GUI::Canvas, set_luma), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void setPosition(float x, float y)", asMETHOD(GUI::Canvas, setPosition), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void setAlignment(float horizontal, float vertical)", asMETHOD(GUI::Canvas, setAlignment), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void setCollapsible(bool collapsible)", asMETHOD(GUI::Canvas, setCollapsible), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void update()", asMETHOD(GUI::Canvas, update), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void fill(uint16 color)", asMETHOD(GUI::Canvas, fill), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void pixel(int x, int y, uint16 color)", asMETHOD(GUI::Canvas, pixel), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void draw_sprite_4bpp(int x, int y, uint c, uint width, uint height, const array<uint16> &in tiledata, const array<uint16> &in palette)", asMETHOD(GUI::Canvas, draw_sprite_4bpp), asCALL_THISCALL); assert( r >= 0 );

  // CheckLabel
  EXPOSE_HIRO(CheckLabel);
  EXPOSE_HIRO_OBJECT(CheckLabel);
  EXPOSE_HIRO_SIZABLE(CheckLabel);
  //EXPOSE_HIRO_WIDGET(CheckLabel);
  REG_LAMBDA(CheckLabel, "void set_text(const string &in text) property", ([](hiro::CheckLabel *p, const string &text) { p->setText(text); }));
  REG_LAMBDA(CheckLabel, "string &get_text() property",                   ([](hiro::CheckLabel *p) { return new string(p->text()); }));
  REG_LAMBDA(CheckLabel, "void set_checked(bool checked) property",       ([](hiro::CheckLabel *p, bool checked) { p->setChecked(checked); }));
  REG_LAMBDA(CheckLabel, "bool get_checked() property",                   ([](hiro::CheckLabel *p) { return p->checked(); }));
  REG_LAMBDA(CheckLabel, "void doToggle()",                               ([](hiro::CheckLabel *p) { p->doToggle(); }));
  REG_LAMBDA(CheckLabel, "void onToggle(Callback @cb)",                   ([](hiro::CheckLabel *p, asIScriptFunction *cb) { p->onToggle(GUI::Callback(cb)); }));
}
