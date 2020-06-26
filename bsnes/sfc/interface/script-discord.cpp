
shared_pointer<discord::Core> discordCore;

auto RegisterDiscord(asIScriptEngine *e) -> void {
  int r;

#define REG_REF_TYPE(name) r = e->RegisterObjectType(#name, 0, asOBJ_REF); assert( r >= 0 )

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

    // constructor:
    REG_LAMBDA_CTOR(Core, "Core@ f(uint64 clientId)", ([](uint64_t clientId) {
      discord::Core* core{};
      auto result = discord::Core::Create(clientId, DiscordCreateFlags_Default, &core);
      if (core == nullptr) {
        asGetActiveContext()->SetException(string{"Discord Core creation failed with err=", static_cast<int>(result)}.data(), true);
        return (shared_pointer<discord::Core>*)nullptr;
      }

      return new shared_pointer<discord::Core>(core);
    }));

    // RunCallbacks()
    REG_LAMBDA(Core, "int RunCallbacks()", ([](shared_pointer<discord::Core>&p){ return p->RunCallbacks(); }));
  }
}
