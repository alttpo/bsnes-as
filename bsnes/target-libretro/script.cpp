
auto Program::scriptEngine() -> asIScriptEngine * {
  return script.engine;
}

auto Program::scriptMessage(const string& msg, bool alert) -> void {
  libretro_print(alert ? RETRO_LOG_ERROR : RETRO_LOG_INFO, "%.*s\n", msg.size(), msg.data());
}

// Implement a simple message callback function
void scriptMessageCallback(const asSMessageInfo *msg, void *param) {
  const char *type = "ERR ";
  if (msg->type == asMSGTYPE_WARNING)
    type = "WARN";
  else if (msg->type == asMSGTYPE_INFORMATION)
    type = "INFO";
  program->scriptMessage(string("{0} ({1}, {2}) : {3} : {4}").format({msg->section, msg->row, msg->col, type, msg->message}), true);
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
