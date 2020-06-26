
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

auto createScriptCallback(asIScriptFunction *cb) -> std::function<void(discord::Result)> {
  if (cb == nullptr) return std::function<void(discord::Result)>(nullptr);
  return [=](discord::Result result) -> void {
    auto ctx = ::SuperFamicom::script.context;
    ctx->Prepare(cb);
    ctx->SetArgDWord(0, static_cast<int>(result));
    executeScript(ctx);
  };
}

auto Register(asIScriptEngine *e) -> void {
  int r;

#define REG_TYPE_FLAGS(name, flags) r = e->RegisterObjectType(#name, 0, flags); assert( r >= 0 )
#define REG_REF_TYPE(name) REG_TYPE_FLAGS(name, asOBJ_REF)
#define REG_REF_NOCOUNT(name) REG_TYPE_FLAGS(name, asOBJ_REF | asOBJ_NOCOUNT)

#define REG_VALUE_TYPE(name, className, flags) \
  r = e->RegisterObjectType(#name, sizeof(className), asOBJ_VALUE | flags | asGetTypeTraits<className>()); assert( r >= 0 )

#define REG_GLOBAL(defn, ptr) r = e->RegisterGlobalProperty(defn, ptr); assert( r >= 0 )

#define REG_METHOD_THISCALL(name, defn, mthd) r = e->RegisterObjectMethod(#name, defn, mthd, asCALL_THISCALL); assert( r >= 0 )

#define REG_LAMBDA(name, defn, lambda) r = e->RegisterObjectMethod(#name, defn, asFUNCTION(+lambda), asCALL_CDECL_OBJFIRST); assert( r >= 0 )
#define REG_LAMBDA_GENERIC(name, defn, lambda) r = e->RegisterObjectMethod(#name, defn, asFUNCTION(+lambda), asCALL_GENERIC); assert( r >= 0 )
#define REG_LAMBDA_BEHAVIOUR(name, bhvr, defn, lambda) r = e->RegisterObjectBehaviour(#name, bhvr, defn, asFUNCTION(+lambda), (bhvr == asBEHAVE_RELEASE ? asCALL_CDECL_OBJLAST : asCALL_CDECL)); assert( r >= 0 )

#define REG_LAMBDA_GLOBAL(defn, lambda) r = e->RegisterGlobalFunction(defn, asFUNCTION(+lambda), asCALL_CDECL); assert( r >= 0 )

#define REG_SCOPED_TYPE(name, className) \
  REG_TYPE_FLAGS(name, asOBJ_REF | asOBJ_SCOPED); \
  REG_LAMBDA_BEHAVIOUR(name, asBEHAVE_FACTORY, #name " @f()", ([](){ return new className(); })); \
  REG_LAMBDA_BEHAVIOUR(name, asBEHAVE_RELEASE, "void f()", ([](className *p){ delete p; }));

  {
    r = e->SetDefaultNamespace("discord"); assert(r >= 0);

    // Callback funcdef:
    r = e->RegisterFuncdef("void Callback(int result)"); assert( r >= 0 );

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

    // Managers:
    REG_REF_NOCOUNT(ApplicationManager);
    REG_REF_NOCOUNT(UserManager);
    REG_REF_NOCOUNT(ImageManager);
    REG_REF_NOCOUNT(ActivityManager);
    REG_REF_NOCOUNT(RelationshipManager);
    REG_REF_NOCOUNT(LobbyManager);
    REG_REF_NOCOUNT(NetworkManager);
    REG_REF_NOCOUNT(OverlayManager);
    REG_REF_NOCOUNT(StorageManager);
    REG_REF_NOCOUNT(StoreManager);
    REG_REF_NOCOUNT(VoiceManager);
    REG_REF_NOCOUNT(AchievementManager);

    REG_LAMBDA_GLOBAL("ApplicationManager@ get_applicationManager() property", ([](){
      if (!core) return result = discord::Result::NotRunning, (discord::ApplicationManager*)nullptr;
      return &core->ApplicationManager();
    }));
    REG_LAMBDA_GLOBAL("UserManager@ get_userManager() property", ([](){
      if (!core) return result = discord::Result::NotRunning, (discord::UserManager*)nullptr;
      return &core->UserManager();
    }));
    REG_LAMBDA_GLOBAL("ImageManager@ get_imageManager() property", ([](){
      if (!core) return result = discord::Result::NotRunning, (discord::ImageManager*)nullptr;
      return &core->ImageManager();
    }));
    REG_LAMBDA_GLOBAL("ActivityManager@ get_activityManager() property", ([](){
      if (!core) return result = discord::Result::NotRunning, (discord::ActivityManager*)nullptr;
      return &core->ActivityManager();
    }));
    REG_LAMBDA_GLOBAL("RelationshipManager@ get_relationshipManager() property", ([](){
      if (!core) return result = discord::Result::NotRunning, (discord::RelationshipManager*)nullptr;
      return &core->RelationshipManager();
    }));
    REG_LAMBDA_GLOBAL("LobbyManager@ get_lobbyManager() property", ([](){
      if (!core) return result = discord::Result::NotRunning, (discord::LobbyManager*)nullptr;
      return &core->LobbyManager();
    }));
    REG_LAMBDA_GLOBAL("NetworkManager@ get_networkManager() property", ([](){
      if (!core) return result = discord::Result::NotRunning, (discord::NetworkManager*)nullptr;
      return &core->NetworkManager();
    }));
    REG_LAMBDA_GLOBAL("OverlayManager@ get_overlayManager() property", ([](){
      if (!core) return result = discord::Result::NotRunning, (discord::OverlayManager*)nullptr;
      return &core->OverlayManager();
    }));
    REG_LAMBDA_GLOBAL("StorageManager@ get_storageManager() property", ([](){
      if (!core) return result = discord::Result::NotRunning, (discord::StorageManager*)nullptr;
      return &core->StorageManager();
    }));
    REG_LAMBDA_GLOBAL("StoreManager@ get_storeManager() property", ([](){
      if (!core) return result = discord::Result::NotRunning, (discord::StoreManager*)nullptr;
      return &core->StoreManager();
    }));
    REG_LAMBDA_GLOBAL("VoiceManager@ get_voiceManager() property", ([](){
      if (!core) return result = discord::Result::NotRunning, (discord::VoiceManager*)nullptr;
      return &core->VoiceManager();
    }));

    // ActivityManager:
    REG_SCOPED_TYPE(Activity, discord::Activity);
    REG_METHOD_THISCALL(Activity, "void   set_Type(int) property", asMETHOD(discord::Activity, SetType));
    REG_METHOD_THISCALL(Activity, "int    get_Type() property",    asMETHOD(discord::Activity, GetType));
    REG_METHOD_THISCALL(Activity, "void   set_ApplicationId(int64) property", asMETHOD(discord::Activity, SetApplicationId));
    REG_METHOD_THISCALL(Activity, "int64  get_ApplicationId() property",      asMETHOD(discord::Activity, GetApplicationId));
    REG_LAMBDA         (Activity, "void   set_Name(const string &in) property", ([](discord::Activity &self, string &value) { self.SetName(value.data()); }));
    REG_LAMBDA_GENERIC (Activity, "string get_Name() property",                 ([](asIScriptGeneric *g) {
      (new(g->GetAddressOfReturnLocation()) string())->assign(reinterpret_cast<discord::Activity *>(g->GetObject())->GetName());
    }));
    REG_LAMBDA         (Activity, "void   set_State(const string &in) property", ([](discord::Activity &self, string &value) { self.SetState(value.data()); }));
    REG_LAMBDA_GENERIC (Activity, "string get_State() property",                 ([](asIScriptGeneric *g) {
      (new(g->GetAddressOfReturnLocation()) string())->assign(reinterpret_cast<discord::Activity *>(g->GetObject())->GetState());
    }));
    REG_LAMBDA         (Activity, "void   set_Details(const string &in) property", ([](discord::Activity &self, string &value) { self.SetDetails(value.data()); }));
    REG_LAMBDA_GENERIC (Activity, "string get_Details() property",                 ([](asIScriptGeneric *g) {
      (new(g->GetAddressOfReturnLocation()) string())->assign(reinterpret_cast<discord::Activity *>(g->GetObject())->GetDetails());
    }));
    //ActivityTimestamps& GetTimestamps();
    //ActivityAssets& GetAssets();
    //ActivityParty& GetParty();
    //ActivitySecrets& GetSecrets();
    REG_METHOD_THISCALL(Activity, "void  set_Instance(bool) property", asMETHOD(discord::Activity, SetInstance));
    REG_METHOD_THISCALL(Activity, "bool  get_Instance() property",     asMETHOD(discord::Activity, GetInstance));

    REG_LAMBDA(
      ActivityManager,
      "void UpdateActivity(const Activity &in, Callback@)",
      ([](discord::ActivityManager& self, discord::Activity const& activity, asIScriptFunction *cb) {
        self.UpdateActivity(activity, createScriptCallback(cb));
      })
    );
    REG_LAMBDA(
      ActivityManager,
      "void ClearActivity(Callback@)",
      ([](discord::ActivityManager& self, asIScriptFunction *cb) {
        self.ClearActivity(createScriptCallback(cb));
      })
    );
  }
}

}
