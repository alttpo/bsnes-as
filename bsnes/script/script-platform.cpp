
#include <script/script.hpp>

namespace Script {

// Implement a simple message callback function
void ScriptMessageCallback(const asSMessageInfo *msg, void *param) {
  Platform *platform = (Platform *)param;

  const char *type = "ERR ";
  if (msg->type == asMSGTYPE_WARNING)
    type = "WARN";
  else if (msg->type == asMSGTYPE_INFORMATION)
    type = "INFO";

  platform->scriptMessage(string("{0} ({1}, {2}) : {3} : {4}").format({msg->section, msg->row, msg->col, type, msg->message}), true);
}

auto Platform::scriptCreateEngine() -> void {
  // initialize angelscript once on emulator startup:
  auto e = asCreateScriptEngine();

  scriptEngineState.engine = e;

  // use single-quoted character literals:
  e->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, true);

  // Set the message callback to receive information on errors in human readable form.
  int r = e->SetMessageCallback(asFUNCTION(ScriptMessageCallback), (void*)this, asCALL_CDECL);
  assert(r >= 0);
}

}
