SettingsWindow @settings;

class SettingsWindow {
  private gui::Window @window;
  private gui::CheckLabel @chk;

  SettingsWindow() {
    @window = gui::Window();
    window.visible = true;
    window.title = "Test";
    window.size = gui::Size(256, 24*3);

    @chk = gui::CheckLabel();
    chk.text = "Test";
    chk.onToggle(@gui::Callback(this.toggled));

    auto vl = gui::VerticalLayout();
    {
      //vl.append(ok, gui::Size(0, 0));
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
