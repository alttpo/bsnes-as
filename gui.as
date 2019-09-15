// GUI test script
gui::Window @w;
//gui::LineEdit @text;
gui::Button @ok;

void init() {
  @w = gui::Window();
  w.visible = true;
  w.title = "Connect to IP address";
  w.size = gui::Size(256, 20);

  @ok = gui::Button();
  ok.text = "OK";
  w.append(ok);

//  @text = gui::LineEdit();
//  text.visible = true;
//  @text.on_change = function(gui::LineEdit @self) {
//    message(self.text);
//  };
//  w.append(text);
}
