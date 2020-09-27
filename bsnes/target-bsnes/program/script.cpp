
//#include <bsnes/target-bsnes/bsnes.hpp>

auto Program::scriptMessage(const string& msg, bool alert, ::Script::MessageLevel level) -> void {
  auto levelName = ::Script::nameMessageLevel(level);

  // append to stdout:
  printf("[%s] %.*s\n", levelName, msg.size(), msg.data());

  // append to script console:
  scriptHostState.console.append("[");
  scriptHostState.console.append(levelName);
  scriptHostState.console.append("] ");
  scriptHostState.console.append(msg);
  scriptHostState.console.append("\n");
  scriptConsole.update();

  // alert in status bar:
  if (alert) {
    showMessage({"[scr] [", levelName, "] ", msg});
  }
}

auto Program::presentationWindow() -> hiro::Window {
  return presentation;
}

auto Program::scriptInit() -> void {
  scriptCreateEngine();

  // Let the emulator register its script definitions:
  emulator->registerScriptDefs(this);
  scriptCreatePrimaryContext();

  // Determine "recent" script folder:
  if (inode::exists(scriptHostState.location)) {
    // from script path specified on command line (--script=xyz):
    scriptHostState.location = Path::realfilepath(scriptHostState.location);
    settings.path.recent.script = scriptHostState.location;
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

  scriptHostState.location = location;
  if (directory::exists(scriptHostState.location)) {
    settings.path.recent.script = Path::real(scriptHostState.location);
  } else {
    settings.path.recent.script = Location::dir(Path::real(scriptHostState.location));
  }

  scriptMessage({"Compiling script file '", (scriptHostState.location), "'"}, true);
  emulator->loadScript(scriptHostState.location);
  scriptMessage({"Script file '", (scriptHostState.location), "' loaded"}, true);
}

auto Program::scriptReload() -> void {
  if (!inode::exists(scriptHostState.location)) {
    scriptMessage({"Script file '", (scriptHostState.location), "' not found"}, true);
    return;
  }

  emulator->loadScript(scriptHostState.location);
  scriptMessage({"Script file '", (scriptHostState.location), "' loaded"}, true);
}

auto Program::scriptUnload() -> void {
  emulator->unloadScript();
  scriptMessage("All scripts unloaded", true);
}
