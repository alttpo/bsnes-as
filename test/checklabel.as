SettingsWindow @settings;

class SettingsWindow {
  private GUI::Window window;
  GUI::ComboButton      dd;
  GUI::HorizontalSlider hs;

  SettingsWindow() {
    auto node = UserSettings::load("test.bml");
    message("'" + node["test"].text + "'");
    message("'" + node["test"].textOr("fallback") + "'");

    node = BML::Node();
    node.create("test").value = "test";
    node.create("test/two").value = "three";
    UserSettings::save("test.bml", node);

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

    auto vl = GUI::VerticalLayout();
    auto spacing = 8.0;
    vl.setSpacing(spacing);
    vl.setPadding(5, 5);
    window.append(vl);
    {
      auto chk = GUI::CheckLabel();
      vl.append(chk, GUI::Size(0, 0), spacing);
      chk.text = "Test 1 enabled, checked";
      chk.onToggle(@GUI::Callback(this.toggled));
      chk.enabled = true;
      chk.checked = true;
      chk.setFocused();

      chk = GUI::CheckLabel();
      vl.append(chk, GUI::Size(0, 0), spacing);
      chk.text = "Test 2 disabled, checked";
      chk.onToggle(@GUI::Callback(this.toggled));
      chk.enabled = false;
      chk.checked = true;

      chk = GUI::CheckLabel();
      vl.append(chk, GUI::Size(0, 0), spacing);
      chk.text = "Test 3 enabled, unchecked";
      chk.onToggle(@GUI::Callback(this.toggled));
      chk.enabled = true;
      chk.checked = false;

      chk = GUI::CheckLabel();
      vl.append(chk, GUI::Size(0, 0), spacing);
      chk.text = "Test 4 disabled, unchecked";
      chk.onToggle(@GUI::Callback(this.toggled));
      chk.enabled = false;
      chk.checked = false;

      auto lbl = GUI::Label();
      vl.append(lbl, GUI::Size(256, 48), spacing);
      lbl.text = "0123456789ABCDEF";
      lbl.toolTip = "tool tip";
      lbl.alignment = GUI::Alignment(0.5, 0.5);
      lbl.foregroundColor = GUI::Color(240, 240,   0);
      lbl.backgroundColor = GUI::Color(0,   120, 120);
      GUI::Color fc = lbl.foregroundColor;
      GUI::Color bc = lbl.backgroundColor;

      auto cv = GUI::SNESCanvas();
      vl.append(cv, GUI::Size(0, 0), spacing);
      cv.size = GUI::Size(256, 256);
      cv.luma = 4;
      cv.fill(0x03E0 | 0x8000);
      cv.update();

      vl.append(dd, GUI::Size(0, 0));

      auto di = GUI::ComboButtonItem();
      di.attributes["test"] = "value";
      message("attr['test'] = " + di.attributes["test"]);
      di.text = "A";
      di.setSelected();
      dd.append(di);

      di = GUI::ComboButtonItem();
      di.attributes["test2"] = "value2";
      message("attr['test2'] = " + di.attributes["test2"]);
      di.text = "B";
      dd.append(di);

      dd.onChange(@GUI::Callback(comboChanged));
      dd.enabled = true;
      dd.toolTip = "tool tip";
      message(fmtInt(dd.count()));
      message(dd.selected.text);
      message(dd[0].text);
      message(dd[0].attributes["test"]);
      message(dd[1].text);
      message(dd[1].attributes["test2"]);

      vl.append(hs, GUI::Size(200, 0), spacing);
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
  @settings = @SettingsWindow();
  @ppu::extra.font = ppu::fonts[0]; // "proggy-tinysz";
  ppu::extra.text_outline = true;
  ppu::extra.color = ppu::rgb(31, 31, 31);
  ppu::extra.outline_color = ppu::rgb(12, 12, 12);
  message(fmtInt(ppu::extra.font.height));
}

void pre_frame() {
  //message(fmtInt(settings.dd.count()));
  //message(settings.dd.selected.text);
  //message(settings.dd[0].text);
  //message(settings.dd[1].text);
}

void post_frame() {
  auto text = "UPPER lower CaSe Test";
  auto @tile = ppu::extra[0];
  tile.reset();
  tile.index = 0;
  tile.priority = 2;
  tile.source = 4;
  tile.width = ppu::extra.font.measureText(text) + 2;
  tile.height = ppu::extra.font.height + 2;
  tile.text(1, 1, text);
  tile.x = 128 - tile.width / 2;
  tile.y = 112;
  ppu::extra.count = 1;
}
