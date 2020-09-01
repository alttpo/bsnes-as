
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

  struct Monitor {
    static auto dpi(maybe<uint> monitor = nothing) -> Position { return Position{}; }
  };

#define Declare(Name) \
  struct Name; \
  struct m##Name; \
  using s##Name = shared_pointer<m##Name>; \
  using w##Name = shared_pointer_weak<m##Name>;

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
    using type = mSizable;

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

  Declare(Window)
  struct mWindow : mObject {
    using type = mWindow;

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
    using type = mVerticalLayoutCell;

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
    using type = mVerticalLayout;
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
    using type = mHorizontalLayoutCell;

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
    using type = mHorizontalLayout;
    using mSizable::remove;

    auto alignment() const -> maybe<float> { return {}; }
    auto append(sSizable sizable, Size size, float spacing = 5.f) -> type&;
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

}
