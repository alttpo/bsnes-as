SettingsWindow @settings;

class SettingsWindow {
  private GUI::Window @window;
  GUI::ComboButton      @dd;
  GUI::HorizontalSlider @hs;

  SettingsWindow() {
    @window = GUI::Window();
    window.dismissable = false;
    window.resizable = true;
    window.visible = true;
    window.title = "Test";
    //window.font = GUI::Font(GUI::Mono, 10);
    window.size = GUI::Size(512, 608);
    //window.backgroundColor = GUI::Color(0, 0, 100);

    auto @vl = GUI::VerticalLayout();
    auto spacing = 0.0;
    vl.setSpacing(spacing);
    vl.setPadding(0, 0);
    window.append(vl);
    {
      auto @cv = GUI::Canvas();
      float w = 8192 / 16.0;
      float h = 9728 / 16.0;
      vl.append(cv, GUI::Size(-1, -1), spacing);
      cv.alignment = GUI::Alignment(0, 0);
      auto start = chrono::monotonic::millisecond;
      if (!cv.loadPNG("test.png")) {
        message("failed to load png");
      }
      auto end = chrono::monotonic::millisecond;
      message(fmtInt(end - start) + " ms");
    }
  }
};

void init() {
  @settings = SettingsWindow();
  @ppu::extra.font = ppu::fonts[0]; // "proggy-tinysz";
  ppu::extra.text_outline = true;
  ppu::extra.color = ppu::rgb(31, 31, 31);
  ppu::extra.outline_color = ppu::rgb(12, 12, 12);
}
