
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

    mutable hiro::Color mBackgroundColor;
    auto backgroundColor() const -> hiro::Color* {
      mBackgroundColor = self->backgroundColor();
      return &mBackgroundColor;
    }

    auto setBackgroundColor(const hiro::Color* color) -> void {
      self->setBackgroundColor(*color);
    }

    auto setFont(const hiro::Font* font) -> void {
      self->setFont(*font);
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

    auto setFont(const hiro::Font* font) -> void {
      self->setFont(*font);
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

    mutable hiro::Alignment mAlignment;
    auto alignment() const -> hiro::Alignment* {
      mAlignment = self->alignment();
      return &mAlignment;
    }

    auto setAlignment(const hiro::Alignment* alignment) -> void {
      self->setAlignment(*alignment);
    }

    mutable hiro::Color mBackgroundColor;
    auto backgroundColor() const -> hiro::Color* {
      mBackgroundColor = self->backgroundColor();
      return &mBackgroundColor;
    }

    auto setBackgroundColor(const hiro::Color* color) -> void {
      self->setBackgroundColor(*color);
    }

    mutable hiro::Color mForegroundColor;
    auto foregroundColor() const -> hiro::Color* {
      mForegroundColor = self->foregroundColor();
      return &mForegroundColor;
    }

    auto setForegroundColor(const hiro::Color* color) -> void {
      self->setForegroundColor(*color);
    }

    auto setFont(const hiro::Font* font) -> void {
      self->setFont(*font);
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

  Constructor(Window)
  static auto createWindowAtPosition(float x, float y, bool relative) -> Window* { return new Window(x, y, relative); }
  Constructor(VerticalLayout)
  Constructor(HorizontalLayout)
  Constructor(LineEdit)
  Constructor(Label)
  Constructor(Button)
  Constructor(Canvas)

#undef Constructor

  static auto newCheckLabel() -> hiro::CheckLabel* {
    auto self = new hiro::CheckLabel;
    return self;
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

  static auto checkLabelSetText(hiro::sCheckLabel &p, const string* text) {
    p->setText(*text);
  }
};

auto RegisterGUI(asIScriptEngine *e) -> void {
  int r;

  // GUI
  r = e->SetDefaultNamespace("gui"); assert(r >= 0);

  // Register types first:
  r = e->RegisterObjectType("Window", 0, asOBJ_REF); assert(r >= 0);
  r = e->RegisterObjectType("VerticalLayout", 0, asOBJ_REF); assert(r >= 0);
  r = e->RegisterObjectType("HorizontalLayout", 0, asOBJ_REF); assert(r >= 0);
  r = e->RegisterObjectType("LineEdit", 0, asOBJ_REF); assert(r >= 0);
  r = e->RegisterObjectType("Label", 0, asOBJ_REF); assert(r >= 0);
  r = e->RegisterObjectType("Button", 0, asOBJ_REF); assert(r >= 0);
  r = e->RegisterObjectType("Canvas", 0, asOBJ_REF); assert(r >= 0);
  r = e->RegisterObjectType("CheckLabel", 0, asOBJ_REF); assert(r >= 0);

  // value types:
  r = e->RegisterObjectType("Alignment", sizeof(hiro::Alignment), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<hiro::Alignment>()); assert( r >= 0 );
  r = e->RegisterObjectType("Size", sizeof(hiro::Size), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<hiro::Size>()); assert(r >= 0);
  r = e->RegisterObjectType("Color", sizeof(hiro::Color), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<hiro::Color>()); assert( r >= 0 );
  r = e->RegisterObjectType("Font", sizeof(hiro::Font), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<hiro::Font>()); assert( r >= 0 );

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
  r = e->RegisterObjectMethod("Color", "uint8 get_alpha() property", asMETHOD(hiro::Color, alpha), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "uint8 get_blue() property",  asMETHOD(hiro::Color, blue), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "uint8 get_green() property", asMETHOD(hiro::Color, green), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "uint8 get_red() property",   asMETHOD(hiro::Color, red), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "Color& set_alpha(int alpha)", asMETHOD(hiro::Color, setAlpha), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "Color& set_blue(int blue)", asMETHOD(hiro::Color, setBlue), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "Color& set_green(int green)", asMETHOD(hiro::Color, setGreen), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Color", "Color& set_red(int red)", asMETHOD(hiro::Color, setRed), asCALL_THISCALL); assert(r >= 0);

  // Font value type:
  r = e->RegisterObjectBehaviour("Font", asBEHAVE_CONSTRUCT, "void f(const string &in family, float size = 0.0)", asFUNCTION(GUI::createFont), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Font", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(GUI::destroyFont), asCALL_CDECL_OBJLAST); assert(r >= 0);

  // Window
  r = e->RegisterObjectBehaviour("Window", asBEHAVE_FACTORY, "Window@ f()", asFUNCTION(GUI::createWindow), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Window", asBEHAVE_FACTORY, "Window@ f(float rx, float ry, bool relative)", asFUNCTION(GUI::createWindowAtPosition), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Window", asBEHAVE_ADDREF, "void f()", asMETHOD(GUI::Window, ref_add), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("Window", asBEHAVE_RELEASE, "void f()", asMETHOD(GUI::Window, ref_release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void append(VerticalLayout @sizable)", asMETHOD(GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void append(HorizontalLayout @sizable)", asMETHOD(GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void append(LineEdit @sizable)", asMETHOD(GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void append(Label @sizable)", asMETHOD(GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void append(Button @sizable)", asMETHOD(GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void append(Canvas @sizable)", asMETHOD(GUI::Window, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void set_visible(bool visible) property", asMETHOD(GUI::Window, setVisible), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void set_title(const string &in title) property", asMETHOD(GUI::Window, setTitle), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void set_size(Size &in size) property", asMETHOD(GUI::Window, setSize), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void set_font(Font &in font) property", asMETHOD(GUI::Window, setFont), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "Color& get_backgroundColor() property", asMETHOD(GUI::Window, backgroundColor), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void set_backgroundColor(Color &in color) property", asMETHOD(GUI::Window, setBackgroundColor), asCALL_THISCALL); assert( r >= 0 );

  // VerticalLayout
  r = e->RegisterObjectBehaviour("VerticalLayout", asBEHAVE_FACTORY, "VerticalLayout@ f()", asFUNCTION(GUI::createVerticalLayout), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("VerticalLayout", asBEHAVE_ADDREF, "void f()", asMETHOD(GUI::VerticalLayout, ref_add), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("VerticalLayout", asBEHAVE_RELEASE, "void f()", asMETHOD(GUI::VerticalLayout, ref_release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("VerticalLayout", "void append(VerticalLayout @sizable, Size &in size)", asMETHOD(GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("VerticalLayout", "void append(HorizontalLayout @sizable, Size &in size)", asMETHOD(GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("VerticalLayout", "void append(LineEdit @sizable, Size &in size)", asMETHOD(GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("VerticalLayout", "void append(Label @sizable, Size &in size)", asMETHOD(GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("VerticalLayout", "void append(Button @sizable, Size &in size)", asMETHOD(GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("VerticalLayout", "void append(Canvas @sizable, Size &in size)", asMETHOD(GUI::VerticalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("VerticalLayout", "void resize()", asMETHOD(GUI::VerticalLayout, resize), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("VerticalLayout", "void set_visible(bool visible) property", asMETHOD(GUI::VerticalLayout, setVisible), asCALL_THISCALL); assert( r >= 0 );

  // HorizontalLayout
  r = e->RegisterObjectBehaviour("HorizontalLayout", asBEHAVE_FACTORY, "HorizontalLayout@ f()", asFUNCTION(GUI::createHorizontalLayout), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("HorizontalLayout", asBEHAVE_ADDREF, "void f()", asMETHOD(GUI::HorizontalLayout, ref_add), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("HorizontalLayout", asBEHAVE_RELEASE, "void f()", asMETHOD(GUI::HorizontalLayout, ref_release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("HorizontalLayout", "void append(VerticalLayout @sizable, Size &in size)", asMETHOD(GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("HorizontalLayout", "void append(HorizontalLayout @sizable, Size &in size)", asMETHOD(GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("HorizontalLayout", "void append(LineEdit @sizable, Size &in size)", asMETHOD(GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("HorizontalLayout", "void append(Label @sizable, Size &in size)", asMETHOD(GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("HorizontalLayout", "void append(Button @sizable, Size &in size)", asMETHOD(GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("HorizontalLayout", "void append(Canvas @sizable, Size &in size)", asMETHOD(GUI::HorizontalLayout, appendSizable), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("HorizontalLayout", "void resize()", asMETHOD(GUI::HorizontalLayout, resize), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("HorizontalLayout", "void set_visible(bool visible) property", asMETHOD(GUI::HorizontalLayout, setVisible), asCALL_THISCALL); assert( r >= 0 );

  // LineEdit
  // Register a simple funcdef for the callback
  r = e->RegisterFuncdef("void LineEditCallback(LineEdit @self)"); assert(r >= 0);
  r = e->RegisterObjectBehaviour("LineEdit", asBEHAVE_FACTORY, "LineEdit@ f()", asFUNCTION(GUI::createLineEdit), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("LineEdit", asBEHAVE_ADDREF, "void f()", asMETHOD(GUI::LineEdit, ref_add), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("LineEdit", asBEHAVE_RELEASE, "void f()", asMETHOD(GUI::LineEdit, ref_release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("LineEdit", "void set_visible(bool visible) property", asMETHOD(GUI::LineEdit, setVisible), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("LineEdit", "void set_font(Font &in font) property", asMETHOD(GUI::LineEdit, setFont), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("LineEdit", "string &get_text() property", asMETHOD(GUI::LineEdit, getText), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("LineEdit", "void set_text(const string &in text) property", asMETHOD(GUI::LineEdit, setText), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("LineEdit", "void set_on_change(LineEditCallback @cb) property", asMETHOD(GUI::LineEdit, setOnChange), asCALL_THISCALL); assert( r >= 0 );

  // Label
  r = e->RegisterObjectBehaviour("Label", asBEHAVE_FACTORY, "Label@ f()", asFUNCTION(GUI::createLabel), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Label", asBEHAVE_ADDREF, "void f()", asMETHOD(GUI::Label, ref_add), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("Label", asBEHAVE_RELEASE, "void f()", asMETHOD(GUI::Label, ref_release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Label", "void set_visible(bool visible) property", asMETHOD(GUI::Label, setVisible), asCALL_THISCALL); assert( r >= 0 );

  r = e->RegisterObjectMethod("Label", "Alignment& get_alignment() property", asMETHOD(GUI::Label, alignment), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Label", "void set_alignment(Alignment &in alignment) property", asMETHOD(GUI::Label, setAlignment), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Label", "Color& get_backgroundColor() property", asMETHOD(GUI::Label, backgroundColor), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Label", "void set_backgroundColor(Color &in color) property", asMETHOD(GUI::Label, setBackgroundColor), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Label", "Color& get_foregroundColor() property", asMETHOD(GUI::Label, foregroundColor), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Label", "void set_foregroundColor(Color &in color) property", asMETHOD(GUI::Label, setForegroundColor), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Label", "void set_font(Font &in font) property", asMETHOD(GUI::Label, setFont), asCALL_THISCALL); assert( r >= 0 );

  r = e->RegisterObjectMethod("Label", "string &get_text() property", asMETHOD(GUI::Label, getText), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Label", "void set_text(const string &in text) property", asMETHOD(GUI::Label, setText), asCALL_THISCALL); assert( r >= 0 );

  // Button
  // Register a simple funcdef for the callback
  r = e->RegisterFuncdef("void ButtonCallback(Button @self)"); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Button", asBEHAVE_FACTORY, "Button@ f()", asFUNCTION(GUI::createButton), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Button", asBEHAVE_ADDREF, "void f()", asMETHOD(GUI::Button, ref_add), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("Button", asBEHAVE_RELEASE, "void f()", asMETHOD(GUI::Button, ref_release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Button", "void set_visible(bool visible) property", asMETHOD(GUI::Button, setVisible), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Button", "string &get_text() property", asMETHOD(GUI::Button, getText), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Button", "void set_text(const string &in text) property", asMETHOD(GUI::Button, setText), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Button", "void set_on_activate(ButtonCallback @cb) property", asMETHOD(GUI::Button, setOnActivate), asCALL_THISCALL); assert( r >= 0 );

  // Canvas
  r = e->RegisterObjectBehaviour("Canvas", asBEHAVE_FACTORY, "Canvas@ f()", asFUNCTION(GUI::createCanvas), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Canvas", asBEHAVE_ADDREF, "void f()", asMETHOD(GUI::Canvas, ref_add), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("Canvas", asBEHAVE_RELEASE, "void f()", asMETHOD(GUI::Canvas, ref_release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void set_visible(bool visible) property", asMETHOD(GUI::Canvas, setVisible), asCALL_THISCALL); assert( r >= 0 );
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
  r = e->RegisterObjectBehaviour("CheckLabel", asBEHAVE_FACTORY, "CheckLabel@ f()", asFUNCTION(GUI::newCheckLabel), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("CheckLabel", asBEHAVE_ADDREF, "void f()", asFUNCTION(GUI::hiroAddRef), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("CheckLabel", asBEHAVE_RELEASE, "void f()", asFUNCTION(GUI::hiroRelease), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
  //r = e->RegisterObjectMethod("CheckLabel", "void setText(const string &in text)", asFUNCTION(GUI::checkLabelSetText), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
  r = e->RegisterObjectMethod("CheckLabel", "CheckLabel@ setText(const string &in text)", asFUNCTION( (+[](hiro::CheckLabel *p, const string &text) { return p->setText(text), p; }) ), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
  //auto setText(const string& text = "") -> type&;
  //auto text() const -> string;
  //auto checked() const -> bool;
  //auto doToggle() const -> void;
  //auto onToggle(const function<void ()>& callback = {}) -> type&;
  //auto setChecked(bool checked = true) -> type&;

}
