auto ScriptConsole::create() -> void {
  setCollapsible();
  setVisible(false);

  consoleView.reset();
  consoleView.setBatchable(true);

  nameLabel.setText("no script loaded");
  loadButton.setText("Clear").onActivate([&] {
    clear();
  });
}

auto ScriptConsole::appendItem(const string& msg, ::Script::MessageLevel level) -> void {
  auto item = ListViewItem().setText(msg);
  switch (level) {
    case ::Script::MessageLevel::MSG_DEBUG:
      item.setIcon(Icon::Prompt::Question);
      break;
    case ::Script::MessageLevel::MSG_WARN:
      item.setIcon(Icon::Prompt::Warning);
      break;
    case ::Script::MessageLevel::MSG_ERROR:
      item.setIcon(Icon::Prompt::Error);
      break;
    case ::Script::MessageLevel::MSG_INFO:
    default:
      item.setIcon(Icon::Prompt::Information);
      break;
  }

  consoleView.append(item);
  // NOTE(jsd): hack to scroll the item into view
  item.setFocused();
}

auto ScriptConsole::clear() -> void {
  consoleView.reset();
}

auto ScriptConsole::update() -> void {
  nameLabel.setText(program.scriptHostState.location);
}
