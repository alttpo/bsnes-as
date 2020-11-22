
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
  //printf("S\"%.*s\" -> N\"%.*s\"\n", ss.size(), ss.data(), ns.size(), ns.data());
  return ns;
}

auto nallToStd(const string& ns) -> std::string {
  std::string ss;
  ss.assign(ns.data(), ns.size());
  //printf("N\"%.*s\" -> S\"%.*s\"\n", ns.size(), ns.data(), ss.size(), ss.data());
  return ss;
}

auto RegisterJSON(asIScriptEngine *e) -> void {
  using Node   = picojson::value;
  using Null   = picojson::null;
  using Object = picojson::object;
  using Array  = picojson::array;

  int r;

#define EXPOSE_CTOR(name, className, mClassName) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_FACTORY, #name "@ f()", asFUNCTION(+([]{ return new className(new mClassName); })), asCALL_CDECL); assert( r >= 0 )

  {
    r = e->SetDefaultNamespace("JSON"); assert(r >= 0);

    REG_VALUE_TYPE(Node, Node, asOBJ_APP_CLASS_CDAK);
    r = e->RegisterObjectBehaviour("Node", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(value_construct<Node>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = e->RegisterObjectBehaviour("Node", asBEHAVE_CONSTRUCT, "void f(const Node &in)", asFUNCTION(value_copy_construct<Node>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = e->RegisterObjectBehaviour("Node", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(value_destroy<Node>), asCALL_CDECL_OBJFIRST); assert(r >= 0);
    r = e->RegisterObjectMethod("Node", "Node &opAssign(const Node &in)", asFUNCTION(value_assign<Node>), asCALL_CDECL_OBJFIRST); assert(r >= 0);

    REG_REF_NOCOUNT(Object);
    REG_REF_NOCOUNT(Array);

    REG_LAMBDA_GLOBAL("Node parse(const string &in)", ([](string &text) -> Node {
      Node v;
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

    REG_LAMBDA(Node, "bool get_isNull() property",    ([](Node &p) { return p.is<Null>(); }));

    REG_LAMBDA(Node, "string get_text() property",    ([](Node &p) -> string { return stdToNall(p.get<std::string>()); }));
    REG_LAMBDA(Node, "bool get_boolean() property",   ([](Node &p) -> bool { return p.get<bool>(); }));
    REG_LAMBDA(Node, "int64 get_integer() property",  ([](Node &p) -> int64 { return (int64)p.get<double>(); }));
    REG_LAMBDA(Node, "uint64 get_natural() property", ([](Node &p) -> uint64 { return (uint64)p.get<double>(); }));
    REG_LAMBDA(Node, "double get_real() property",    ([](Node &p) -> double { return p.get<double>(); }));

    REG_LAMBDA(Node,   "Object@ get_object() property", ([](Node &p) -> Object& { return p.get<Object>(); }));
    REG_LAMBDA(Object, "Node&   get_opIndex(const string &in) property", ([](Object &p, string& key) -> Node& { return p[nallToStd(key)]; }));
    REG_LAMBDA(Object, "uint    get_length() property",   ([](Object &p) -> size_t { return p.size(); }));

    REG_LAMBDA(Node,  "Array@  get_array() property",       ([](Node &p) -> Array& { return p.get<Array>(); }));
    REG_LAMBDA(Array, "Node&   get_opIndex(uint) property", ([](Array &p, size_t index) -> Node& { return p[index]; }));
    REG_LAMBDA(Array, "uint    get_length() property",      ([](Array &p) -> size_t { return p.size(); }));

#if 0
    REG_LAMBDA(Node, "string textOr(const string &in fallback)", ([](Node &p, string &fallback)  { return p.text(fallback); }));
    REG_LAMBDA(Node, "bool booleanOr(bool fallback)",            ([](Node &p, bool fallback)     { return p.boolean(fallback); }));
    REG_LAMBDA(Node, "int64 integerOr(int64 fallback)",          ([](Node &p, int64_t fallback)  { return p.integer(fallback); }));
    REG_LAMBDA(Node, "uint64 naturalOr(uint64 fallback)",        ([](Node &p, uint64_t fallback) { return p.natural(fallback); }));
    REG_LAMBDA(Node, "float realOr(float fallback)",             ([](Node &p, double fallback)   { return p.real(fallback); }));
#endif
  }
}
