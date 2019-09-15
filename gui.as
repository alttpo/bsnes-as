// GUI test script
gui::Window @w;
gui::HorizontalLayout @hz;
gui::LineEdit @text;
gui::Button @ok;

void init() {
  @w = gui::Window();
  w.visible = true;
  w.title = "Connect to IP address";
  w.size = gui::Size(256, 24);

  @hz = gui::HorizontalLayout();
  w.append(hz);

  @text = gui::LineEdit();
  text.visible = true;
  @text.on_change = function(gui::LineEdit @self) {
    message(self.text);
  };
  hz.append(text, gui::Size(128, 20));

  @ok = gui::Button();
  ok.text = "OK";
  @ok.on_activate = function(gui::Button @self) {
    message("OK!");
  };
  hz.append(ok, gui::Size(0, 0));
}
