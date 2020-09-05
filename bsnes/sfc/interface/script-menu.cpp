
auto RegisterMenu(asIScriptEngine *e) -> void {
  using Node        = nall::Markup::Node;
  using SharedNode  = nall::Markup::SharedNode;
  using ManagedNode = nall::Markup::ManagedNode;

  int r;

#define EXPOSE_CTOR(name, className, mClassName) \
  r = e->RegisterObjectBehaviour(#name, asBEHAVE_FACTORY, #name "@ f()", asFUNCTION(+([]{ return new className(new mClassName); })), asCALL_CDECL); assert( r >= 0 )

  {
    r = e->SetDefaultNamespace("Menu"); assert(r >= 0);

    REG_LAMBDA_GLOBAL(
      "void registerMenu(const string &in menuName, const string &in display, const string &in desc)",
      ([](const string& menuName, const string& display, const string& desc) -> void {
        platform->registerMenu(menuName, display, desc);
      })
    );

    REG_LAMBDA_GLOBAL(
      "void registerMenuOption(const string &in menuName, const string &in key, const string &in desc, const string &in info, const array<string> &in values)",
      ([](const string& menuName, const string& key, const string& desc, const string& info, CScriptArray *valuesArray) -> void {
        // translate CScriptArray to vector<string>:
        int len = valuesArray->GetSize();

        vector<string> values;
        values.reserve(len);
        for (int i = 0; i < len; i++) {
          values.append(*(string *) valuesArray->At(i));
        }

        platform->registerMenuOption(menuName, key, desc, info, values);
      })
    );
  }
}
