// GUI test script
SettingsWindow @settings;

class SettingsWindow {
  private gui::Window @window;
  private gui::LineEdit @txtServerIP;
  private gui::LineEdit @txtClientIP;
  private gui::Button @ok;

  string clientIP;
  string serverIP;
  bool started;

  SettingsWindow() {
    @window = gui::Window();
    window.visible = true;
    window.title = "Connect to IP address";
    window.size = gui::Size(256, 24*3);

    auto vl = gui::VerticalLayout();
    {
      auto @hz = gui::HorizontalLayout();
      {
        auto @lbl = gui::Label();
        lbl.text = "Server IP:";
        hz.append(lbl, gui::Size(80, 0));

        @txtServerIP = gui::LineEdit();
        //txtServerIP.visible = true;
        hz.append(txtServerIP, gui::Size(128, 20));
      }
      vl.append(hz, gui::Size(0, 0));

      @hz = gui::HorizontalLayout();
      {
        auto @lbl = gui::Label();
        lbl.text = "Client IP:";
        hz.append(lbl, gui::Size(80, 0));

        @txtClientIP = gui::LineEdit();
        //txtClientIP.visible = true;
        hz.append(txtClientIP, gui::Size(128, 20));
      }
      vl.append(hz, gui::Size(0, 0));

      @ok = gui::Button();
      ok.text = "Start";
      @ok.on_activate = @gui::ButtonCallback(this.startClicked);
      vl.append(ok, gui::Size(0, 0));
    }
    window.append(vl);
  }

  void startClicked(gui::Button @self) {
    message("Start!");
    clientIP = txtClientIP.text;
    serverIP = txtServerIP.text;
    started = true;
    window.visible = false;
  }
};

void init() {
  @settings = SettingsWindow();
}

void post_frame() {
  if (!settings.started) return;

}
