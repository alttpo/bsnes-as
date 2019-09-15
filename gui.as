// GUI test script
gui::Window @w;
gui::LineEdit @txtServerIP;
gui::LineEdit @txtClientIP;
gui::Button @ok;

void init() {
  @w = gui::Window();
  w.visible = true;
  w.title = "Connect to IP address";
  w.size = gui::Size(256, 24*3);

  auto vl = gui::VerticalLayout();
  w.append(vl);

  auto @hz = gui::HorizontalLayout();
  auto @lbl = gui::Label();
  lbl.text = "Server IP:";
  hz.append(lbl, gui::Size(80, 0));

  @txtServerIP = gui::LineEdit();
  txtServerIP.visible = true;
  @txtServerIP.on_change = function(gui::LineEdit @self) {
    message(self.text);
  };
  hz.append(txtServerIP, gui::Size(128, 20));
  vl.append(hz, gui::Size(0, 0));

  @hz = gui::HorizontalLayout();
  @lbl = gui::Label();
  lbl.text = "Client IP:";
  hz.append(lbl, gui::Size(80, 0));

  @txtClientIP = gui::LineEdit();
  txtClientIP.visible = true;
  @txtClientIP.on_change = function(gui::LineEdit @self) {
    message(self.text);
  };
  hz.append(txtClientIP, gui::Size(128, 20));
  vl.append(hz, gui::Size(0, 0));

  @ok = gui::Button();
  ok.text = "Start";
  @ok.on_activate = function(gui::Button @self) {
    message("OK!");
  };
  vl.append(ok, gui::Size(0, 0));
}
