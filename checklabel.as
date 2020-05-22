SettingsWindow @settings;

class SettingsWindow {
  private gui::Window @window;
  private gui::CheckLabel @chk;

  SettingsWindow() {
    @window = gui::Window();
    window.visible = true;
    window.title = "Test";
    window.font = gui::Font("{mono}", 12);
    window.size = gui::Size(256, 24*3);

    auto @vl = gui::VerticalLayout();
    {
      @chk = gui::CheckLabel();
      chk.text = "Test";
      chk.onToggle(@gui::Callback(this.toggled));
      vl.append(chk, gui::Size(0, 0));

      auto @lbl = gui::Label();
      lbl.alignment = gui::Alignment(0.5, 0.5);
      lbl.text = "0123456789ABCDEF";
      lbl.foregroundColor = gui::Color(240, 240,   0);
      lbl.backgroundColor = gui::Color(0,   120, 120);
      vl.append(lbl, gui::Size(256, 48));


    }
    window.append(vl);
  }

  void toggled() {
    message("toggled!");
  }
};

void init() {
  @settings = SettingsWindow();
}
