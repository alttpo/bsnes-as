SettingsWindow @settings;

class SettingsWindow {
  private GUI::Window @window;
  private GUI::CheckLabel @chk;

  SettingsWindow() {
    @window = GUI::Window();
    window.visible = true;
    window.title = "Test";
    window.font = GUI::Font("{mono}", 12);
    window.size = GUI::Size(256, 24*3);

    auto @vl = GUI::VerticalLayout();
    {
      @chk = GUI::CheckLabel();
      chk.text = "Test";
      chk.onToggle(@GUI::Callback(this.toggled));
      vl.append(chk, GUI::Size(0, 0));

      auto @lbl = GUI::Label();
      lbl.alignment = GUI::Alignment(0.5, 0.5);
      lbl.text = "0123456789ABCDEF";
      lbl.foregroundColor = GUI::Color(240, 240,   0);
      lbl.backgroundColor = GUI::Color(0,   120, 120);
      vl.append(lbl, GUI::Size(256, 48));


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
