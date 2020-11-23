
struct stringiterator {
  using T = char;

  stringiterator(T* self, uint64_t offset) : _self(self), _offset(offset) {}
  auto operator*() -> T& { return _self[_offset]; }
  auto operator!=(const stringiterator& source) const -> bool { return _offset != source._offset; }
  auto operator==(const stringiterator& source) const -> bool { return _offset == source._offset; }
  auto operator++() -> stringiterator& { return _offset++, *this; }
  auto offset() const -> uint64_t { return _offset; }

private:
  T* _self;
  uint64_t _offset;
};

auto stdToNall(const std::string& ss) -> string {
  string ns;
  ns.assign(string_view(ss.data(), ss.size()));
  ns.resize(ss.size());
  //printf("S\"%.*s\" -> N\"%.*s\"\n", ss.size(), ss.data(), ns.size(), ns.data());
  return ns;
}

auto nallToStd(const string& ns) -> std::string {
  std::string ss;
  ss.assign(ns.data(), ns.size());
  //printf("N\"%.*s\" -> S\"%.*s\"\n", ns.size(), ns.data(), ss.size(), ss.data());
  return ss;
}

auto getJSONType(picojson::value& p) -> string {
       if (p.is<picojson::null>())   return "null";
  else if (p.is<std::string>())      return "string";
  else if (p.is<picojson::object>()) return "object";
  else if (p.is<picojson::array>())  return "array";
  else if (p.is<double>())           return "number";
  else if (p.is<bool>())             return "boolean";
  else return "";
}

