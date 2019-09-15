// GUI test script
gui::Window @w;
gui::LineEdit @text;

void init() {
  @w = gui::Window();
  w.visible = true;
  w.title = "Connect to IP address";
  w.size = gui::Size(256, 20);

  @text = gui::LineEdit();
  text.visible = true;
  @text.on_change = function(gui::LineEdit @self) {
    message(self.text);
  };
  w.append(text);
}
