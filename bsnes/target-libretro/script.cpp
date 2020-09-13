
auto convertMessageLevelToRetro(::Script::MessageLevel level) -> enum retro_log_level {
  switch (level) {
    case ::Script::MSG_DEBUG: return RETRO_LOG_DEBUG;
    case ::Script::MSG_INFO:  return RETRO_LOG_INFO;
    case ::Script::MSG_WARN:  return RETRO_LOG_WARN;
    case ::Script::MSG_ERROR:
    default:
      return RETRO_LOG_ERROR;
  }
};

auto Program::scriptMessage(const string& msg, bool alert, ::Script::MessageLevel level) -> void {
  if (alert) {
    if (environ_cb) {
      string lnmsg = msg;
      lnmsg.replace("\n", " ").stripRight();

      retro_message rmsg = {
        lnmsg.data(), // msg
        180         // frames: 3 seconds
      };
      environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &rmsg);
      return;
    }
  }

  enum retro_log_level rLevel = convertMessageLevelToRetro(level);

  libretro_print(rLevel, "%.*s\n", msg.size(), msg.data());
}

// Implement a simple message callback function for script compiler warnings/errors
auto Program::scriptMessageCallback(const asSMessageInfo *msg) -> void {
  auto level = ::Script::convertMessageLevel(msg->type);

  // format message:
  string text = string("({0}:{1},{2}) {3}").format({msg->section, msg->row, msg->col, msg->message});

  scriptMessage(text, true, level);
}

auto Program::scriptInit() -> void {
  // initialize angelscript once on emulator startup:
  scriptCreateEngine();

  // Let the emulator register its script definitions:
  emulator->registerScriptDefs(this);

  scriptCreatePrimaryContext();
}

auto Program::scriptReload() -> void {
  if (!inode::exists(script.location)) {
    scriptMessage({"Script file '", (script.location), "' not found"}, true);
    return;
  }

  emulator->loadScript(script.location);
  scriptMessage({"Script file '", (script.location), "' loaded"}, true);
}

auto Program::scriptUnload() -> void {
  emulator->unloadScript();
  scriptMessage("All scripts unloaded", true);
}

auto Program::registerMenu(const string& menuName, const string& display, const string& desc) -> void {
  script.menus.insert(menuName, {menuName, display, desc});
}

auto Program::registerMenuOption(const string& menuName, const string& key, const string& desc, const string& info, const vector<string>& values) -> void {
  auto maybeMenu = script.menus.find(menuName);
  if (!maybeMenu) {
    return;
  }

  auto& menu = *maybeMenu;
  menu.options.append({key, desc, info, values, values[0]});
}

auto Program::setMenuOption(const string& menuName, const string& key, const string& value) -> void {
  auto maybeMenu = script.menus.find(menuName);
  if (!maybeMenu) {
    // no such menu:
    return;
  }

  auto& menu = *maybeMenu;
  for (auto& option : menu.options) {
    if (option.key != key) {
      continue;
    }

    // set the option's value and return:
    option.value = value;
    return;
  }
}

auto Program::getMenuOption(const string& menuName, const string& key) -> string {
  auto maybeMenu = script.menus.find(menuName);
  if (!maybeMenu) {
    // no such menu:
    return {};
  }

  auto &menu = *maybeMenu;
  for (auto &option : menu.options) {
    if (option.key != key) {
      continue;
    }

    // return the option's value:
    return option.value;
  }

  // no such option key:
  return {};
}
