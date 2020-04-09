
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
  r = e->RegisterObjectType("Size", sizeof(hiro::Size), asOBJ_VALUE); assert(r >= 0);

  // Size value type:
  r = e->RegisterObjectBehaviour("Size", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(GUI::createSize), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Size", asBEHAVE_CONSTRUCT, "void f(float width, float height)", asFUNCTION(GUI::createSizeWH), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Size", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(GUI::destroySize), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = e->RegisterObjectMethod("Size", "float get_width()", asMETHOD(hiro::Size, width), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Size", "void set_width(float width)", asMETHOD(hiro::Size, setWidth), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Size", "float get_height()", asMETHOD(hiro::Size, height), asCALL_THISCALL); assert(r >= 0);
  r = e->RegisterObjectMethod("Size", "void set_height(float height)", asMETHOD(hiro::Size, setHeight), asCALL_THISCALL); assert(r >= 0);

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
  r = e->RegisterObjectMethod("Window", "void set_visible(bool visible)", asMETHOD(GUI::Window, setVisible), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void set_title(const string &in title)", asMETHOD(GUI::Window, setTitle), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Window", "void set_size(Size &in size)", asMETHOD(GUI::Window, setSize), asCALL_THISCALL); assert( r >= 0 );

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
  r = e->RegisterObjectMethod("VerticalLayout", "void set_visible(bool visible)", asMETHOD(GUI::VerticalLayout, setVisible), asCALL_THISCALL); assert( r >= 0 );

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
  r = e->RegisterObjectMethod("HorizontalLayout", "void set_visible(bool visible)", asMETHOD(GUI::HorizontalLayout, setVisible), asCALL_THISCALL); assert( r >= 0 );

  // LineEdit
  // Register a simple funcdef for the callback
  r = e->RegisterFuncdef("void LineEditCallback(LineEdit @self)"); assert(r >= 0);
  r = e->RegisterObjectBehaviour("LineEdit", asBEHAVE_FACTORY, "LineEdit@ f()", asFUNCTION(GUI::createLineEdit), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("LineEdit", asBEHAVE_ADDREF, "void f()", asMETHOD(GUI::LineEdit, ref_add), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("LineEdit", asBEHAVE_RELEASE, "void f()", asMETHOD(GUI::LineEdit, ref_release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("LineEdit", "void set_visible(bool visible)", asMETHOD(GUI::LineEdit, setVisible), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("LineEdit", "string &get_text()", asMETHOD(GUI::LineEdit, getText), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("LineEdit", "void set_text(const string &in text)", asMETHOD(GUI::LineEdit, setText), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("LineEdit", "void set_on_change(LineEditCallback @cb)", asMETHOD(GUI::LineEdit, setOnChange), asCALL_THISCALL); assert( r >= 0 );

  // Label
  r = e->RegisterObjectBehaviour("Label", asBEHAVE_FACTORY, "Label@ f()", asFUNCTION(GUI::createLabel), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Label", asBEHAVE_ADDREF, "void f()", asMETHOD(GUI::Label, ref_add), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("Label", asBEHAVE_RELEASE, "void f()", asMETHOD(GUI::Label, ref_release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Label", "void set_visible(bool visible)", asMETHOD(GUI::Label, setVisible), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Label", "string &get_text()", asMETHOD(GUI::Label, getText), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Label", "void set_text(const string &in text)", asMETHOD(GUI::Label, setText), asCALL_THISCALL); assert( r >= 0 );

  // Button
  // Register a simple funcdef for the callback
  r = e->RegisterFuncdef("void ButtonCallback(Button @self)"); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Button", asBEHAVE_FACTORY, "Button@ f()", asFUNCTION(GUI::createButton), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Button", asBEHAVE_ADDREF, "void f()", asMETHOD(GUI::Button, ref_add), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("Button", asBEHAVE_RELEASE, "void f()", asMETHOD(GUI::Button, ref_release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Button", "void set_visible(bool visible)", asMETHOD(GUI::Button, setVisible), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Button", "string &get_text()", asMETHOD(GUI::Button, getText), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Button", "void set_text(const string &in text)", asMETHOD(GUI::Button, setText), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Button", "void set_on_activate(ButtonCallback @cb)", asMETHOD(GUI::Button, setOnActivate), asCALL_THISCALL); assert( r >= 0 );

  // Canvas
  r = e->RegisterObjectBehaviour("Canvas", asBEHAVE_FACTORY, "Canvas@ f()", asFUNCTION(GUI::createCanvas), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Canvas", asBEHAVE_ADDREF, "void f()", asMETHOD(GUI::Canvas, ref_add), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("Canvas", asBEHAVE_RELEASE, "void f()", asMETHOD(GUI::Canvas, ref_release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void set_visible(bool visible)", asMETHOD(GUI::Canvas, setVisible), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void set_size(Size &in size)", asMETHOD(GUI::Canvas, setSize), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void setPosition(float x, float y)", asMETHOD(GUI::Canvas, setPosition), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void setAlignment(float horizontal, float vertical)", asMETHOD(GUI::Canvas, setAlignment), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void setCollapsible(bool collapsible)", asMETHOD(GUI::Canvas, setCollapsible), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void update()", asMETHOD(GUI::Canvas, update), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void fill(uint16 color)", asMETHOD(GUI::Canvas, fill), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void pixel(int x, int y, uint16 color)", asMETHOD(GUI::Canvas, pixel), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Canvas", "void draw_sprite_4bpp(int x, int y, uint c, uint width, uint height, const array<uint16> &in tiledata, const array<uint16> &in palette)", asMETHOD(GUI::Canvas, draw_sprite_4bpp), asCALL_THISCALL); assert( r >= 0 );
}
