
//#include <bsnes/target-bsnes/bsnes.hpp>

auto Program::scriptEngine() -> asIScriptEngine * {
  return script.engine;
}

auto Program::scriptMessage(const string& msg, bool alert) -> void {
  // append to stdout:
  printf("%.*s\n", msg.size(), msg.data());

  // append to script console:
  script.console.append(msg);
  script.console.append("\n");
  scriptConsole.update();

  // alert in status bar:
  if (alert) {
    showMessage({"[scr] ", msg});
  }
}

auto Program::presentationWindow() -> hiro::Window {
  return presentation;
}

// Implement a simple message callback function
void MessageCallback(const asSMessageInfo *msg, void *param) {
  const char *type = "ERR ";
  if (msg->type == asMSGTYPE_WARNING)
    type = "WARN";
  else if (msg->type == asMSGTYPE_INFORMATION)
    type = "INFO";
  program.scriptMessage(string("{0} ({1}, {2}) : {3} : {4}").format({msg->section, msg->row, msg->col, type, msg->message}), true);
}

auto Program::scriptInit() -> void {
  // initialize angelscript once on emulator startup:
  script.engine = asCreateScriptEngine();

  // Set the message callback to receive information on errors in human readable form.
  int r = script.engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);
  assert(r >= 0);

  // Let the emulator register its script definitions:
  emulator->registerScriptDefs();

  // Determine "recent" script folder:
  if (inode::exists(script.location)) {
    // from script path specified on command line (--script=xyz):
    script.location = {Path::real(script.location), Location::file(script.location)};
    settings.path.recent.script = script.location;
  }
  if (!inode::exists(settings.path.recent.script)) {
    // set to current folder:
    settings.path.recent.script = Path::active();
  }
  if (!inode::exists(settings.path.recent.script)) {
    // set to folder of current executable:
    settings.path.recent.script = Path::program();
  }

  // Clean up recent path to always refer to a folder:
  if (file::exists(settings.path.recent.script)) {
    settings.path.recent.script = Location::dir(settings.path.recent.script);
  }
}

auto Program::scriptLoad(bool loadDirectory) -> void {
  BrowserDialog dialog;
  dialog.setAlignment(*presentation);

  dialog.setTitle("Load AngelScript");
  dialog.setPath(path("Scripts", settings.path.recent.script));

  auto location = loadDirectory ? dialog.selectFolder() : dialog.openFile();
  if (!location) {
    return;
  }
  if (!inode::exists(location)) {
    scriptMessage({"Script file '", (location), "' not found"}, true);
    return;
  }

  script.location = location;
  if (directory::exists(script.location)) {
    settings.path.recent.script = Path::real(script.location);
  } else {
    settings.path.recent.script = Location::dir(Path::real(script.location));
  }

  emulator->loadScript(script.location);
  scriptMessage({"Script file '", (script.location), "' loaded"}, true);
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
