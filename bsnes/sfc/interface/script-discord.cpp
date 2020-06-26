
namespace DiscordInterface {

discord::Result result;

auto Register(asIScriptEngine *e) -> void {
  int r;

#define REG_REF_TYPE(name) r = e->RegisterObjectType(#name, 0, asOBJ_REF); assert( r >= 0 )

#define REG_GLOBAL(defn, ptr) r = e->RegisterGlobalProperty(defn, ptr); assert( r >= 0 )

#define REG_LAMBDA(name, defn, lambda) r = e->RegisterObjectMethod(#name, defn, asFUNCTION(+lambda), asCALL_CDECL_OBJFIRST); assert( r >= 0 )

#define REG_LAMBDA_GLOBAL(defn, lambda) r = e->RegisterGlobalFunction(defn, asFUNCTION(+lambda), asCALL_CDECL); assert( r >= 0 )

#define EXPOSE_SHARED_PTR(name, className) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_ADDREF,  "void f()", asFUNCTION(sharedPtrAddRef), asCALL_CDECL_OBJFIRST); assert( r >= 0 ); \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_RELEASE, "void f()", asFUNCTION(+([](shared_pointer<className>& p){ sharedPtrRelease<className>(p); })), asCALL_CDECL_OBJFIRST); assert( r >= 0 )

#define REG_LAMBDA_CTOR(name, defn, lambda) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_FACTORY, defn, asFUNCTION(+lambda), asCALL_CDECL); assert( r >= 0 )

  {
    r = e->SetDefaultNamespace("discord"); assert(r >= 0);

    // Core type:
    REG_REF_TYPE(Core);
    EXPOSE_SHARED_PTR(Core, discord::Core);

    REG_GLOBAL("int result", &result);

    // constructor:
    REG_LAMBDA_CTOR(Core, "Core@ f(uint64 clientId, bool requireDiscordClient = false)", ([](uint64_t clientId, bool requireDiscordClient) {
      discord::Core* core{};

      result = discord::Core::Create(clientId, requireDiscordClient ? DiscordCreateFlags_Default : DiscordCreateFlags_NoRequireDiscord, &core);
      if (core == nullptr) {
        return (shared_pointer<discord::Core>*)nullptr;
      }

      return new shared_pointer<discord::Core>(core);
    }));

    // RunCallbacks()
    REG_LAMBDA(Core, "int RunCallbacks()", ([](shared_pointer<discord::Core>&p){ return p->RunCallbacks(); }));
  }
}

}
