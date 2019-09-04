
// string support:

struct CNallStringFactory : public asIStringFactory {
  CNallStringFactory() {}
  ~CNallStringFactory() {}

  const void *GetStringConstant(const char *data, asUINT length) {
    string *str = new string();
    str->resize(length);
    memory::copy(str->get(), str->capacity(), data, length);

    return reinterpret_cast<const void*>(str);
  }

  int ReleaseStringConstant(const void *str) {
    if (str == nullptr)
      return asERROR;

    const_cast<string *>(reinterpret_cast<const string*>(str))->reset();

    return asSUCCESS;
  }

  int GetRawStringData(const void *str, char *data, asUINT *length) const {
    if (str == nullptr)
      return asERROR;

    string *s = const_cast<string *>(reinterpret_cast<const string*>(str));

    if (length)
      *length = (asUINT)s->size();

    if (data)
      memory::copy(data, s->data(), s->size());

    return asSUCCESS;
  }
};

static CNallStringFactory *stringFactory = nullptr;

CNallStringFactory *GetStdStringFactorySingleton() {
  if(stringFactory == nullptr) {
    stringFactory = new CNallStringFactory();
  }
  return stringFactory;
}

static void ConstructString(string *thisPointer) {
  new(thisPointer) string();
}

static void CopyConstructString(const string &other, string *thisPointer) {
  new(thisPointer) string(other);
}

static void DestructString(string *thisPointer) {
  thisPointer->~string();
}

auto Interface::registerScriptString() -> void {
  int r;
  // register string type:
  r = script.engine->RegisterObjectType("string", sizeof(string), asOBJ_VALUE | asOBJ_APP_CLASS_CDAK); assert( r >= 0 );
  r = script.engine->RegisterStringFactory("string", GetStdStringFactorySingleton());

  // Register the object operator overloads
  r = script.engine->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT,  "void f()",                    asFUNCTION(ConstructString), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = script.engine->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT,  "void f(const string &in)",    asFUNCTION(CopyConstructString), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = script.engine->RegisterObjectBehaviour("string", asBEHAVE_DESTRUCT,   "void f()",                    asFUNCTION(DestructString),  asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("string", "string &opAssign(const string &in)", asMETHODPR(string, operator =, (const string&), string&), asCALL_THISCALL); assert(r >= 0);
  //todo[jsd] add more string functions if necessary
}
