
#include <script/script.hpp>

namespace Script {

auto Platform::scriptMessage(const string& msg, bool alert) -> void {
  printf("script: %.*s\n", msg.size(), msg.data());
}

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

auto Platform::scriptEngine() -> asIScriptEngine* {
  return scriptEngineState.engine;
}

auto Platform::getStackTrace(asIScriptContext *ctx) -> vector<string> {
  vector<string> frames;

  for (asUINT n = 1; n < ctx->GetCallstackSize(); n++) {
    asIScriptFunction *func;
    const char *scriptSection;
    int line, column;

    func = ctx->GetFunction(n);
    line = ctx->GetLineNumber(n, &column, &scriptSection);

    frames.append(string("  `{0}` {1}:{2}:{3}").format({
                                                         func->GetDeclaration(),
                                                         scriptSection,
                                                         line,
                                                         column
                                                       }));
  }

  return frames;
}

auto Platform::exceptionCallback(asIScriptContext *ctx) -> void {
  asIScriptEngine *engine = ctx->GetEngine();

  // Determine the exception that occurred
  const asIScriptFunction *function = ctx->GetExceptionFunction();
  const char *scriptSection;
  int line, column;
  line = ctx->GetExceptionLineNumber(&column, &scriptSection);

  // format main message:
  auto message = string("EXCEPTION `{0}`\n  `{1}` {2}:{3}:{4}\n")
    .format({
              ctx->GetExceptionString(),
              function->GetDeclaration(),
              scriptSection,
              line,
              column
            });

  // append stack trace:
  message.append(getStackTrace(ctx).merge("\n"));

  scriptMessage(message, true);
}

auto Platform::scriptCreateContext() -> asIScriptContext* {
  auto e = scriptEngineState.engine;

  // create context:
  auto context = e->CreateContext();

  //script.context->SetExceptionCallback(asMETHOD(ScriptInterface::ExceptionHandler, exceptionCallback), &ScriptInterface::exceptionHandler, asCALL_THISCALL);
  context->SetExceptionCallback(
    asFUNCTION(+([](asIScriptContext* ctx, Platform& self) {
      self.exceptionCallback(ctx);
    })),
    (void *)this,
    asCALL_CDECL
  );

  return context;
}

auto Platform::scriptCreatePrimaryContext() -> void {
  scriptEngineState.context = scriptCreateContext();
}

auto Platform::scriptPrimaryContext() -> asIScriptContext* {
  return scriptEngineState.context;
}

}
