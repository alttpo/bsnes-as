
// string support:

#include <angelscript.h>

struct CNallStringFactory : public asIStringFactory {
  CNallStringFactory() = default;
  ~CNallStringFactory() override = default;

  const void *GetStringConstant(const char *data, asUINT length) override {
    string *str = new string();
    str->resize(length);
    memory::copy(str->get(), str->capacity(), data, length);

    return reinterpret_cast<const void*>(str);
  }

  int ReleaseStringConstant(const void *str) override {
    if (str == nullptr)
      return asERROR;

    const_cast<string *>(reinterpret_cast<const string*>(str))->reset();

    return asSUCCESS;
  }

  int GetRawStringData(const void *str, char *data, asUINT *length) const override {
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

static auto stringFactorySingleton() -> CNallStringFactory* {
  if(stringFactory == nullptr) {
    stringFactory = new CNallStringFactory();
  }
  return stringFactory;
}

static auto stringConstruct(string *thisPointer) -> void {
  new(thisPointer) string();
}

static auto stringCopyConstruct(const string &other, string *thisPointer) -> void {
  new(thisPointer) string(other);
}

static auto stringDestruct(string *thisPointer) -> void {
  thisPointer->~string();
}

static auto stringAddAssign(const string *other, string *self) -> string* {
  self->_append<string>(*other);
  return self;
}

static auto stringEquals(const string *self, const string *other) -> bool {
  return *self == *other;
}

static auto stringCmp(const string *self, const string *other) -> int {
  return self->compare(*other);
}

static auto stringAdd(const string *self, const string *other) -> string* {
  string *dest = new string(*self);
  dest->_append<string>(*other);
  return dest;
}

static auto stringHex(uint64_t value, int precision = 0) -> string* {
  return new string(hex(value, precision));
}

auto Interface::registerScriptString() -> void {
  int r;
  // register string type:
  r = script.engine->RegisterObjectType("string", sizeof(string), asOBJ_VALUE | asOBJ_APP_CLASS_CDAK); assert( r >= 0 );
  r = script.engine->RegisterStringFactory("string", stringFactorySingleton());

  // Register the object operator overloads
  r = script.engine->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT,  "void f()",                    asFUNCTION(stringConstruct), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = script.engine->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT,  "void f(const string &in)",    asFUNCTION(stringCopyConstruct), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = script.engine->RegisterObjectBehaviour("string", asBEHAVE_DESTRUCT,   "void f()",                    asFUNCTION(stringDestruct),  asCALL_CDECL_OBJLAST); assert(r >= 0);

  r = script.engine->RegisterObjectMethod("string", "string &opAssign(const string &in)", asMETHODPR(string, operator =, (const string&), string&), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("string", "string &opAddAssign(const string &in)", asFUNCTION(stringAddAssign), asCALL_CDECL_OBJLAST); assert( r >= 0 );
  r = script.engine->RegisterObjectMethod("string", "bool opEquals(const string &in) const", asFUNCTION(stringEquals), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
  r = script.engine->RegisterObjectMethod("string", "int opCmp(const string &in) const", asFUNCTION(stringCmp), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
  r = script.engine->RegisterObjectMethod("string", "string &opAdd(const string &in) const", asFUNCTION(stringAdd), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

  // Register global functions
  r = script.engine->RegisterGlobalFunction("string &hex(uint64 value, int precision = 0)", asFUNCTION(stringHex), asCALL_CDECL); assert(r >= 0);

  //todo[jsd] add more string functions if necessary
}
