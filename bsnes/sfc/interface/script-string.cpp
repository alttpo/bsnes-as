
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

static auto stringDestruct(string *self) -> void {
  self->~string();
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

static auto stringAdd(const string *self, const string *other) -> string {
  string dest = *self;
  dest._append<string>(*other);
  return dest;
}

static auto fmtHex(uint64_t value, int precision = 0) -> string {
  return (hex(value, precision));
}

static auto fmtBinary(uint64_t value, int precision = 0) -> string {
  return (binary(value, precision));
}

static auto fmtInt(int64_t value) -> string {
  return string(value);
}

static auto fmtUint(uint64_t value) -> string {
  return string(value);
}

static auto stringSlice(const string *self, int offset, int length) -> string {
  return self->slice(offset, length);
}

auto Interface::registerScriptString() -> void {
  int r;
  // register string type:
  r = script.engine->RegisterObjectType("string", sizeof(string), asOBJ_VALUE | asOBJ_APP_CLASS_CDAK); assert( r >= 0 );
  r = script.engine->RegisterStringFactory("string", stringFactorySingleton());

  // Register the object operator overloads
  r = script.engine->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT,  "void f()",                       asFUNCTION(stringConstruct), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = script.engine->RegisterObjectBehaviour("string", asBEHAVE_CONSTRUCT,  "void f(const string &in)",       asFUNCTION(stringCopyConstruct), asCALL_CDECL_OBJLAST); assert(r >= 0);
  r = script.engine->RegisterObjectBehaviour("string", asBEHAVE_DESTRUCT,   "void f()",                       asFUNCTION(stringDestruct),  asCALL_CDECL_OBJLAST); assert(r >= 0);

  r = script.engine->RegisterObjectMethod("string", "string &opAssign(const string &in)", asMETHODPR(string, operator =, (const string&), string&), asCALL_THISCALL); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("string", "string &opAddAssign(const string &in)", asFUNCTION(stringAddAssign), asCALL_CDECL_OBJLAST); assert( r >= 0 );
  r = script.engine->RegisterObjectMethod("string", "bool opEquals(const string &in) const", asFUNCTION(stringEquals), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
  r = script.engine->RegisterObjectMethod("string", "int opCmp(const string &in) const", asFUNCTION(stringCmp), asCALL_CDECL_OBJFIRST); assert( r >= 0 );
  r = script.engine->RegisterObjectMethod("string", "string opAdd(const string &in) const", asFUNCTION(stringAdd), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

  r = script.engine->RegisterObjectMethod("string", "uint length() const", asMETHOD(string, length), asCALL_THISCALL); assert( r >= 0 );

  r = script.engine->RegisterObjectMethod("string", "string slice(int beginIndex, int length = -1) const", asFUNCTION(stringSlice), asCALL_CDECL_OBJFIRST); assert( r >= 0 );

  // trim and strip:
  r = script.engine->RegisterObjectMethod("string", "string trim(const string &in lhs, const string &in rhs)", asFUNCTION(+([](string &value, string &lhs, string &rhs) { return value.trim(lhs, rhs); })), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("string", "string trimLeft(const string &in lhs)", asFUNCTION(+([](string &value, string &lhs) { return value.trimLeft(lhs); })), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("string", "string trimRight(const string &in rhs)", asFUNCTION(+([](string &value, string &rhs) { return value.trimRight(rhs); })), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("string", "string strip()", asFUNCTION(+([](string &value) { return value.strip(); })), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("string", "string stripLeft()", asFUNCTION(+([](string &value) { return value.stripLeft(); })), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("string", "string stripRight()", asFUNCTION(+([](string &value) { return value.stripRight(); })), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  // conversion functions:
  r = script.engine->RegisterObjectMethod("string", "bool boolean()", asFUNCTION(+([](string &value) { return (uint8)value.boolean(); })), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("string", "int64 integer()", asFUNCTION(+([](string &value) { return (int64_t)value.integer(); })), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("string", "uint64 natural()", asFUNCTION(+([](string &value) { return (uint64_t)value.natural(); })), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("string", "uint64 hex()", asFUNCTION(+([](string &value) { return (uint64_t)value.hex(); })), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("string", "float real()", asFUNCTION(+([](string &value) { return (double)value.real(); })), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  // Register global functions
  r = script.engine->RegisterGlobalFunction("string fmtHex(uint64 value, int precision = 0)", asFUNCTION(fmtHex), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("string fmtBinary(uint64 value, int precision = 0)", asFUNCTION(fmtBinary), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("string fmtInt(int64 value)", asFUNCTION(fmtInt), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("string fmtUint(uint64 value)", asFUNCTION(fmtUint), asCALL_CDECL); assert(r >= 0);
  r = script.engine->RegisterGlobalFunction("string fmtFloat(float value)", asFUNCTION(+([](float value) { return string(value); })), asCALL_CDECL); assert(r >= 0);

}
