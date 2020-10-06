
auto RegisterBML(asIScriptEngine *e) -> void {
  using Node        = nall::Markup::Node;
  using SharedNode  = nall::Markup::SharedNode;
  using ManagedNode = nall::Markup::ManagedNode;

  int r;

#define EXPOSE_CTOR(name, className, mClassName) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_FACTORY, #name "@ f()", asFUNCTION(+([]{ return new className(new mClassName); })), asCALL_CDECL); assert( r >= 0 )

  {
    r = e->SetDefaultNamespace("BML"); assert(r >= 0);

    REG_REF_TYPE(Node);
    EXPOSE_CTOR(Node, SharedNode, ManagedNode);
    EXPOSE_SHARED_PTR(Node, SharedNode, ManagedNode);

    REG_LAMBDA(Node, "Node @get_opIndex(const string &in path) property", ([](SharedNode &p, string &path) { return new SharedNode((SharedNode) Node(p)[path]); }));
    REG_LAMBDA(Node, "Node @create(const string &in path)",               ([](SharedNode &p, string &path) { return new SharedNode((SharedNode) Node(p)(path)); }));

    REG_LAMBDA(Node, "string get_name() property",                ([](SharedNode &p)                { return Node(p).name(); }));
    REG_LAMBDA(Node, "void set_name(const string &in) property",  ([](SharedNode &p, string &name)  { Node(p).setName(name); }));
    REG_LAMBDA(Node, "string get_value() property",               ([](SharedNode &p)                { return Node(p).value(); }));
    REG_LAMBDA(Node, "void set_value(const string &in) property", ([](SharedNode &p, string &value) { Node(p).setValue(value); }));

    REG_LAMBDA(Node, "bool get_valid() property",     ([](SharedNode &p) { return (bool) Node(p); }));

    REG_LAMBDA(Node, "string get_text() property",    ([](SharedNode &p) { return Node(p).text(); }));
    REG_LAMBDA(Node, "bool get_boolean() property",   ([](SharedNode &p) { return Node(p).boolean(); }));
    REG_LAMBDA(Node, "int64 get_integer() property",  ([](SharedNode &p) { return Node(p).integer(); }));
    REG_LAMBDA(Node, "uint64 get_natural() property", ([](SharedNode &p) { return Node(p).natural(); }));
    REG_LAMBDA(Node, "float get_real() property",     ([](SharedNode &p) { return Node(p).real(); }));

    REG_LAMBDA(Node, "string textOr(const string &in fallback)", ([](SharedNode &p, string &fallback)  { return Node(p).text(fallback); }));
    REG_LAMBDA(Node, "bool booleanOr(bool fallback)",            ([](SharedNode &p, bool fallback)     { return Node(p).boolean(fallback); }));
    REG_LAMBDA(Node, "int64 integerOr(int64 fallback)",          ([](SharedNode &p, int64_t fallback)  { return Node(p).integer(fallback); }));
    REG_LAMBDA(Node, "uint64 naturalOr(uint64 fallback)",        ([](SharedNode &p, uint64_t fallback) { return Node(p).natural(fallback); }));
    REG_LAMBDA(Node, "float realOr(float fallback)",             ([](SharedNode &p, double fallback)   { return Node(p).real(fallback); }));
  }

  // load/save *.bml files in user's settings folder:
  {
    r = e->SetDefaultNamespace("UserSettings"); assert(r >= 0);

    REG_LAMBDA_GLOBAL("BML::Node @load(const string &in path)", ([](string &path) {
      auto document = BML::unserialize(file::read({Path::userSettings(), "bsnes/", path}));
      return new SharedNode( (SharedNode)document );
    }));

    REG_LAMBDA_GLOBAL("void save(const string &in path, BML::Node @document)", ([](string &path, SharedNode &p) {
      file::write({Path::userSettings(), "bsnes/", path}, BML::serialize(Node(p)));
    }));
  }

  // load/save *.bml files in script folder:
  {
    r = e->SetDefaultNamespace("ScriptFiles"); assert(r >= 0);

    REG_LAMBDA_GLOBAL("BML::Node @loadBML(const string &in path)", ([](string &relpath) {
      string path;
      path.append(platform->scriptEngineState.directory);
      path.append(relpath);

      auto document = BML::unserialize(file::read(path));
      return new SharedNode( (SharedNode)document );
    }));

    REG_LAMBDA_GLOBAL("void saveBML(const string &in path, BML::Node @document)", ([](string &relpath, SharedNode &p) {
      string path;
      path.append(platform->scriptEngineState.directory);
      path.append(relpath);

      file::write(path, BML::serialize(Node(p)));
    }));
  }
}
