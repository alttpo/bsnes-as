
// Fake implementation of hiro namespace to keep script bindings compiling, i.e. for libretro target.

namespace hiro {
  struct Color {
    Color() {}
    Color(const Color &o) { (void)o; }
    Color(int red, int green, int blue, int alpha = 255) { (void)red; (void)green; (void)blue; (void)alpha; }
  };

  struct Alignment {
    using type = Alignment;

    Alignment() {}
    Alignment(const Alignment &o) { (void)o; }
    Alignment(float horizontal, float vertical = 0.5) { (void)horizontal; (void)vertical; }

    auto horizontal() const -> float { return 0.f; }
    auto reset() -> type& { return *this; }
    auto setAlignment(float horizontal = -1.0, float vertical = 0.5) -> type& { return (void)horizontal, (void)vertical, *this; }
    auto setHorizontal(float horizontal) -> type& { return (void)horizontal, *this; }
    auto setVertical(float vertical) -> type& { return (void)vertical, *this; }
    auto vertical() const -> float { return 0.f; }
  };

  struct Size {
    using type = Size;

    Size() {}
    Size(const Size &o) { (void)o; }
    Size(float width, float height) { (void)width; (void)height; }
    template<typename W, typename H>
    Size(W width, H height) : Size((float)width, (float)height) {}

    auto height() const -> float { return 0.f; }
    auto reset() -> type& { return *this; }
    auto setHeight(float height) -> type& { return (void)height, *this; }
    auto setSize(Size source = {}) -> type& { return (void)source, *this; }
    auto setSize(float width, float height) -> type& { return (void)width, (void)height, *this; }
    auto setWidth(float width) -> type& { return (void)width, *this; }
    auto width() const -> float { return 0.f; }
  };

  struct Position {
    using type = Position;

    Position() {}
    Position(const Position &o) { (void)o; }
    Position(float x, float y) { (void)x; (void)y; }
    template<typename X, typename Y>
    Position(X x, Y y) : Position((float)x, (float)y) {}

    auto reset() -> type& { return *this; }
    auto setPosition(Position position = {}) -> type& { return (void)position, *this; }
    auto setPosition(float x, float y) -> type& { return (void)x, (void)y, *this; }
    auto setX(float x) -> type& { return (void)x, *this; }
    auto setY(float y) -> type& { return (void)y, *this; }
    auto x() const -> float { return 0.f; }
    auto y() const -> float { return 0.f; }
  };

  struct Geometry {
    using type = Geometry;

    Geometry() {}
    Geometry(Position position, Size size) { (void)position; (void)size; }
    Geometry(float x, float y, float width, float height) { (void)x; (void)y; (void)width; (void)height; }
    template<typename X, typename Y, typename W, typename H>
    Geometry(X x, Y y, W width, H height) : Geometry((float) x, (float) y, (float) width, (float) height) {}

    auto setPosition(Position position = {}) -> type& { return (void)position, *this; }
    auto setSize(Size size = {}) -> type& { return (void)size, *this; }

    auto position() const -> Position { return Position(); }
    auto size() const -> Size { return Size(); }
  };

  struct Font {
    using type = Font;

    Font(const string& family = "", float size = 0.0) { (void)family; (void)size; }
    Font(const Font& o) { (void)o; }

    auto bold() const -> bool { return false; }
    auto family() const -> string { return {}; }
    auto italic() const -> bool { return false; }
    auto reset() -> type& { return *this; }
    auto setBold(bool bold = true) -> type& { return (void)bold, *this; }
    auto setFamily(const string& family = "") -> type& { return (void)family, *this; }
    auto setItalic(bool italic = true) -> type& { return (void)italic, *this; }
    auto setSize(float size = 0.0) -> type& { return (void)size, *this; }
    auto size() const -> float { return 0.f; }
    auto size(const string& text) const -> Size { return Size(); }

    static const string Sans;
    static const string Serif;
    static const string Mono;
  };

  const string Font::Sans  = "{sans}";
  const string Font::Serif = "{serif}";
  const string Font::Mono  = "{mono}";

  struct Gradient {
    using type = Gradient;

    Gradient() {}