auto RegisterJSON(asIScriptEngine *e) -> void {
  using Value   = picojson::value;
  using Null   = picojson::null;
  using Object = picojson::object;
  using Array  = picojson::array;

  int r;

#define EXPOSE_CTOR(name, className, mClassName) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_FACTORY, #name "@ f()", asFUNCTION(+([]{ return new className(new mClassName); })), asCALL_CDECL); assert( r >= 0 )

  {
    r = e->SetDefaultNamespace("JSON"); assert(r >= 0);

    REG_VALUE_TYPE(Value, Value, asOBJ_APP_CLASS_CDAK);
    REG_VALUE_TYPE(Object, Object, asOBJ_APP_CLASS_CDAK);
    REG_VALUE_TYPE(Array, Array, asOBJ_APP_CLASS_CDAK);

    // Value:
    r = e->RegisterObjectBehaviour("Value", asBEHAVE_CONSTRUCT, "void f(const Value &in)", asFUNCTION(value_copy_construct<Value>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = e->RegisterObjectBehaviour("Value", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(value_destroy<Value>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = e->RegisterObjectMethod("Value", "Value &opAssign(const Value &in)", asFUNCTION(value_assign<Value>), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    REG_LAMBDA_BEHAVIOUR(Value, asBEHAVE_CONSTRUCT, "void f()",                 ([](void* mem){ new(mem) Value(); }));
    REG_LAMBDA_BEHAVIOUR(Value, asBEHAVE_CONSTRUCT, "void f(const string &in)", ([](void* mem, const string& value){ new(mem) Value(nallToStd(value)); }));
    REG_LAMBDA_BEHAVIOUR(Value, asBEHAVE_CONSTRUCT, "void f(const Object &in)", ([](void* mem, const Object& value){ new(mem) Value(value); }));
    REG_LAMBDA_BEHAVIOUR(Value, asBEHAVE_CONSTRUCT, "void f(const Array &in)",  ([](void* mem, const Array& value){ new(mem) Value(value); }));
    REG_LAMBDA_BEHAVIOUR(Value, asBEHAVE_CONSTRUCT, "void f(bool)",             ([](void* mem, bool value){ new(mem) Value(value); }));
    REG_LAMBDA_BEHAVIOUR(Value, asBEHAVE_CONSTRUCT, "void f(double)",           ([](void* mem, double value){ new(mem) Value(value); }));
    REG_LAMBDA_BEHAVIOUR(Value, asBEHAVE_CONSTRUCT, "void f(float)",            ([](void* mem, float value){ new(mem) Value((double)value); }));
    REG_LAMBDA_BEHAVIOUR(Value, asBEHAVE_CONSTRUCT, "void f(int64)",            ([](void* mem, int64 value){ new(mem) Value((double)value); }));
    REG_LAMBDA_BEHAVIOUR(Value, asBEHAVE_CONSTRUCT, "void f(uint64)",           ([](void* mem, uint64 value){ new(mem) Value((double)value); }));

    REG_LAMBDA_GLOBAL("Value parse(const string &in)", ([](string &text) -> Value {
      Value v;
      std::string err;

      //printf("JSON::parse(N\"%.*s\")\n", text.size(), text.data());

      stringiterator start(text.get(), 0);
      stringiterator end(text.get(), text.size());
      picojson::parse(v, start, end, &err);

      if (!err.empty()) {
        asIScriptContext *ctx = asGetActiveContext();
        ctx->SetException(err.c_str());
        return v;
      }

      return v;
    }));

    REG_LAMBDA_GLOBAL("string serialize(Value &in, bool prettify = false)", ([](Value &value, bool prettify) -> string {
      std::string ss = value.serialize(prettify);
      return stdToNall(ss);
    }));

    REG_LAMBDA(Value, "bool get_isNull() const property",    ([](Value &p) { return p.is<Null>(); }));
    REG_LAMBDA(Value, "bool get_isBoolean() const property", ([](Value &p) { return p.is<bool>(); }));
    REG_LAMBDA(Value, "bool get_isString() const property",  ([](Value &p) { return p.is<std::string>(); }));
    REG_LAMBDA(Value, "bool get_isNumber() const property",  ([](Value &p) { return p.is<double>(); }));
    REG_LAMBDA(Value, "bool get_isObject() const property",  ([](Value &p) { return p.is<Object>(); }));
    REG_LAMBDA(Value, "bool get_isArray() const property",   ([](Value &p) { return p.is<Array>(); }));

    REG_LAMBDA(Value, "string get_type() const property",    ([](Value &p) -> string { return getJSONType(p); }));

    REG_LAMBDA(Value, "string get_string() const property",  ([](Value &p) -> string {
      if (!p.is<std::string>()) {
        asGetActiveContext()->SetException(string{"JSON type mismatch; expected 'string' but found '", getJSONType(p), "'"});
        return {};
      }
      return stdToNall(p.get<std::string>());
    }));
    REG_LAMBDA(Value, "bool get_boolean() const property",   ([](Value &p) -> bool   {
      if (!p.is<bool>()) {
        asGetActiveContext()->SetException(string{"JSON type mismatch; expected 'boolean' but found '", getJSONType(p), "'"});
        return {};
      }
      return p.get<bool>();
    }));
    REG_LAMBDA(Value, "int64 get_integer() const property",  ([](Value &p) -> int64  {
      if (!p.is<double>()) {
        asGetActiveContext()->SetException(string{"JSON type mismatch; expected 'integer' but found '", getJSONType(p), "'"});
        return {};
      }
      return (int64)p.get<double>();
    }));
    REG_LAMBDA(Value, "uint64 get_natural() const property", ([](Value &p) -> uint64 {
      if (!p.is<double>()) {
        asGetActiveContext()->SetException(string{"JSON type mismatch; expected 'natural' but found '", getJSONType(p), "'"});
        return {};
      }
      return (uint64)p.get<double>();
    }));
    REG_LAMBDA(Value, "double get_real() const property",    ([](Value &p) -> double {
      if (!p.is<double>()) {
        asGetActiveContext()->SetException(string{"JSON type mismatch; expected 'double' but found '", getJSONType(p), "'"});
        return {};
      }
      return p.get<double>();
    }));
    REG_LAMBDA(Value, "Object& get_object() property", ([](Value &p) -> Object& {
      if (!p.is<Object>()) {
        asGetActiveContext()->SetException(string{"JSON type mismatch; expected 'object' but found '", getJSONType(p), "'"});
      }
      return p.get<Object>();
    }));
    REG_LAMBDA(Value, "Array&  get_array() property",  ([](Value &p) -> Array& {
      if (!p.is<Array>()) {
        asGetActiveContext()->SetException(string{"JSON type mismatch; expected 'array' but found '", getJSONType(p), "'"});
      }
      return p.get<Array>();
    }));

    // fallback getters:
    REG_LAMBDA(Value, "string stringOr(const string &in) const",  ([](Value &p, string& def) -> string {
      if (!p.is<std::string>()) { return def; }
      return stdToNall(p.get<std::string>());
    }));
    REG_LAMBDA(Value, "bool booleanOr(bool) const",   ([](Value &p, bool def) -> bool   {
      if (!p.is<bool>()) { return def; }
      return p.get<bool>();
    }));
    REG_LAMBDA(Value, "int64 integerOr(int64) const",  ([](Value &p, int64 def) -> int64  {
      if (!p.is<double>()) { return def; }
      return (int64)p.get<double>();
    }));
    REG_LAMBDA(Value, "uint64 naturalOr(uint64) const", ([](Value &p, uint64 def) -> uint64 {
      if (!p.is<double>()) { return def; }
      return (uint64)p.get<double>();
    }));
    REG_LAMBDA(Value, "double realOr(double) const",    ([](Value &p, double def) -> double {
      if (!p.is<double>()) { return def; }
      return p.get<double>();
    }));
    REG_LAMBDA(Value, "Object& objectOr(const Object &in)", ([](Value &p, Object& def) -> Object& {
      if (!p.is<Object>()) { return def; }
      return p.get<Object>();
    }));
    REG_LAMBDA(Value, "Array&  arrayOr(const Array &in)",  ([](Value &p, Array& def) -> Array& {
      if (!p.is<Array>()) { return def; }
      return p.get<Array>();
    }));

    REG_LAMBDA(Value, "void set_string(const string &in) property", ([](Value &p, const string& value) { p.set<std::string>(nallToStd(value)); }));
    REG_LAMBDA(Value, "void set_boolean(bool) property",            ([](Value &p, bool          value) { p.set<bool>(value); }));
    REG_LAMBDA(Value, "void set_integer(int64) property",           ([](Value &p, int64         value) { double tmp = (double)value; p.set<double>(tmp); }));
    REG_LAMBDA(Value, "void set_natural(uint64) property",          ([](Value &p, uint64        value) { double tmp = (double)value; p.set<double>(tmp); }));
    REG_LAMBDA(Value, "void set_real(double) property",             ([](Value &p, double        value) { p.set<double>(value); }));
    REG_LAMBDA(Value, "void set_object(Object &in) property",       ([](Value &p, Object&       value) { p.set<Object>(value); }));
    REG_LAMBDA(Value, "void set_array(Array &in) property",         ([](Value &p, Array&        value) { p.set<Array>(value); }));

    // Object:
    r = e->RegisterObjectBehaviour("Object", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(value_construct<Object>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = e->RegisterObjectBehaviour("Object", asBEHAVE_CONSTRUCT, "void f(const Object &in)", asFUNCTION(value_copy_construct<Object>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = e->RegisterObjectBehaviour("Object", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(value_destroy<Object>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = e->RegisterObjectMethod("Object", "Object &opAssign(const Object &in)", asFUNCTION(value_assign<Object>), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    REG_LAMBDA(Object, "Value& get_opIndex(const string &in) property", ([](Object &p, string& key) -> Value& { return p[nallToStd(key)]; }));
    REG_LAMBDA(Object, "void set_opIndex(const string &in, Value &in) property", ([](Object &p, string& key, const Value& value) -> void {
      p.insert(std::pair<std::string,Value>(nallToStd(key), value));
    }));
    REG_LAMBDA(Object, "uint get_length() const property", ([](Object &p) -> size_t { return p.size(); }));
    REG_LAMBDA(Object, "void remove(const string &in)", ([](Object &p, string& key) {
      p.erase(nallToStd(key));
    }));

    // Array:
    r = e->RegisterObjectBehaviour("Array", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(value_construct<Array>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = e->RegisterObjectBehaviour("Array", asBEHAVE_CONSTRUCT, "void f(const Array &in)", asFUNCTION(value_copy_construct<Array>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = e->RegisterObjectBehaviour("Array", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(value_destroy<Array>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = e->RegisterObjectMethod("Array", "Array &opAssign(const Array &in)", asFUNCTION(value_assign<Array>), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    REG_LAMBDA(Array, "Value& get_opIndex(uint) property", ([](Array &p, size_t index) -> Value& { return p[index]; }));
    REG_LAMBDA(Array, "void set_opIndex(uint, Value &in) property", ([](Array &p, size_t index, Value& value) -> void {
      p[index] = value;
    }));
    REG_LAMBDA(Array, "uint  get_length() const property", ([](Array &p) -> size_t { return p.size(); }));

    REG_LAMBDA(Array, "void resize(uint)", ([](Array &p, size_t newSize) {
      p.resize(newSize);
    }));
    REG_LAMBDA(Array, "void insertLast(Value &in)", ([](Array &p, Value& value) {
      p.push_back(value);
    }));
  }
}
