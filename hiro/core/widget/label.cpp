#if defined(Hiro_Label)

auto mLabel::allocate() -> pObject* {
  return new pLabel(*this);
}

//

auto mLabel::alignment() const -> Alignment {
  return state.alignment;
}

auto mLabel::backgroundColor() const -> Color {
  return state.backgroundColor;
}

auto mLabel::foregroundColor() const -> Color {
  return state.foregroundColor;
}

auto mLabel::setAlignment(Alignment alignment) -> type& {
  state.alignment = alignment;
  signal(setAlignment, alignment);
  return *this;
}

auto mLabel::setBackgroundColor(Color color) -> type& {
  if (color == state.backgroundColor) return *this;
  state.backgroundColor = color;
  signal(setBackgroundColor, color);
  return *this;
}

auto mLabel::setForegroundColor(Color color) -> type& {
  if (color == state.foregroundColor) return *this;
  state.foregroundColor = color;
  signal(setForegroundColor, color);
  return *this;
}

auto mLabel::setText(const string& text) -> type& {
  if (text == state.text) return *this;
  state.text = text;
  signal(setText, text);
  return *this;
}

auto mLabel::text() const -> string {
  return state.text;
}

#endif
