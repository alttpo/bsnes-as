
//#include <bsnes/target-bsnes/bsnes.hpp>

// Implement a simple message callback function
void MessageCallback(const asSMessageInfo *msg, void *param)
{
  const char *type = "ERR ";
  if( msg->type == asMSGTYPE_WARNING )
    type = "WARN";
  else if( msg->type == asMSGTYPE_INFORMATION )
    type = "INFO";
  // [jsd] todo: hook this up to better systems available in bsens
  printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

auto Program::scriptInit() -> void {
  // initialize angelscript
  engine = asCreateScriptEngine();

  // Set the message callback to receive information on errors in human readable form.
  int r = engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);
  assert(r >= 0);

  // Let the emulator register its script definitions:
  emulator->registerScriptDefs();

  // load script from file:
  string scriptSource = string::read("script.as");

  // add script into module:
  asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
  r = mod->AddScriptSection("script", scriptSource.begin(), scriptSource.length());
  assert(r >= 0);

  // compile module:
  r = mod->Build();
  assert(r >= 0);
}


