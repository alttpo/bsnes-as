// GUI test script
gui::Window @w;
gui::LineEdit @text;

void init() {
  @w = gui::Window();
  w.visible = true;
  w.title = "Connect to IP address";
  @text = gui::LineEdit(w);
  text.visible = true;
}
