// GUI test script
SettingsWindow @settings;

class SettingsWindow {
  private GUI::Window @window;
  private GUI::LineEdit @txtServerIP;
  private GUI::LineEdit @txtClientIP;
  private GUI::Button @ok;

  string clientIP;
  string serverIP;
  bool started;

  SettingsWindow() {
    @window = GUI::Window();
    window.visible = true;
    window.title = "Connect to IP address";
    window.size = GUI::Size(256, 24*3);

    auto vl = GUI::VerticalLayout();
    {
      auto @hz = GUI::HorizontalLayout();
      {
        auto @lbl = GUI::Label();
        lbl.text = "Server IP:";
        hz.append(lbl, GUI::Size(80, 0));

        @txtServerIP = GUI::LineEdit();
        //txtServerIP.visible = true;
        hz.append(txtServerIP, GUI::Size(128, 20));
      }
      vl.append(hz, GUI::Size(0, 0));

      @hz = GUI::HorizontalLayout();
      {
        auto @lbl = GUI::Label();
        lbl.text = "Client IP:";
        hz.append(lbl, GUI::Size(80, 0));

        @txtClientIP = GUI::LineEdit();
        //txtClientIP.visible = true;
        hz.append(txtClientIP, GUI::Size(128, 20));
      }
      vl.append(hz, GUI::Size(0, 0));

      @ok = GUI::Button();
      ok.text = "Start";
      ok.onActivate(@GUI::Callback(this.startClicked));
      vl.append(ok, GUI::Size(0, 0));
    }
    window.append(vl);
  }

  private void startClicked() {
    message("Start!");
    clientIP = txtClientIP.text;
    serverIP = txtServerIP.text;
    started = true;
    hide();
  }

  void show() {
    window.visible = true;
  }

  void hide() {
    window.visible = false;
  }
};

void init() {
  @settings = SettingsWindow();
}

void post_frame() {
  if (!settings.started) return;

}
