// GUI test script
gui::Window @w;
gui::LineEdit @txtServerIP;
gui::LineEdit @txtClientIP;
gui::Button @ok;

void init() {
  @w = gui::Window();
  w.visible = true;
  w.title = "Connect to IP address";
  w.size = gui::Size(256, 24);

  gui::VerticalLayout @vl = gui::VerticalLayout();
  w.append(vl);

  //auto @hz = gui::HorizontalLayout();
  //vl.append(hz);

  @txtServerIP = gui::LineEdit();
  txtServerIP.visible = true;
  @txtServerIP.on_change = function(gui::LineEdit @self) {
    message(self.text);
  };
  vl.append(txtServerIP, gui::Size(128, 20));

  @txtClientIP = gui::LineEdit();
  txtClientIP.visible = true;
  @txtClientIP.on_change = function(gui::LineEdit @self) {
    message(self.text);
  };
  vl.append(txtClientIP, gui::Size(128, 20));

  @ok = gui::Button();
  ok.text = "OK";
  @ok.on_activate = function(gui::Button @self) {
    message("OK!");
  };
  vl.append(ok, gui::Size(0, 0));
}
