
auto RegisterJSON(asIScriptEngine *e) -> void {
  using Node = picojson::value;

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

#if 0
    REG_LAMBDA(Node, "Node get_opIndex(const string &in path) property", ([](Node &p, string &path) { return p[path]; }));
    REG_LAMBDA(Node, "Node create(const string &in path)",               ([](Node &p, string &path) { return p(path); }));

    REG_LAMBDA(Node, "string get_name() property",                ([](Node &p)                { return p.name(); }));
    REG_LAMBDA(Node, "void set_name(const string &in) property",  ([](Node &p, string &name)  {        p.setName(name); }));
    REG_LAMBDA(Node, "string get_value() property",               ([](Node &p)                { return p.value(); }));
    REG_LAMBDA(Node, "void set_value(const string &in) property", ([](Node &p, string &value) {        p.setValue(value); }));

    REG_LAMBDA(Node, "bool get_valid() property",     ([](Node &p) { return (bool) p; }));

    REG_LAMBDA(Node, "string get_text() property",    ([](Node &p) { return p.text(); }));
    REG_LAMBDA(Node, "bool get_boolean() property",   ([](Node &p) { return p.boolean(); }));
    REG_LAMBDA(Node, "int64 get_integer() property",  ([](Node &p) { return p.integer(); }));
    REG_LAMBDA(Node, "uint64 get_natural() property", ([](Node &p) { return p.natural(); }));
    REG_LAMBDA(Node, "float get_real() property",     ([](Node &p) { return p.real(); }));

    REG_LAMBDA(Node, "string textOr(const string &in fallback)", ([](Node &p, string &fallback)  { return p.text(fallback); }));
    REG_LAMBDA(Node, "bool booleanOr(bool fallback)",            ([](Node &p, bool fallback)     { return p.boolean(fallback); }));
    REG_LAMBDA(Node, "int64 integerOr(int64 fallback)",          ([](Node &p, int64_t fallback)  { return p.integer(fallback); }));
    REG_LAMBDA(Node, "uint64 naturalOr(uint64 fallback)",        ([](Node &p, uint64_t fallback) { return p.natural(fallback); }));
    REG_LAMBDA(Node, "float realOr(float fallback)",             ([](Node &p, double fallback)   { return p.real(fallback); }));
#endif
  }
}
