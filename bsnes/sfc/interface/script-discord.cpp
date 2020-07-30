
namespace DiscordInterface {

discord::Result result;
discord::Core* core{};
discord::LogLevel minLevel = discord::LogLevel::Debug;
#ifdef DISCORD_DISABLE
bool enabled = false;
#else
bool enabled = true;
#endif

auto reset() -> void {
  delete core;
  core = nullptr;
  result = discord::Result::NotRunning;
}

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
#define REG_REF_NOHANDLE(name) REG_TYPE_FLAGS(name, asOBJ_REF | asOBJ_NOHANDLE)

#define REG_VALUE_TYPE(name, className, flags) \
  r = e->RegisterObjectType(#name, sizeof(className), asOBJ_VALUE | flags | asGetTypeTraits<className>()); assert( r >= 0 )

#define REG_GLOBAL(defn, ptr) r = e->RegisterGlobalProperty(defn, ptr); assert( r >= 0 )

#define REG_METHOD_THISCALL(name, defn, mthd) r = e->RegisterObjectMethod(#name, defn, mthd, asCALL_THISCALL); assert( r >= 0 )

#define REG_LAMBDA(name, defn, lambda) r = e->RegisterObjectMethod(#name, defn, asFUNCTION(+lambda), asCALL_CDECL_OBJFIRST); assert( r >= 0 )
#define REG_LAMBDA_GENERIC(name, defn, lambda) r = e->RegisterObjectMethod(#name, defn, asFUNCTION(+lambda), asCALL_GENERIC); assert( r >= 0 )
#define REG_LAMBDA_BEHAVIOUR(name, bhvr, defn, lambda) r = e->RegisterObjectBehaviour(#name, bhvr, defn, asFUNCTION(+lambda), (bhvr == asBEHAVE_RELEASE ? asCALL_CDECL_OBJLAST : asCALL_CDECL)); assert( r >= 0 )

#define REG_LAMBDA_GLOBAL(defn, lambda) r = e->RegisterGlobalFunction(defn, asFUNCTION(+lambda), asCALL_CDECL); assert( r >= 0 )

#define REG_REF_SCOPED(name, className) \
  REG_TYPE_FLAGS(name, asOBJ_REF | asOBJ_SCOPED); \
  REG_LAMBDA_BEHAVIOUR(name, asBEHAVE_FACTORY, #name " @f()", ([](){ return new className(); })); \
  REG_LAMBDA_BEHAVIOUR(name, asBEHAVE_RELEASE, "void f()", ([](className *p){ delete p; }));

  {
    r = e->SetDefaultNamespace("discord"); assert(r >= 0);

    // Callback funcdef:
    r = e->RegisterFuncdef("void Callback(int result)"); assert( r >= 0 );

    REG_GLOBAL("const int result", &result);
    REG_GLOBAL("const bool created", &core); // implicit nullptr -> false
    // check `enabled` before attempting `create()`:
    REG_GLOBAL("const bool enabled", &enabled);

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

    // ActivityTimestamps:
    REG_REF_NOHANDLE(ActivityTimestamps);
    REG_LAMBDA(ActivityTimestamps, "void   set_Start(int64) property",
      ([](discord::ActivityTimestamps &self, int64_t value) {
        self.SetStart(value);
      })
    );
    REG_LAMBDA(ActivityTimestamps, "int64  get_Start() property",
      ([](discord::ActivityTimestamps &self) {
        return self.GetStart();
      })
    );
    REG_LAMBDA(ActivityTimestamps, "void   set_End(int64) property",
      ([](discord::ActivityTimestamps &self, int64_t value) {
        self.SetEnd(value);
      })
    );
    REG_LAMBDA(ActivityTimestamps, "int64  get_End() property",
      ([](discord::ActivityTimestamps &self) {
        return self.GetEnd();
      })
    );

    // ActivityAssets:
    REG_REF_NOHANDLE(ActivityAssets);
    REG_LAMBDA         (ActivityAssets, "void   set_LargeImage(const string &in) property",
      ([](discord::ActivityAssets &self, string &value) {
        self.SetLargeImage(value.data());
      })
    );
    REG_LAMBDA_GENERIC (ActivityAssets, "string get_LargeImage() property",
      ([](asIScriptGeneric *g) {
        auto self = reinterpret_cast<discord::ActivityAssets *>(g->GetObject());
        (new(g->GetAddressOfReturnLocation()) string())->assign(self->GetLargeImage());
      })
    );
    REG_LAMBDA         (ActivityAssets, "void   set_LargeText(const string &in) property",
      ([](discord::ActivityAssets &self, string &value) {
        self.SetLargeText(value.data());
      })
    );
    REG_LAMBDA_GENERIC (ActivityAssets, "string get_LargeText() property",
      ([](asIScriptGeneric *g) {
        auto self = reinterpret_cast<discord::ActivityAssets *>(g->GetObject());
        (new(g->GetAddressOfReturnLocation()) string())->assign(self->GetLargeText());
      })
    );
    REG_LAMBDA         (ActivityAssets, "void   set_SmallImage(const string &in) property",
      ([](discord::ActivityAssets &self, string &value) {
        self.SetSmallImage(value.data());
      })
    );
    REG_LAMBDA_GENERIC (ActivityAssets, "string get_SmallImage() property",
      ([](asIScriptGeneric *g) {
        auto self = reinterpret_cast<discord::ActivityAssets *>(g->GetObject());
        (new(g->GetAddressOfReturnLocation()) string())->assign(self->GetSmallImage());
      })
    );
    REG_LAMBDA         (ActivityAssets, "void   set_SmallText(const string &in) property",
      ([](discord::ActivityAssets &self, string &value) {
        self.SetSmallText(value.data());
      })
    );
    REG_LAMBDA_GENERIC (ActivityAssets, "string get_SmallText() property",
      ([](asIScriptGeneric *g) {
        auto self = reinterpret_cast<discord::ActivityAssets *>(g->GetObject());
        (new(g->GetAddressOfReturnLocation()) string())->assign(self->GetSmallText());
      })
    );

    // PartySize:
    REG_REF_NOHANDLE(PartySize);
    REG_METHOD_THISCALL(PartySize, "void   set_CurrentSize(int32) property", asMETHOD(discord::PartySize, SetCurrentSize));
    REG_METHOD_THISCALL(PartySize, "int32  get_CurrentSize() property",      asMETHOD(discord::PartySize, GetCurrentSize));
    REG_METHOD_THISCALL(PartySize, "void   set_MaxSize(int32) property",     asMETHOD(discord::PartySize, SetMaxSize));
    REG_METHOD_THISCALL(PartySize, "int32  get_MaxSize() property",          asMETHOD(discord::PartySize, GetMaxSize));

    // ActivityParty:
    REG_REF_NOHANDLE(ActivityParty);
    REG_LAMBDA         (ActivityParty, "void   set_Id(const string &in) property", ([](discord::ActivityParty &self, string &value) { self.SetId(value.data()); }));
    REG_LAMBDA_GENERIC (ActivityParty, "string get_Id() property",                 ([](asIScriptGeneric *g) {
      (new(g->GetAddressOfReturnLocation()) string())->assign(reinterpret_cast<discord::ActivityParty *>(g->GetObject())->GetId());
    }));
    REG_METHOD_THISCALL(ActivityParty, "PartySize& get_Size() property",      asMETHODPR(discord::ActivityParty, GetSize, (void), discord::PartySize&));

    // ActivitySecrets:
    REG_REF_NOHANDLE(ActivitySecrets);
    REG_LAMBDA         (ActivitySecrets, "void   set_Match(const string &in) property", ([](discord::ActivitySecrets &self, string &value) { self.SetMatch(value.data()); }));
    REG_LAMBDA_GENERIC (ActivitySecrets, "string get_Match() property",                 ([](asIScriptGeneric *g) {
      (new(g->GetAddressOfReturnLocation()) string())->assign(reinterpret_cast<discord::ActivitySecrets *>(g->GetObject())->GetMatch());
    }));
    REG_LAMBDA         (ActivitySecrets, "void   set_Join(const string &in) property", ([](discord::ActivitySecrets &self, string &value) { self.SetJoin(value.data()); }));
    REG_LAMBDA_GENERIC (ActivitySecrets, "string get_Join() property",                 ([](asIScriptGeneric *g) {
      (new(g->GetAddressOfReturnLocation()) string())->assign(reinterpret_cast<discord::ActivitySecrets *>(g->GetObject())->GetJoin());
    }));
    REG_LAMBDA         (ActivitySecrets, "void   set_Spectate(const string &in) property", ([](discord::ActivitySecrets &self, string &value) { self.SetSpectate(value.data()); }));
    REG_LAMBDA_GENERIC (ActivitySecrets, "string get_Spectate() property",                 ([](asIScriptGeneric *g) {
      (new(g->GetAddressOfReturnLocation()) string())->assign(reinterpret_cast<discord::ActivitySecrets *>(g->GetObject())->GetSpectate());
    }));

    // Activity:
    REG_REF_SCOPED (Activity, discord::Activity);
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
    REG_METHOD_THISCALL(Activity, "ActivityTimestamps& get_Timestamps() property", asMETHODPR(discord::Activity, GetTimestamps, (void), discord::ActivityTimestamps&));
    REG_METHOD_THISCALL(Activity, "ActivityAssets&     get_Assets() property",     asMETHODPR(discord::Activity, GetAssets, (void), discord::ActivityAssets&));
    REG_METHOD_THISCALL(Activity, "ActivityParty&      get_Party() property",      asMETHODPR(discord::Activity, GetParty, (void), discord::ActivityParty&));
    REG_METHOD_THISCALL(Activity, "ActivitySecrets&    get_Secrets() property",    asMETHODPR(discord::Activity, GetSecrets, (void), discord::ActivitySecrets&));
    REG_METHOD_THISCALL(Activity, "void  set_Instance(bool) property", asMETHOD(discord::Activity, SetInstance));
    REG_METHOD_THISCALL(Activity, "bool  get_Instance() property",     asMETHOD(discord::Activity, GetInstance));

    // ActivityManager:
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
