SettingsWindow @settings;

class SettingsWindow {
  private GUI::Window @window;
  GUI::ComboButton      @dd;
  GUI::HorizontalSlider @hs;

  SettingsWindow() {
    @window = GUI::Window();
    window.dismissable = false;
    window.resizable = false;
    window.visible = true;
    window.title = "Test";
    //window.font = GUI::Font(GUI::Mono, 10);
    window.size = GUI::Size(512, 512);
    //window.backgroundColor = GUI::Color(0, 0, 100);

    message(window.font.bold ? "bold" : "not bold");
    message(window.font.italic ? "italic" : "not italic");
    message(fmtFloat(window.font.size));
    string text = "";
    text += "abc";
    message(text);

    auto @vl = GUI::VerticalLayout();
    window.append(vl);
    {
      auto @chk = GUI::CheckLabel();
      vl.append(chk, GUI::Size(0, 0));
      chk.text = "Test 1 enabled, checked";
      chk.onToggle(@GUI::Callback(this.toggled));
      chk.enabled = true;
      chk.checked = true;
      chk.setFocused();

      @chk = GUI::CheckLabel();
      vl.append(chk, GUI::Size(0, 0));
      chk.text = "Test 2 disabled, checked";
      chk.onToggle(@GUI::Callback(this.toggled));
      chk.enabled = false;
      chk.checked = true;

      @chk = GUI::CheckLabel();
      vl.append(chk, GUI::Size(0, 0));
      chk.text = "Test 3 enabled, unchecked";
      chk.onToggle(@GUI::Callback(this.toggled));
      chk.enabled = true;
      chk.checked = false;

      @chk = GUI::CheckLabel();
      vl.append(chk, GUI::Size(0, 0));
      chk.text = "Test 4 disabled, unchecked";
      chk.onToggle(@GUI::Callback(this.toggled));
      chk.enabled = false;
      chk.checked = false;

      auto @lbl = GUI::Label();
      vl.append(lbl, GUI::Size(256, 48));
      lbl.text = "0123456789ABCDEF";
      lbl.alignment = GUI::Alignment(0.5, 0.5);
      lbl.foregroundColor = GUI::Color(240, 240,   0);
      lbl.backgroundColor = GUI::Color(0,   120, 120);
      GUI::Color fc = lbl.foregroundColor;
      GUI::Color bc = lbl.backgroundColor;

      auto @cv = GUI::SNESCanvas();
      vl.append(cv, GUI::Size(0, 0));
      cv.size = GUI::Size(256, 256);
      cv.luma = 4;
      cv.fill(0x03E0 | 0x8000);
      cv.update();

      @dd = GUI::ComboButton();
      vl.append(dd, GUI::Size(0, 0));

      auto @di = GUI::ComboButtonItem();
      di.text = "A";
      di.setSelected();
      dd.append(di);

      @di = GUI::ComboButtonItem();
      di.text = "B";
      dd.append(di);

      dd.onChange(@GUI::Callback(comboChanged));
      dd.enabled = true;
      message(fmtInt(dd.count()));
      message(dd.selected.text);
      message(dd[0].text);
      message(dd[1].text);

      @hs = GUI::HorizontalSlider();
      vl.append(hs, GUI::Size(160, 0));
      hs.length = 31;
      hs.position = 31;
      hs.onChange(@GUI::Callback(slideChanged));
    }
  }

  void toggled() {
    message("toggled!");
  }

  void comboChanged() {
    message("changed");
  }

  void slideChanged() {
    message(fmtInt(hs.position));
  }
};

void init() {
  @settings = SettingsWindow();
}

void pre_frame() {
  //message(fmtInt(settings.dd.count()));
  //message(settings.dd.selected.text);
  //message(settings.dd[0].text);
  //message(settings.dd[1].text);
}
