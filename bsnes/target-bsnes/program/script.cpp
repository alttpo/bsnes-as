
//#include <bsnes/target-bsnes/bsnes.hpp>

auto Program::scriptEngine() -> asIScriptEngine * {
  return script.engine;
}

auto Program::scriptMessage(const string *msg) -> void {
  //printf("script: %.*s\n", msg->size(), msg->data());
  showMessage({"script: ", *msg});
}

// Implement a simple message callback function
void MessageCallback(const asSMessageInfo *msg, void *param) {
  const char *type = "ERR ";
  if (msg->type == asMSGTYPE_WARNING)
    type = "WARN";
  else if (msg->type == asMSGTYPE_INFORMATION)
    type = "INFO";
  // [jsd] todo: hook this up to better systems available in bsnes
  printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

auto Program::scriptInit() -> void {
  // initialize angelscript
  script.engine = asCreateScriptEngine();

  // Set the message callback to receive information on errors in human readable form.
  int r = script.engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);
  assert(r >= 0);

  // Let the emulator register its script definitions:
  emulator->registerScriptDefs();
}

auto Program::scriptLoad() -> void {
  BrowserDialog dialog;
  dialog.setAlignment(*presentation);

  dialog.setTitle("Load AngelScript");
  dialog.setPath(path("Scripts", settings.path.recent.script));
  dialog.setFilters({string{"AngelScripts|*.as"}, string{"All Files|*"}});

  auto location = dialog.openObject();
  if (!inode::exists(location)) {
    showMessage({"Script file '", Location::file(script.location), "' not found"});
    return;
  }

  script.location = location;
  settings.path.recent.script = Location::dir(script.location);

  emulator->loadScript(script.location);
  //showMessage({"Script file '", Location::file(script.location), "' loaded"});
}

auto Program::scriptReload() -> void {
  if (!inode::exists(script.location)) {
    showMessage({"Script file '", Location::file(script.location), "' not found"});
    return;
  }

  emulator->loadScript(script.location);
  //showMessage({"Script file '", Location::file(script.location), "' loaded"});
}

auto Program::scriptUnload() -> void {
  emulator->unloadScript();
  showMessage("All scripts unloaded");
}