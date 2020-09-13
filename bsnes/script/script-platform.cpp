
#include <script/script.hpp>

namespace Script {

auto convertMessageLevel(asEMsgType msgType) -> MessageLevel {
  switch (msgType) {
    case asMSGTYPE_ERROR:       return MSG_ERROR;
    case asMSGTYPE_WARNING:     return MSG_WARN;
    case asMSGTYPE_INFORMATION: return MSG_INFO;
    default: return MSG_DEBUG;
  }
}

auto nameMessageLevel(MessageLevel level) -> const char * {
  const char *type = "DEBUG";
  if (level == MSG_WARN)
    type = "WARN ";
  else if (level == MSG_INFO)
    type = "INFO ";
  else if (level == MSG_ERROR)
    type = "ERROR";

  return type;
}

auto Platform::scriptMessage(const string& msg, bool alert, MessageLevel level) -> void {
  printf("script: [%s] %.*s\n", nameMessageLevel(level), msg.size(), msg.data());
}

auto Platform::scriptMessageCallback(const asSMessageInfo *msg) -> void {
  auto level = convertMessageLevel(msg->type);

  scriptMessage(
    string("({0}:{1},{2}) {3}").format({
      msg->section,
      msg->row,
      msg->col,
      msg->message
    }),
    true,
    level
  );
}

auto Platform::scriptCreateEngine() -> void {
  // initialize angelscript once on emulator startup:
  auto e = asCreateScriptEngine();
  scriptEngineState.engine = e;

  // use single-quoted character literals:
  e->SetEngineProperty(asEP_USE_CHARACTER_LITERALS, true);

  // Set the message callback to receive information on errors in human readable form.
  int r = e->SetMessageCallback(
    asFUNCTION(+([](const asSMessageInfo *msg, void *param) {
      ((Platform *) param)->scriptMessageCallback(msg);
    })),
    (void*)this,
    asCALL_CDECL
  );
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

    frames.append(string("  ({1}:{2},{3}) `{0}`").format({
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
  auto message = string("!!! {0}\n  ({2}:{3},{4}) `{1}`\n")
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

auto Platform::scriptExecute(asIScriptContext *ctx) -> asUINT {
  auto r = ctx->Execute();
  scriptEngineState.profiler.resetLocation();
  return r;
}

auto Platform::scriptInvokeFunction(asIScriptFunction *func, function<void (asIScriptContext*)> prepareArgs) -> asUINT {
  return scriptInvokeFunctionWithContext(func, nullptr, prepareArgs);
}

auto Platform::scriptInvokeFunctionWithContext(asIScriptFunction *func, asIScriptContext *ctx, function<void (asIScriptContext*)> prepareArgs) -> asUINT {
  if (!func) {
    return 0;
  }

  if (!ctx) {
    ctx = scriptPrimaryContext();
  }

  ctx->Prepare(func);
  if (prepareArgs) {
    prepareArgs(ctx);
  }

  return scriptExecute(ctx);
}

auto Platform::scriptProfilerEnable(asIScriptContext *ctx) -> void {
#if defined(AS_PROFILER_ENABLE)
  scriptEngineState.profiler.enable(ctx);
#endif
}

auto Platform::scriptProfilerDisable(asIScriptContext *ctx) -> void {
#if defined(AS_PROFILER_ENABLE)
  scriptEngineState.profiler.disable(ctx);
#endif
}

}
