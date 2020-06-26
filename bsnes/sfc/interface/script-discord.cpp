
namespace DiscordInterface {

discord::Result result;
discord::Core* core{};
discord::LogLevel minLevel = discord::LogLevel::Debug;

auto logCallback(discord::LogLevel level, const char* message) -> void {
  static const char *levelNames[] = {
    "ERROR",
    "WARN",
    "INFO",
    "DEBUG"
  };
  platform->scriptMessage({"Discord ", levelNames[static_cast<int>(level)], ": ", message}, (level == discord::LogLevel::Error));
}

auto Register(asIScriptEngine *e) -> void {
  int r;

#define REG_REF_TYPE(name) r = e->RegisterObjectType(#name, 0, asOBJ_REF); assert( r >= 0 )
#define REG_REF_FLAGS(name, flags) r = e->RegisterObjectType(#name, 0, flags); assert( r >= 0 )

#define REG_GLOBAL(defn, ptr) r = e->RegisterGlobalProperty(defn, ptr); assert( r >= 0 )

#define REG_LAMBDA(name, defn, lambda) r = e->RegisterObjectMethod(#name, defn, asFUNCTION(+lambda), asCALL_CDECL_OBJFIRST); assert( r >= 0 )

#define REG_LAMBDA_GLOBAL(defn, lambda) r = e->RegisterGlobalFunction(defn, asFUNCTION(+lambda), asCALL_CDECL); assert( r >= 0 )

#define REG_LAMBDA_CTOR(name, defn, lambda) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_FACTORY, defn, asFUNCTION(+lambda), asCALL_CDECL); assert( r >= 0 )

  {
    r = e->SetDefaultNamespace("discord"); assert(r >= 0);

    // Core type:
    //REG_REF_FLAGS(Core, asOBJ_REF | asOBJ_NOCOUNT);

    REG_GLOBAL("int result", &result);
    REG_GLOBAL("const bool created", &core); // implicit nullptr -> false

    // create():
    REG_LAMBDA_GLOBAL("int create(uint64 clientId, bool requireDiscordClient = false)", ([](uint64_t clientId, bool requireDiscordClient) {
      result = discord::Core::Create(clientId, requireDiscordClient ? DiscordCreateFlags_Default : DiscordCreateFlags_NoRequireDiscord, &core);
      if (!core) return static_cast<int>(result);

      core->SetLogHook(minLevel, logCallback);
      return static_cast<int>(result);
    }));

    // set_logLevel:
    REG_LAMBDA_GLOBAL("void set_logLevel(int level) property", ([](int level) {
      minLevel = static_cast<discord::LogLevel>(level);
      if (!core) return void(result = discord::Result::NotRunning);

      core->SetLogHook(minLevel, logCallback);
    }));

    // runCallbacks()
    REG_LAMBDA_GLOBAL("int runCallbacks()", ([]() {
      if (!core) return static_cast<int>(result = discord::Result::NotRunning);

      result = core->RunCallbacks();
      if (result == discord::Result::NotRunning) {
        delete core;
        core = nullptr;
      }

      return static_cast<int>(result);
    }));
  }
}

}