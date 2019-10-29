auto ScriptConsole::create() -> void {
  setCollapsible();
  setVisible(false);

  #if 0 && defined(Hiro_SourceEdit)
  consoleView.setFont(Font().setFamily(Font::Mono).setSize(10));
  #else
  consoleView.setFont(Font().setFamily(Font::Mono));
  #endif
  consoleView.setEditable(false);
  consoleView.setWordWrap(false);

  nameLabel.setText("no script loaded");
  loadButton.setText("Clear").onActivate([&] {
    program.script.console = "";
    update();
  });
}

auto ScriptConsole::update() -> void {
  nameLabel.setText(program.script.location);
  consoleView.setText(program.script.console);
  consoleView.setTextCursor(program.script.console.size());
}
