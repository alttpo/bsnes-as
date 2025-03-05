auto ScriptConsole::create() -> void {
  setCollapsible();
  setVisible(false);

  consoleView.reset();
  consoleView.setBatchable(true);

  nameLabel.setText("no script loaded");
  tailOption.setText("Tail").setChecked(tail).onToggle([&]{
    tail = tailOption.checked();
  });
  copyButton.setText("Copy").onActivate([&] {
#if defined(PLATFORM_WINDOWS)
    const unsigned crlf_size = 2;
    const char *crlf = "\r\n";
#else
    const unsigned crlf_size = 1;
    const char *crlf = "\n";
#endif

    // grab selected items else select all:
    auto batch = consoleView.batched();
    if (batch.size() == 0) {
      batch = consoleView.items();
    }

    // measure total length of buffer to build:
    unsigned len = 0;
    batch.foreach([&](const ListViewItem &item) { len += item.text().size() + crlf_size; });

    // reserve the buffer:
    string buffer;
    buffer.reserve(len);

    // append all items to the buffer:
    batch.foreach([&](const ListViewItem &item) {
      buffer.append(item.text());
      buffer.append(crlf);
    });
    // remove trailing '\n':
    buffer.resize(buffer.size() - crlf_size);

    // paste to the pasteboard:
    ::hiro::Pasteboard::pasteString(buffer);
  });
  clearButton.setText("Clear").onActivate([&] {
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

  if (tail) {
    // NOTE(jsd): hack to scroll the item into view
    item.setFocused();
  }
}

auto ScriptConsole::clear() -> void {
  consoleView.reset();
}

auto ScriptConsole::update() -> void {
  nameLabel.setText(program.scriptHostState.location);
}
