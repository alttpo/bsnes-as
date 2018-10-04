struct AboutWindow : Window {
  AboutWindow();

  VerticalLayout layout{this};
    Canvas canvas{&layout, Size{399, 95}, 15};
    HorizontalLayout informationLayout{&layout, Size{~0, 0}};
      Label informationLeft{&informationLayout, Size{~0, 0}, 3};
      Label informationRight{&informationLayout, Size{~0, 0}};
};

struct Presentation : Window {
  enum : uint { StatusHeight = 24 };

  Presentation();
  auto updateEmulatorMenu() -> void;
  auto updateEmulatorDeviceSelections() -> void;
  auto updateSizeMenu() -> void;
  auto configureViewport() -> void;
  auto clearViewport() -> void;
  auto resizeViewport() -> void;
  auto resizeWindow() -> void;
  auto toggleFullScreen() -> void;
  auto loadSystems() -> void;
  auto loadShaders() -> void;

  MenuBar menuBar{this};
    Menu systemsMenu{&menuBar};
    Menu systemMenu{&menuBar};
      Menu inputPort1{&systemMenu};
      Menu inputPort2{&systemMenu};
      Menu inputPort3{&systemMenu};
      MenuSeparator systemMenuSeparatorPorts{&systemMenu};
      MenuItem resetSystem{&systemMenu};
      MenuItem powerSystem{&systemMenu};
      MenuItem unloadSystem{&systemMenu};
    Menu settingsMenu{&menuBar};
      Menu sizeMenu{&settingsMenu};
      Group sizeGroup;
      Menu outputMenu{&settingsMenu};
        MenuRadioItem centerViewport{&outputMenu};
        MenuRadioItem scaleViewport{&outputMenu};
        MenuRadioItem stretchViewport{&outputMenu};
        Group outputGroup{&centerViewport, &scaleViewport, &stretchViewport};
        MenuSeparator outputSeparator{&outputMenu};
        MenuCheckItem adaptiveSizing{&outputMenu};
        MenuCheckItem aspectCorrection{&outputMenu};
        MenuCheckItem showOverscanArea{&outputMenu};
      Menu videoEmulationMenu{&settingsMenu};
        MenuCheckItem blurEmulation{&videoEmulationMenu};
        MenuCheckItem colorEmulation{&videoEmulationMenu};
        MenuCheckItem scanlineEmulation{&videoEmulationMenu};
      Menu videoShaderMenu{&settingsMenu};
        MenuRadioItem videoShaderNone{&videoShaderMenu};
        MenuRadioItem videoShaderBlur{&videoShaderMenu};
        Group videoShaders{&videoShaderNone, &videoShaderBlur};
      MenuSeparator videoSettingsSeparator{&settingsMenu};
      MenuCheckItem synchronizeVideo{&settingsMenu};
      MenuCheckItem synchronizeAudio{&settingsMenu};
      MenuCheckItem muteAudio{&settingsMenu};
      MenuCheckItem showStatusBar{&settingsMenu};
      MenuSeparator settingsSeparator{&settingsMenu};
      MenuItem showSystemSettings{&settingsMenu};
      MenuItem showVideoSettings{&settingsMenu};
      MenuItem showAudioSettings{&settingsMenu};
      MenuItem showInputSettings{&settingsMenu};
      MenuItem showHotkeySettings{&settingsMenu};
      MenuItem showAdvancedSettings{&settingsMenu};
    Menu toolsMenu{&menuBar};
      Menu saveQuickStateMenu{&toolsMenu};
        MenuItem saveSlot1{&saveQuickStateMenu};
        MenuItem saveSlot2{&saveQuickStateMenu};
        MenuItem saveSlot3{&saveQuickStateMenu};
        MenuItem saveSlot4{&saveQuickStateMenu};
        MenuItem saveSlot5{&saveQuickStateMenu};
      Menu loadQuickStateMenu{&toolsMenu};
        MenuItem loadSlot1{&loadQuickStateMenu};
        MenuItem loadSlot2{&loadQuickStateMenu};
        MenuItem loadSlot3{&loadQuickStateMenu};
        MenuItem loadSlot4{&loadQuickStateMenu};
        MenuItem loadSlot5{&loadQuickStateMenu};
      MenuCheckItem pauseEmulation{&toolsMenu};
      MenuSeparator toolsMenuSeparator{&toolsMenu};
      MenuItem cheatEditor{&toolsMenu};
      MenuItem stateManager{&toolsMenu};
      MenuItem manifestViewer{&toolsMenu};
      MenuItem gameNotes{&toolsMenu};
    Menu helpMenu{&menuBar};
      MenuItem documentation{&helpMenu};
      MenuItem credits{&helpMenu};
      MenuSeparator helpMenuSeparator{&helpMenu};
      MenuItem about{&helpMenu};

  VerticalLayout layout{this};
    HorizontalLayout viewportLayout{&layout, Size{~0, ~0}, 0};
      Viewport viewport{&viewportLayout, Size{~0, ~0}, 0};
      VerticalLayout iconLayout{&viewportLayout, Size{0, ~0}, 0};
        Widget iconBefore{&iconLayout, Size{128, ~0}, 0};
        Canvas iconCanvas{&iconLayout, Size{112, 112}, 0};
        Widget iconAfter{&iconLayout, Size{128, 8}, 0};
    HorizontalLayout statusLayout{&layout, Size{~0, StatusHeight}, 0};
      Label spacerLeft{&statusLayout, Size{8, ~0}, 0};
      Label statusMessage{&statusLayout, Size{~0, ~0}, 0};
      Label statusInfo{&statusLayout, Size{100, ~0}, 0};
      Label spacerRight{&statusLayout, Size{8, ~0}, 0};
};

extern unique_pointer<AboutWindow> aboutWindow;
extern unique_pointer<Presentation> presentation;