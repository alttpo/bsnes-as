SettingsWindow @settings;

class SettingsWindow {
  private GUI::Window @window;

  SettingsWindow() {
    @window = GUI::Window();
    // TODO: this doesn't work yet; I would expect it to disable closing of the window. Tested on MacOS only so far.
    window.dismissable = false;
    window.resizable = false;
    window.visible = true;
    window.title = "Test";
    window.font = GUI::Font(GUI::Mono, 10);
    window.size = GUI::Size(256, 256);
    window.backgroundColor = GUI::Color(0, 0, 100);

    auto @vl = GUI::VerticalLayout();
    {
      auto @chk = GUI::CheckLabel();
      chk.text = "Test 1 enabled, checked";
      chk.onToggle(@GUI::Callback(this.toggled));
      chk.enabled = true;
      chk.checked = true;
      chk.setFocused();
      vl.append(chk, GUI::Size(0, 0));

      @chk = GUI::CheckLabel();
      chk.text = "Test 2 disabled, checked";
      chk.onToggle(@GUI::Callback(this.toggled));
      chk.enabled = false;
      chk.checked = true;
      vl.append(chk, GUI::Size(0, 0));

      @chk = GUI::CheckLabel();
      chk.text = "Test 3 enabled, unchecked";
      chk.onToggle(@GUI::Callback(this.toggled));
      chk.enabled = true;
      chk.checked = false;
      chk.setFocused();
      vl.append(chk, GUI::Size(0, 0));

      @chk = GUI::CheckLabel();
      chk.text = "Test 4 disabled, unchecked";
      chk.onToggle(@GUI::Callback(this.toggled));
      chk.enabled = false;
      chk.checked = false;
      vl.append(chk, GUI::Size(0, 0));

      auto @lbl = GUI::Label();
      lbl.text = "0123456789ABCDEF";
      lbl.alignment = GUI::Alignment(0.5, 0.5);
      lbl.foregroundColor = GUI::Color(240, 240,   0);
      lbl.backgroundColor = GUI::Color(0,   120, 120);
      GUI::Color fc = lbl.foregroundColor;
      GUI::Color bc = lbl.backgroundColor;
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
