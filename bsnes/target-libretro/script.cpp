
auto Program::scriptEngine() -> asIScriptEngine * {
  return script.engine;
}

auto Program::scriptMessage(const string& msg, bool alert) -> void {
  libretro_print(RETRO_LOG_INFO, "%.*s\n", msg.size(), msg.data());

  if (alert) {
    if (environ_cb) {
      retro_message rmsg = {
        msg.data(), // msg
        180         // frames: 3 seconds
      };
      environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &rmsg);
    }
  }
}

// Implement a simple message callback function for script compiler warnings/errors
void scriptMessageCallback(const asSMessageInfo *msg, void *param) {
  // translate angelscript log level to libretro log level:
  enum retro_log_level level;
  if (msg->type == asMSGTYPE_ERROR)
    level = RETRO_LOG_ERROR;
  else if (msg->type == asMSGTYPE_WARNING)
    level = RETRO_LOG_WARN;
  else if (msg->type == asMSGTYPE_INFORMATION)
    level = RETRO_LOG_INFO;

  // format message:
  const string &text = string("{0} ({1}, {2}) : {3}").format({msg->section, msg->row, msg->col, msg->message});
  libretro_print(level, "%.*s\n", text.size(), text.data());

  // alert:
  if (environ_cb) {
    retro_message rmsg = {
      text.data(), // msg
      180         // frames: 3 seconds
    };
    environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &rmsg);
  }
}

auto Program::scriptInit() -> void {
  // initialize angelscript once on emulator startup:
  script.engine = asCreateScriptEngine();

  // use single-quoted character literals:
  script.engine->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, true);

  // Set the message callback to receive information on errors in human readable form.
  int r = script.engine->SetMessageCallback(asFUNCTION(scriptMessageCallback), 0, asCALL_CDECL);
  assert(r >= 0);

  // Let the emulator register its script definitions:
  emulator->registerScriptDefs();
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
