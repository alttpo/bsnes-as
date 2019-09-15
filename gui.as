// GUI test script
gui::Window @w;
gui::LineEdit @text;

void init() {
  @w = gui::Window();
  w.visible = true;
  w.title = "Connect to IP address";
  w.size = gui::Size(128, 32);

  @text = gui::LineEdit();
  text.visible = true;
  w.append(text);
}