    explicit operator bool() const { return false; }
    auto operator==(const Gradient& source) const -> bool { return false; }
    auto operator!=(const Gradient& source) const -> bool { return false; }

    auto setBilinear(Color topLeft, Color topRight, Color bottomLeft, Color bottomRight) -> type& { return *this; }
    auto setHorizontal(Color left, Color right) -> type& { return *this; }
    auto setVertical(Color top, Color bottom) -> type& { return *this; }
  };

  struct Monitor {
    static auto dpi(maybe<uint> monitor = nothing) -> Position { return Position{}; }
  };

  enum class Orientation : uint { Horizontal, Vertical };

#define Declare(Name) \
  struct Name; \
  struct m##Name; \
  using s##Name = shared_pointer<m##Name>; \
  using w##Name = shared_pointer_weak<m##Name>;

#define Define(Name) \
  using type = m##Name; \
  m##Name() {} \
  template<typename T, typename... P> m##Name(T* parent, P&&... p) : m##Name() {}

#define DefineShared(Name) \
  using type = Name; \
  using internalType = m##Name; \
  Name() {} \
  template<typename T, typename... P> Name(T* parent, P&&... p) : Name() {} \
  Name(const s##Name& source) { (void)source; } \
  template<typename T> Name(const T& source, \
  std::enable_if_t<std::is_base_of<internalType, typename T::internalType>::value>* = 0) { (void)source; } \

  Declare(Object)
  struct mObject {
    using type = mObject;

    mObject() {}
    template<typename T, typename... P> mObject(T* parent, P&&... p) : mObject() {
    }

    template<typename T = string> auto attribute(const string& name) const -> T {
      return {};
    }

    template<typename T = string, typename U = string> auto setAttribute(const string& name, const U& value) -> type& {
      (void)name;
      (void)value;
      return *this;
    }

    auto enabled(bool recursive = false) const -> bool { return (void)recursive, false; }
    auto focused() const -> bool { return false; }
    auto font(bool recursive = false) const -> Font { (void)recursive; return {}; }
    auto offset() const -> int { return 0; }
    auto visible(bool recursive = false) const -> bool { return (void)recursive, false; }

    auto remove() -> type& { return *this; }
    auto reset() -> type& { return *this; }

    auto setEnabled(bool enabled = true) -> type& { return (void)enabled, *this; }
    auto setFocused() -> type& { return *this; }
    auto setFont(const Font& font = {}) -> type& { return (void)font, *this; };
    auto setVisible(bool visible = true) -> type& { return (void)visible, *this; }

    auto setParent(mObject* parent = nullptr, int offset = -1) -> type& { return (void)parent, (void)offset, *this; };
  };
  struct Object : sObject, mObject {};

  Declare(Sizable)
  struct mSizable : mObject {
    Define(Sizable);

    auto collapsible() const -> bool { return false; }
    auto layoutExcluded() const -> bool { return false; }
    auto doSize() const -> void {}
    auto geometry() const -> Geometry { return {}; }
    auto minimumSize() const -> Size { return {}; }
    auto onSize(const function<void ()>& callback = {}) -> type& { return *this; };
    auto setCollapsible(bool collapsible = true) -> type& { return (void)collapsible, *this; }
    auto setLayoutExcluded(bool layoutExcluded = true) -> type& { return (void)layoutExcluded, *this; }
    auto setGeometry(Geometry geometry) -> type& { return (void)geometry, *this; }
  };
  struct Sizable : sSizable, mSizable {};

  struct MouseCursor {
    using type = MouseCursor;

    MouseCursor(const string& name = "");

    explicit operator bool() const { return false; }
    auto operator==(const MouseCursor& source) const -> bool { return false; }
    auto operator!=(const MouseCursor& source) const -> bool { return false; }

    auto name() const -> string { return {}; }
    auto setName(const string& name = "") -> type& { return *this; }

    static const string Default;
    static const string Hand;
    static const string HorizontalResize;
    static const string VerticalResize;
  };

  const string MouseCursor::Default = "";
  const string MouseCursor::Hand = "{hand}";
  const string MouseCursor::HorizontalResize = "{horizontal-resize}";
  const string MouseCursor::VerticalResize = "{vertical-resize}";

  Declare(Widget)
  struct mWidget : mSizable {
    Define(Widget);

    auto doDrop(vector<string> names) const -> void {}
    //auto doMouseEnter() const -> void {}
    //auto doMouseLeave() const -> void {}
    //auto doMouseMove(Position position) const -> void {}
    //auto doMousePress(Mouse::Button button) const -> void {}
    //auto doMouseRelease(Mouse::Button button) const -> void {}
    auto droppable() const -> bool { return false; }
    auto focusable() const -> bool { return false; }
    auto mouseCursor() const -> MouseCursor { return {}; }
    auto onDrop(const function<void (vector<string>)>& callback = {}) -> type& { return *this; }
    //auto onMouseEnter(const function<void ()>& callback = {}) -> type& { return *this; }
    //auto onMouseLeave(const function<void ()>& callback = {}) -> type& { return *this; }
    //auto onMouseMove(const function<void (Position position)>& callback = {}) -> type& { return *this; }
    //auto onMousePress(const function<void (Mouse::Button)>& callback = {}) -> type& { return *this; }
    //auto onMouseRelease(const function<void (Mouse::Button)>& callback = {}) -> type& { return *this; }
    //auto remove() -> type& override;
    auto setDroppable(bool droppable = true) -> type& { return *this; }
    auto setFocusable(bool focusable = true) -> type& { return *this; }
    auto setMouseCursor(const MouseCursor& mouseCursor = {}) -> type& { return *this; }
    auto setToolTip(const string& toolTip = "") -> type& { return *this; }
    auto toolTip() const -> string { return {}; }
  };
  struct Widget : sWidget, mWidget {};

  Declare(Window)
  struct mWindow : mObject {
    Define(Window);

    auto append(sSizable sizable) -> type& { (void)sizable; return *this; }
    auto backgroundColor() const -> Color { return {}; }
    auto dismissable() const -> bool { return false; }
    auto frameGeometry() const -> Geometry { return {}; }
    auto fullScreen() const -> bool { return false; }
    auto geometry() const -> Geometry { return {}; }
    auto maximized() const -> bool { return false; }
    auto maximumSize() const -> Size { return {}; }
    auto minimized() const -> bool { return false; }
    auto minimumSize() const -> Size { return {}; }
    auto modal() const -> bool { return false; }
    auto resizable() const -> bool { return false; }
    auto sizable() const -> Sizable { return {}; }
    auto title() const -> string { return {}; }
    auto onSize(const function<void ()>& callback = {}) -> type& { return (void)callback, *this; };
    auto setBackgroundColor(Color color = {}) -> type& { return (void)color, *this; }
    auto setDismissable(bool dismissable = true) -> type& { return (void)dismissable, *this; }
    auto setDroppable(bool droppable = true) -> type& { return (void)droppable, *this; }
    auto setFrameGeometry(Geometry geometry) -> type& { return (void)geometry, *this; }
    auto setFramePosition(Position position) -> type& { return (void)position, *this; }
    auto setFrameSize(Size size) -> type& { return (void)size, *this; }
    auto setFullScreen(bool fullScreen = true) -> type& { return (void)fullScreen, *this; }
    auto setGeometry(Geometry geometry) -> type& { return (void)geometry, *this; }
    auto setGeometry(Alignment alignment, Size size) -> type& { return (void)alignment, (void)size, *this; }
    auto setMaximized(bool maximized = true) -> type& { return (void)maximized, *this; }
    auto setMaximumSize(Size size = {}) -> type& { return (void)size, *this; }
    auto setMinimized(bool minimized = true) -> type& { return (void)minimized, *this; }
    auto setMinimumSize(Size size = {}) -> type& { return (void)size, *this; }
    auto setModal(bool modal = true) -> type& { return (void)modal, *this; }
    auto setPosition(Position position) -> type& { return (void)position, *this; }
    auto setPosition(sWindow relativeTo, Position position) -> type& { return (void)relativeTo, (void)position, *this; }
    auto setResizable(bool resizable = true) -> type& { return (void)resizable, *this; }
    auto setSize(Size size) -> type& { return (void)size, *this; }
    auto setTitle(const string& title = "") -> type& { return (void)title, *this; }
  };
  struct Window : sWindow, mWindow {};

  Declare(VerticalLayoutCell)
  struct mVerticalLayoutCell : mObject {
    Define(VerticalLayoutCell);

    auto alignment() const -> maybe<float> { return {}; }
    auto collapsible() const -> bool { return false; }
    auto layoutExcluded() const -> bool { return false; }
    auto setAlignment(maybe<float> alignment) -> type& { return *this; }
    //auto setEnabled(bool enabled) -> type& override;
    //auto setFont(const Font& font) -> type& override;
    //auto setParent(mObject* parent = nullptr, int offset = -1) -> type& override;
    auto setSizable(sSizable sizable) -> type& { return *this; }
    auto setSize(Size size) -> type& { return *this; }
    auto setSpacing(float spacing) -> type& { return *this; }
    //auto setVisible(bool visible) -> type& override;
    auto sizable() const -> Sizable { return {}; }
    auto size() const -> Size { return {}; }
    auto spacing() const -> float { return 0.f; }
    auto synchronize() -> type& { return *this; }
  };
  struct VerticalLayoutCell : sVerticalLayoutCell, mVerticalLayoutCell {};

  Declare(VerticalLayout)
  struct mVerticalLayout : mSizable {
    Define(VerticalLayout);
    using mSizable::remove;

    auto alignment() const -> maybe<float> { return {}; }
    auto append(sSizable sizable, Size size, float spacing = 5.f) -> type& { return *this; }
    auto cell(uint position) const -> VerticalLayoutCell { return {}; }
    auto cell(sSizable sizable) const -> VerticalLayoutCell { return {}; }
    auto cells() const -> vector<VerticalLayoutCell> { return {}; }
    auto cellCount() const -> uint { return 0; }
    //auto minimumSize() const -> Size override;
    auto padding() const -> Geometry { return {}; }
    auto remove(sSizable sizable) -> type& { return *this; }
    auto remove(sVerticalLayoutCell cell) -> type& { return *this; }
    //auto reset() -> type& override;
    auto resize() -> type& { return *this; }
    auto setAlignment(maybe<float> alignment = {}) -> type& { return *this; }
    //auto setEnabled(bool enabled) -> type& override;
    //auto setFont(const Font& font) -> type& override;
    //auto setGeometry(Geometry geometry) -> type& override;
    auto setPadding(Geometry padding) -> type& { return *this; }
    auto setPadding(float padding) { return setPadding({padding, padding, padding, padding}), *this; }
    auto setPadding(float x, float y) { return setPadding({x, y, x, y}), *this; }
    //auto setParent(mObject* parent = nullptr, int offset = -1) -> type& override;
    auto setSpacing(float spacing) -> type& { return *this; }
    //auto setVisible(bool visible) -> type& override;
    auto spacing() const -> float { return 0.f; }
    auto synchronize() -> type& { return *this; }
  };
  struct VerticalLayout : sVerticalLayout, mVerticalLayout {};

  Declare(HorizontalLayoutCell)
  struct mHorizontalLayoutCell : mObject {
    Define(HorizontalLayoutCell);

    auto alignment() const -> maybe<float> { return {}; }
    auto collapsible() const -> bool { return false; }
    auto layoutExcluded() const -> bool { return false; }
    auto setAlignment(maybe<float> alignment) -> type& { return *this; }
    //auto setEnabled(bool enabled) -> type& override;
    //auto setFont(const Font& font) -> type& override;
    //auto setParent(mObject* parent = nullptr, int offset = -1) -> type& override;
    auto setSizable(sSizable sizable) -> type& { return *this; }
    auto setSize(Size size) -> type& { return *this; }
    auto setSpacing(float spacing) -> type& { return *this; }
    //auto setVisible(bool visible) -> type& override;
    auto sizable() const -> Sizable { return {}; }
    auto size() const -> Size { return {}; }
    auto spacing() const -> float { return 0.f; }
    auto synchronize() -> type& { return *this; }
  };
  struct HorizontalLayoutCell : sHorizontalLayoutCell, mHorizontalLayoutCell {};

  Declare(HorizontalLayout)
  struct mHorizontalLayout : mSizable {
    Define(HorizontalLayout);
    using mSizable::remove;

    auto alignment() const -> maybe<float> { return {}; }
    auto append(sSizable sizable, Size size, float spacing = 5.f) -> type& { return *this; }
    auto cell(uint position) const -> HorizontalLayoutCell { return {}; }
    auto cell(sSizable sizable) const -> HorizontalLayoutCell { return {}; }
    auto cells() const -> vector<HorizontalLayoutCell> { return {}; }
    auto cellCount() const -> uint { return 0; }
    //auto minimumSize() const -> Size override;
    auto padding() const -> Geometry { return {}; }
    auto remove(sSizable sizable) -> type& { return *this; }
    auto remove(sHorizontalLayoutCell cell) -> type& { return *this; }
    //auto reset() -> type& override;
    auto resize() -> type& { return *this; }
    auto setAlignment(maybe<float> alignment = {}) -> type& { return *this; }
    //auto setEnabled(bool enabled) -> type& override;
    //auto setFont(const Font& font) -> type& override;
    //auto setGeometry(Geometry geometry) -> type& override;
    auto setPadding(Geometry padding) -> type& { return *this; }
    auto setPadding(float padding) { return setPadding({padding, padding, padding, padding}), *this; }
    auto setPadding(float x, float y) { return setPadding({x, y, x, y}), *this; }
    auto setParent(mObject* parent = nullptr, int offset = -11) -> type& { return *this; }
    auto setSpacing(float spacing) -> type& { return *this; }
    //auto setVisible(bool visible) -> type& override;
    auto spacing() const -> float { return 0.f; }
    auto synchronize() -> type& { return *this; }
  };
  struct HorizontalLayout : sHorizontalLayout, mHorizontalLayout {};

  Declare(Group)
  struct mGroup : mObject {
    Define(Group);

    auto append(sObject object) -> type& { return *this; }
    auto object(uint offset) const -> Object { return {}; }
    auto objectCount() const -> uint { return 0; }
    auto objects() const -> vector<Object> { return {}; }
    auto remove(sObject object) -> type& { return *this; }
  };
  struct Group : sGroup, mGroup {};

  Declare(ComboButtonItem)
  struct mComboButtonItem : mObject {
    Define(ComboButtonItem);

    auto icon() const -> image { return {}; }
    //auto remove() -> type& override;
    auto selected() const -> bool { return false; }
    auto setIcon(const image& icon = {}) -> type& { return *this; }
    auto setSelected() -> type& { return *this; }
    auto setText(const string& text = "") -> type& { return *this; }
    auto text() const -> string { return {}; }
  };
  struct ComboButtonItem : sComboButtonItem, mComboButtonItem {
    DefineShared(ComboButtonItem);
  };

  Declare(ComboButton)
  struct mComboButton : mWidget {
    Define(ComboButton);
    using mObject::remove;

    auto append(sComboButtonItem item) -> type& { return *this; }
    auto doChange() const -> void { }
    auto item(uint position) const -> ComboButtonItem { return {}; }
    auto itemCount() const -> uint { return 0; }
    auto items() const -> vector<ComboButtonItem> { return {}; }
    auto onChange(const function<void ()>& callback = {}) -> type& { return *this; }
    auto remove(sComboButtonItem item) -> type& { return *this; }
    auto reset() -> type& { return *this; }
    auto selected() const -> ComboButtonItem { return {}; }
    //auto setParent(mObject* parent = nullptr, int offset = -1) -> type& override;

    //auto destruct() -> void override;
  };
  struct ComboButton : sComboButton, mComboButton {
    DefineShared(ComboButton);

    auto reset() { return *this; }
  };

  Declare(LineEdit)
  struct mLineEdit : mWidget {
    Define(LineEdit);

    auto backgroundColor() const -> Color { return {}; }
    auto doActivate() const -> void { }
    auto doChange() const -> void { }
    auto editable() const -> bool { return false; }
    auto foregroundColor() const -> Color { return {}; }
    auto onActivate(const function<void ()>& callback = {}) -> type& { return *this; }
    auto onChange(const function<void ()>& callback = {}) -> type& { return *this; }
    auto setBackgroundColor(Color color = {}) -> type& { return *this; }
    auto setEditable(bool editable = true) -> type& { return *this; }
    auto setForegroundColor(Color color = {}) -> type& { return *this; }
    auto setText(const string& text = "") -> type& { return *this; }
    auto text() const -> string { return {}; }
  };
  struct LineEdit : sLineEdit, mLineEdit {
    DefineShared(LineEdit);
  };

  Declare(Label)
  struct mLabel : mWidget {
    Define(Label);

    auto alignment() const -> Alignment { return {}; }
    auto backgroundColor() const -> Color { return {}; }
    auto foregroundColor() const -> Color { return {}; }
    auto setAlignment(Alignment alignment = {}) -> type& { return *this; }
    auto setBackgroundColor(Color color = {}) -> type& { return *this; }
    auto setForegroundColor(Color color = {}) -> type& { return *this; }
    auto setText(const string& text = "") -> type& { return *this; }
    auto text() const -> string { return {}; }
  };
  struct Label : sLabel, mLabel {
    DefineShared(Label);
  };

  Declare(Button)
  struct mButton : mWidget {
    Define(Button);

    auto bordered() const -> bool { return false; }
    auto doActivate() const -> void { }
    auto icon() const -> image { return {}; }
    auto onActivate(const function<void ()>& callback = {}) -> type& { return *this; }
    auto orientation() const -> Orientation { return {}; }
    auto setBordered(bool bordered = true) -> type& { return *this; }
    auto setIcon(const image& icon = {}) -> type& { return *this; }
    auto setOrientation(Orientation orientation = Orientation::Horizontal) -> type& { return *this; }
    auto setText(const string& text = "") -> type& { return *this; }
    auto text() const -> string { return {}; }
  };
  struct Button : sButton, mButton {
    DefineShared(Button);
  };

  Declare(Canvas)
  struct mCanvas : mWidget {
    Define(Canvas);

    auto alignment() const -> Alignment { return {}; }
    auto color() const -> Color { return {}; }
    auto data() -> uint32_t* { return nullptr; }
    auto gradient() const -> Gradient { return {}; }
    auto icon() const -> image { return {}; }
    auto setAlignment(Alignment alignment = {}) -> type& { return *this; }
    auto setColor(Color color = {}) -> type& { return *this; }
    auto setGradient(Gradient gradient = {}) -> type& { return *this; }
    auto setIcon(const image& icon = {}) -> type& { return *this; }
    auto setSize(Size size = {}) -> type& { return *this; }
    auto size() const -> Size { return {}; }
    auto update() -> type& { return *this; }

    // [jsd]
    auto iconRef() -> image& { return state.icon; }
    auto moveIcon(image&& icon) -> type& { return *this; }

  private:
    struct State {
      image icon;
    } state;
  };
  struct Canvas : sCanvas, mCanvas {
    DefineShared(Canvas);
  };

  Declare(CheckLabel)
  struct mCheckLabel : mWidget {
    Define(CheckLabel);

    auto checked() const -> bool { return false; }
    auto doToggle() const -> void {}
    auto onToggle(const function<void ()>& callback = {}) -> type& { return *this; }
    auto setChecked(bool checked = true) -> type& { return *this; }
    auto setText(const string& text = "") -> type& { return *this; }
    auto text() const -> string { return {}; }
  };
  struct CheckLabel : sCheckLabel, mCheckLabel {
    DefineShared(CheckLabel);
  };

  Declare(HorizontalSlider)
  struct mHorizontalSlider : mWidget {
    Define(HorizontalSlider);

    auto doChange() const -> void {}
    auto length() const -> uint { return 0; }
    auto onChange(const function<void ()>& callback = {}) -> type& { return *this; }
    auto position() const -> uint { return 0; }
    auto setLength(uint length = 101) -> type& { return *this; }
    auto setPosition(uint position = 0) -> type& { return *this; }

  };
  struct HorizontalSlider : sHorizontalSlider, mHorizontalSlider {
    DefineShared(HorizontalSlider);
  };
}
