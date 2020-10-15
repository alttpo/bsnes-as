
#include <sfc/sfc.hpp>

#if !defined(PLATFORM_WINDOWS)
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/ioctl.h>
  #include <netinet/in.h>
  #include <netinet/tcp.h>
  #include <netdb.h>
  #define SEND_BUF_CAST(t) ((const void *)(t))
  #define RECV_BUF_CAST(t) ((void *)(t))

int sock_capture_error() {
  return errno;
}

char* sock_error_string(int err) {
  return strerror(err);
}

bool sock_has_error(int err) {
  return err != 0;
}
#else
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #define SEND_BUF_CAST(t) ((const char *)(t))
  #define RECV_BUF_CAST(t) ((char *)(t))

/* gcc doesn't know _Thread_local from C11 yet */
#ifdef __GNUC__
# define thread_local __thread
#elif __STDC_VERSION__ >= 201112L
# define thread_local _Thread_local
#elif defined(_MSC_VER)
# define thread_local __declspec( thread )
#else
# error Cannot define thread_local
#endif

thread_local char errmsg[256];

char* sock_error_string(int errcode) {
  DWORD len = FormatMessageA(FORMAT_MESSAGE_ARGUMENT_ARRAY | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errcode, 0, errmsg, 255, NULL);
  if (len != 0)
    errmsg[len] = 0;
  else
    sprintf(errmsg, "error %d", errcode);
  return errmsg;
}

int sock_capture_error() {
  return WSAGetLastError();
}

bool sock_has_error(int err) {
  return FAILED(err);
}
#endif

#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION __FILE__ ":" S2(__LINE__)

#define REG_TYPE_FLAGS(name, flags) r = e->RegisterObjectType(#name, 0, flags); assert( r >= 0 )
#define REG_REF_TYPE(name) REG_TYPE_FLAGS(name, asOBJ_REF)
#define REG_REF_NOCOUNT(name) REG_TYPE_FLAGS(name, asOBJ_REF | asOBJ_NOCOUNT)
#define REG_REF_NOHANDLE(name) REG_TYPE_FLAGS(name, asOBJ_REF | asOBJ_NOHANDLE)

#define REG_VALUE_TYPE(name, className, flags) \
  r = e->RegisterObjectType(#name, sizeof(className), asOBJ_VALUE | flags); assert( r >= 0 )

#define REG_GLOBAL(defn, ptr) r = e->RegisterGlobalProperty(defn, ptr); assert( r >= 0 )

#define REG_FUNC(name, defn, func) r = e->RegisterObjectMethod(#name, defn, asFUNCTION(func), asCALL_CDECL_OBJFIRST); assert( r >= 0 )

#define REG_LAMBDA(name, defn, lambda) r = e->RegisterObjectMethod(#name, defn, asFUNCTION(+lambda), asCALL_CDECL_OBJFIRST); assert( r >= 0 )
#define REG_LAMBDA_GENERIC(name, defn, lambda) r = e->RegisterObjectMethod(#name, defn, asFUNCTION(+lambda), asCALL_GENERIC); assert( r >= 0 )
#define REG_LAMBDA_BEHAVIOUR(name, bhvr, defn, lambda) r = e->RegisterObjectBehaviour(#name, bhvr, defn, asFUNCTION(+lambda), ((bhvr == asBEHAVE_RELEASE || bhvr == asBEHAVE_ADDREF) ? asCALL_CDECL_OBJLAST : asCALL_CDECL)); assert( r >= 0 )

#define REG_LAMBDA_CTOR(name, defn, lambda) r = e->RegisterObjectBehaviour(#name, asBEHAVE_CONSTRUCT, defn, asFUNCTION(+lambda), asCALL_CDECL_OBJFIRST); assert( r >= 0 )
#define REG_LAMBDA_DTOR(name, defn, lambda) r = e->RegisterObjectBehaviour(#name, asBEHAVE_DESTRUCT,  defn, asFUNCTION(+lambda), asCALL_CDECL_OBJFIRST); assert( r >= 0 )

#define REG_LAMBDA_GLOBAL(defn, lambda) r = e->RegisterGlobalFunction(defn, asFUNCTION(+lambda), asCALL_CDECL); assert( r >= 0 )

#define REG_REF_SCOPED(name, className) \
  REG_TYPE_FLAGS(name, asOBJ_REF | asOBJ_SCOPED); \
  REG_LAMBDA_BEHAVIOUR(name, asBEHAVE_FACTORY, #name " @f()", ([](){ return new className(); })); \
  REG_LAMBDA_BEHAVIOUR(name, asBEHAVE_RELEASE, "void f()", ([](className *p){ delete p; }));      \
  REG_LAMBDA(name, #name " &opAssign(const " #name " &in)", ([](className& self, const className& other) -> className& { return self.operator=(other); }))

template <typename T>
void value_construct(void * address) {
  new (address) T;
}

template <typename T>
void value_destroy(T * object) {
  object->~T();
}

template <typename T>
void value_copy_construct(void * address, T * other) {
  new (address) T(*other);
}

template <typename T>
void value_assign(T * lhs, T* rhs) {
  *lhs = *rhs;
}

template <typename T> struct Deref {};

static const char *deref_error = "cannot dereference null shared_pointer; call construct() first";

template <typename T>
struct Deref<void (T::*)(void)> {
  template <void (T::*fp)(void)>
  static void f(T &self) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return;
    }
    (self.*fp)();
  }
};

template <typename T, typename R>
struct Deref<R (T::*)(void)> {
  template <R (T::*fp)(void)>
  static R f(T &self) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return R{};
    }
    return (self.*fp)();
  }

  // discard return value:
  template <R (T::*fp)(void)>
  static void d(T &self) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return;
    }
    (self.*fp)();
  }
};
template <typename T, typename R>
struct Deref<R (T::*)(void) const> {
  template <R (T::*fp)(void) const>
  static R f(T &self) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return R{};
    }
    return (self.*fp)();
  }

  // discard return value:
  template <R (T::*fp)(void) const>
  static void d(T &self) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return;
    }
    (self.*fp)();
  }
};

template <typename T, typename A0>
struct Deref<void (T::*)(A0)> {
  template <void (T::*fp)(A0)>
  static void f(T &self, A0 a0) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return;
    }
    (self.*fp)(a0);
  }
};
template <typename T, typename A0>
struct Deref<void (T::*)(A0) const> {
  template <void (T::*fp)(A0) const>
  static void f(T &self, A0 a0) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return;
    }
    (self.*fp)(a0);
  }
};

template <typename T, typename R, typename A0>
struct Deref<R (T::*)(A0)> {
  // return return value:
  template <R (T::*fp)(A0)>
  static R f(T &self, A0 a0) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return R{};
    }
    return (self.*fp)(a0);
  }

  // discard return value:
  template <R (T::*fp)(A0)>
  static void d(T &self, A0 a0) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return;
    }
    (self.*fp)(a0);
  }
};
template <typename T, typename R, typename A0>
struct Deref<R (T::*)(A0) const> {
  template <R (T::*fp)(A0) const>
  static R f(T &self, A0 a0) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return R{};
    }
    return (self.*fp)(a0);
  }

  // discard return value:
  template <R (T::*fp)(A0) const>
  static void d(T &self, A0 a0) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return;
    }
    (self.*fp)(a0);
  }
};

template <typename T, typename R, typename A0, typename A1>
struct Deref<R (T::*)(A0, A1)> {
  // return return value:
  template <R (T::*fp)(A0, A1)>
  static R f(T &self, A0 a0, A1 a1) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return R{};
    }
    return (self.*fp)(a0, a1);
  }

  // discard return value:
  template <R (T::*fp)(A0, A1)>
  static void d(T &self, A0 a0, A1 a1) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return;
    }
    (self.*fp)(a0, a1);
  }
};
template <typename T, typename R, typename A0, typename A1>
struct Deref<R (T::*)(A0, A1) const> {
  template <R (T::*fp)(A0, A1) const>
  static R f(T &self, A0 a0, A1 a1) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return R{};
    }
    return (self.*fp)(a0, a1);
  }

  // discard return value:
  template <R (T::*fp)(A0, A1) const>
  static void d(T &self, A0 a0, A1 a1) {
    auto alive = (bool)(self.ptr());
    if (!alive) {
      asGetActiveContext()->SetException(deref_error, true);
      return;
    }
    (self.*fp)(a0, a1);
  }
};
#include "script-string.cpp"

// R5G5B5 is what ends up on the final PPU frame buffer (R and B are swapped from SNES)
typedef uint16 r5g5b5;
// B5G5R5 is used by the SNES system
typedef uint16 b5g5r5;

namespace ScriptInterface {

  static auto message(const string *msg) -> void {
    platform->scriptMessage(*msg);
  }

  static auto array_to_string(CScriptArray *array, uint offs, uint size) -> string* {
    // avoid index out of range exception:
    if (array->GetSize() <= offs) return new string();
    if (array->GetSize() < offs + size) return new string();

    // append bytes from array:
    auto appended = new string();
    appended->resize(size);
    for (auto i : range(size)) {
      appended->get()[i] = *(const char *)(array->At(offs + i));
    }
    return appended;
  }

  static void uint8_array_append_uint8(CScriptArray *array, const uint8 *value) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertLast((void *)value);
      return;
    }

    array->InsertLast((void *)value);
  }

  static void uint8_array_append_uint16(CScriptArray *array, const uint16 *value) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertLast((void *)value);
      return;
    }

    array->Reserve(array->GetSize() + 2);
    array->InsertLast((void *)&((const uint8 *)value)[0]);
    array->InsertLast((void *)&((const uint8 *)value)[1]);
  }

  static void uint8_array_append_uint24(CScriptArray *array, const uint32 *value) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertLast((void *)value);
      return;
    }

    array->Reserve(array->GetSize() + 3);
    array->InsertLast((void *)&((const uint8 *)value)[0]);
    array->InsertLast((void *)&((const uint8 *)value)[1]);
    array->InsertLast((void *)&((const uint8 *)value)[2]);
  }

  static void uint8_array_append_uint32(CScriptArray *array, const uint32 *value) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertLast((void *)value);
      return;
    }

    array->Reserve(array->GetSize() + 4);
    array->InsertLast((void *)&((const uint8 *)value)[0]);
    array->InsertLast((void *)&((const uint8 *)value)[1]);
    array->InsertLast((void *)&((const uint8 *)value)[2]);
    array->InsertLast((void *)&((const uint8 *)value)[3]);
  }

  static void uint8_array_append_string(CScriptArray *array, string *other) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      return;
    }
    if (other == nullptr) {
      return;
    }

    array->Reserve(array->GetSize() + other->length());
    for (auto& c : *other) {
      array->InsertLast((void *)&c);
    }
  }

  static void uint8_array_append_array(CScriptArray *array, CScriptArray *other) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertAt(array->GetSize(), *other);
      return;
    }

    if (other->GetElementTypeId() == asTYPEID_UINT8) {
      array->InsertAt(array->GetSize(), *other);
    } else if (other->GetElementTypeId() == asTYPEID_UINT16) {
      auto insertPoint = array->GetSize();
      array->Resize(insertPoint + (other->GetSize() * 2));
      for (uint i = 0, j = insertPoint; i < other->GetSize(); i++, j += 2) {
        auto value = (const uint16 *) other->At(i);
        array->SetValue(j+0, (void *) &((const uint8 *) value)[0]);
        array->SetValue(j+1, (void *) &((const uint8 *) value)[1]);
      }
    } else if (other->GetElementTypeId() == asTYPEID_UINT32) {
      auto insertPoint = array->GetSize();
      array->Resize(insertPoint + (other->GetSize() * 4));
      for (uint i = 0, j = insertPoint; i < other->GetSize(); i++, j += 4) {
        auto value = (const uint32 *) other->At(i);
        array->SetValue(j+0, (void *) &((const uint8 *) value)[0]);
        array->SetValue(j+1, (void *) &((const uint8 *) value)[1]);
        array->SetValue(j+2, (void *) &((const uint8 *) value)[2]);
        array->SetValue(j+3, (void *) &((const uint8 *) value)[3]);
      }
    }
  }

  uint32_t  *emulatorPalette;
  uint      emulatorDepth;    // 24 or 30

  struct Callback {
    asIScriptFunction *func = nullptr;
    void              *obj  = nullptr;

    Callback(asIScriptFunction *cb) {
      //printf("cb[%p].ctor()\n", (void*)this);

      // if dealing with a delegate, only take a reference to the method and not its object:
      if (cb && cb->GetFuncType() == asFUNC_DELEGATE) {
        obj  = cb->GetDelegateObject();
        func = cb->GetDelegateFunction();

        auto c = func->AddRef();
        //printf("cb[%p].func[%p].addref -> %d\n", (void*)this, (void*)func, c);

        // release the delegate:
        c = cb->Release();
        //printf("cb[%p].cb[%p].release -> %d\n", (void*)this, (void*)cb, c);
      } else {
        func = cb;
      }
    }
    Callback(const Callback& other) : func(other.func), obj(other.obj) {
      //printf("cb[%p].copy()\n", (void*)this);
      auto c = func->AddRef();
      //printf("cb[%p].func[%p].addref -> %d\n", (void*)this, (void*)func, c);
    }

    ~Callback() {
      //printf("cb[%p].dtor()\n", (void*)this);
      auto c = func->Release();
      //printf("cb[%p].func[%p].release -> %d\n", (void*)this, (void*)func, c);
      func = nullptr;
      obj = nullptr;
    }

    auto operator()() -> void {
      auto ctx = platform->scriptCreateContext();
      platform->scriptInvokeFunctionWithContext(func, ctx, [=](asIScriptContext* ctx) {
        // use the script object for which the delegate applies to:
        // TODO: what if obj == nullptr?
        ctx->SetObject(obj);
      });
      ctx->Release();
    }
  };

  #include "script-bus.cpp"
  #include "script-ppu.cpp"
  #include "script-frame.cpp"
  #include "script-extra.cpp"
  #include "script-net.cpp"
  #include "script-gui.cpp"
  #include "script-bml.cpp"
  #include "script-discord.cpp"
  #include "script-menu.cpp"

};

auto string_format_list_factory(void *initList) -> string_format* {
  auto e = platform->scriptEngine();

  auto f = new string_format();

  asUINT length = *(asUINT *) initList;
  initList = (void *) ((asUINT *) initList + 1);

  for (asUINT i = 0; i < length; i++) {
    // angelscript gives us a typeId paired with the value for each '?' value in the initializer list:
    asUINT typeId = *((asUINT *) initList);
    initList = (void *) ((asUINT *) initList + 1);

    // grab value:
    void *value = initList;

    //asTYPEID_OBJHANDLE      = 0x40000000,
    //asTYPEID_HANDLETOCONST  = 0x20000000,
    //asTYPEID_MASK_OBJECT    = 0x1C000000,
    //asTYPEID_APPOBJECT      = 0x04000000,
    //asTYPEID_SCRIPTOBJECT   = 0x08000000,
    //asTYPEID_TEMPLATE       = 0x10000000,
    //asTYPEID_MASK_SEQNBR    = 0x03FFFFFF
    if (typeId & asTYPEID_APPOBJECT) {
      // app-defined object:
      const char *declaration = e->GetTypeDeclaration(typeId);
      //printf("typeId=%d, decl=`%s`\n", typeId, declaration);

      if (strcmp("string", declaration) != 0) {
        asIScriptContext *ctx = asGetActiveContext();
        if (ctx) {
          ctx->SetException(
            string("Unsupported app class `{0}` at list index {1}").format({declaration, i}).data()
          );
          delete f;
          return nullptr;
        }
      }

      f->append(*(string *) value);

      initList = (void *) ((string *) initList + 1);
    } else if (typeId & asTYPEID_OBJHANDLE) {
      // script-defined object:
      initList = (void *) ((void **) initList + 1);
      asITypeInfo *ti = e->GetTypeInfoById(typeId);

      // look up a toString() method:
      auto func = ti->GetMethodByDecl("string toString() const");
      if (func == nullptr) {
        asIScriptContext *ctx = asGetActiveContext();
        if (ctx) {
          ctx->SetException(
            string("Unsupported script class `{0}` at list index {1}; class does not define a `{2}` method").format(
              {
                ti->GetName(),
                i,
                "string toString() const"
              }).data());
          delete f;
          return nullptr;
        }
      }

      // execute the toString() method:
      bool isNested = false;
      asIScriptContext *ctx = asGetActiveContext();
      if (ctx) {
        if (ctx->GetEngine() == e && ctx->PushState() >= 0)
          isNested = true;
        else
          ctx = nullptr;
      }
      if (ctx == nullptr) {
        ctx = e->RequestContext();
      }

      int r;
      r = ctx->Prepare(func);
      assert(r >= 0);
      r = ctx->SetObject(*((void **) value));
      assert(r >= 0);
      r = ctx->Execute();
      if (r == asEXECUTION_FINISHED) {
        // grab the returned string:
        string *s = (string *) ctx->GetReturnObject();
        f->append(*s);
      }

      if (isNested) {
        asEContextState state = ctx->GetState();
        ctx->PopState();
        if (state == asEXECUTION_ABORTED) {
          ctx->Abort();
        }
      } else {
        e->ReturnContext(ctx);
      }
    } else {
      int size = max(4, e->GetSizeOfPrimitiveType(typeId));
      initList = (void *) ((uint8_t *) initList + size);
      switch (typeId) {
        case asTYPEID_BOOL:
          f->append(*(bool *) value);
          break;
        case asTYPEID_INT8:
          f->append(*(int8_t *) value);
          break;
        case asTYPEID_INT16:
          f->append(*(int16_t *) value);
          break;
        case asTYPEID_INT32:
          f->append(*(int32_t *) value);
          break;
        case asTYPEID_INT64:
          f->append(*(int64_t *) value);
          break;
        case asTYPEID_UINT8:
          f->append(*(uint8_t *) value);
          break;
        case asTYPEID_UINT16:
          f->append(*(uint16_t *) value);
          break;
        case asTYPEID_UINT32:
          f->append(*(uint32_t *) value);
          break;
        case asTYPEID_UINT64:
          f->append(*(uint64_t *) value);
          break;
        case asTYPEID_FLOAT:
          f->append(*(float *) value);
          break;
        case asTYPEID_DOUBLE:
          f->append(*(double *) value);
          break;
        default:
          asIScriptContext *ctx = asGetActiveContext();
          if (ctx) {
            ctx->SetException(string("Unsupported primitive typeId {0} at list index {1}").format({typeId, i}).data());
            delete f;
            return nullptr;
          }
          break;
      }
    }
  }

  return f;
}

auto Interface::paletteUpdated(uint32_t *palette, uint depth) -> void {
  //printf("Interface::paletteUpdated(%p, %d)\n", palette, depth);
  ScriptInterface::emulatorPalette = palette;
  ScriptInterface::emulatorDepth = depth;

  platform->scriptInvokeFunction(script.funcs.palette_updated);
}

auto Interface::menuOptionUpdated(const string& menuName, const string& key, const string& value) -> void {
  // TODO
}

auto Interface::registerScriptDefs(::Script::Platform *scriptPlatform) -> void {
  int r;

  auto e = platform->scriptEngine();

  // register global functions for the script to use:
  auto defaultNamespace = e->GetDefaultNamespace();

  // register array type:
  RegisterScriptArray(e, true /* bool defaultArrayType */, false /* useNative */);

  // register string type:
  registerScriptString();

  // additional array functions for serialization purposes:
  r = e->RegisterObjectMethod("array<T>", "void write_u8(const uint8 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint8), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("array<T>", "void write_u16(const uint16 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint16), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("array<T>", "void write_u24(const uint32 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint24), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("array<T>", "void write_u32(const uint32 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint32), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("array<T>", "void write_arr(const ? &in array)", asFUNCTION(ScriptInterface::uint8_array_append_array), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = e->RegisterObjectMethod("array<T>", "void write_str(const string &in other)", asFUNCTION(ScriptInterface::uint8_array_append_string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  r = e->RegisterObjectMethod("array<T>", "string &toString(uint offs, uint size) const", asFUNCTION(ScriptInterface::array_to_string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  {
    // string_format scoped ref type:
    REG_TYPE_FLAGS(string_format, asOBJ_REF | asOBJ_SCOPED);
    REG_LAMBDA_BEHAVIOUR(string_format, asBEHAVE_FACTORY, "string_format @f()", ([](){ return new string_format(); }));
    r = e->RegisterObjectBehaviour(
      "string_format",
      asBEHAVE_LIST_FACTORY,
      "string_format @f(int &in) {repeat ?}",
      asFUNCTION(string_format_list_factory),
      asCALL_CDECL
    ); assert(r >= 0);
    REG_LAMBDA_BEHAVIOUR(string_format, asBEHAVE_RELEASE, "void f()", ([](string_format *p){ delete p; }));
    REG_LAMBDA(string_format, "string_format &opAssign(const string_format &in)", ([](string_format& self, const string_format& other) -> string_format& { return self.operator=(other); }));

    // string.format:
    REG_LAMBDA(string, "string format(const string_format &in) const", ([](const string &self, string_format &fmt) -> string {
      return string(self).format(fmt);
    }));
  }

  // global function to write debug messages:
  r = e->RegisterGlobalFunction("void message(const string &in msg)", asFUNCTION(ScriptInterface::message), asCALL_CDECL); assert(r >= 0);

  {
    // integer math:
    r = e->SetDefaultNamespace("mathi"); assert(r >= 0);

    REG_LAMBDA_GLOBAL("int64 abs(int64)", ([](int64 x) -> int64 { return abs(x); }));
    REG_LAMBDA_GLOBAL("int64 sqrt(int64)", ([](int64 x) -> int64 { return sqrt(x); }));
    REG_LAMBDA_GLOBAL("int64 pow(int64, int64)", ([](int64 x, int64 y) -> int64 { return pow(x, y); }));
  }

  {
    // float math:
    r = e->SetDefaultNamespace("mathf"); assert(r >= 0);

    REG_LAMBDA_GLOBAL("float abs(float)", ([](float x) -> float { return fabs(x); }));
    REG_LAMBDA_GLOBAL("float sqrt(float)", ([](float x) -> float { return sqrt(x); }));
    REG_LAMBDA_GLOBAL("float pow(float, float)", ([](float x, float y) -> int64 { return pow(x, y); }));
  }

  // chrono namespace to get system timestamp and monotonic time:
  {
    r = e->SetDefaultNamespace("chrono::monotonic"); assert(r >= 0);

    r = e->RegisterGlobalFunction("uint64 get_nanosecond() property", asFUNCTION(chrono::nanosecond), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("uint64 get_microsecond() property", asFUNCTION(chrono::microsecond), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("uint64 get_millisecond() property", asFUNCTION(chrono::millisecond), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("uint64 get_second() property", asFUNCTION(chrono::second), asCALL_CDECL); assert(r >= 0);
  }

  {
    r = e->SetDefaultNamespace("chrono::realtime"); assert(r >= 0);

    r = e->RegisterGlobalFunction("uint64 get_nanosecond() property", asFUNCTION(chrono::realtime::nanosecond), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("uint64 get_microsecond() property", asFUNCTION(chrono::realtime::microsecond), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("uint64 get_millisecond() property", asFUNCTION(chrono::realtime::millisecond), asCALL_CDECL); assert(r >= 0);
    r = e->RegisterGlobalFunction("uint64 get_second() property", asFUNCTION(chrono::realtime::second), asCALL_CDECL); assert(r >= 0);

    // timestamp() calls the old ::time() API; should be identical to get_second but not tested that yet:
    //r = e->RegisterGlobalFunction("uint64 get_timestamp() property", asFUNCTIONPR(chrono::timestamp, (), uint64_t), asCALL_CDECL); assert(r >= 0);
  }

  ScriptInterface::RegisterBus(e);

  {
    // Order here is important as RegisterPPU sets namespace to 'ppu' and the following functions expect that.
    ScriptInterface::RegisterPPU(e);
    ScriptInterface::RegisterPPUFrame(e);
    ScriptInterface::RegisterPPUExtra(e);
  }

  ScriptInterface::RegisterNet(e);
  ScriptInterface::RegisterGUI(e);

  ScriptInterface::RegisterBML(e);

  ScriptInterface::DiscordInterface::Register(e);

  ScriptInterface::RegisterMenu(e);

  r = e->SetDefaultNamespace(defaultNamespace); assert(r >= 0);
}

auto Interface::loadScript(string location) -> void {
  int r;

  if (!inode::exists(location)) return;

  if (platform->scriptEngineState.modules) {
    unloadScript();
  }

  auto e = platform->scriptEngine();

  // create a main module:
  auto main_module = e->GetModule("main", asGM_ALWAYS_CREATE);
  platform->scriptEngineState.main_module = main_module;

  // (/parent/child.type/)
  // (/parent/child.type/)name.type
  platform->scriptEngineState.directory = Location::path(location);

  if (directory::exists(location)) {
    // add all *.as files in root directory to main module:
    for (auto scriptLocation : directory::files(location, "*.as")) {
      string path = scriptLocation;
      path.prepend(location);

      auto filename = Location::file(path);
      //platform->scriptMessage({"Loading ", path});

      // load script from file:
      string scriptSource = string::read(path);
      if (!scriptSource) {
        platform->scriptMessage({"WARN empty file at ", path});
      }

      // add script section into module:
      r = main_module->AddScriptSection(filename, scriptSource.begin(), scriptSource.length());
      if (r < 0) {
        platform->scriptMessage({"Loading ", filename, " failed"});
        return;
      }
    }

    // TODO: more modules from folders
  } else {
    // load script from single specified file:
    string scriptSource = string::read(location);

    // add script into main module:
    r = main_module->AddScriptSection(Location::file(location), scriptSource.begin(), scriptSource.length());
    assert(r >= 0);
  }

  // compile module:
  r = main_module->Build();
  assert(r >= 0);

  // track main module:
  platform->scriptEngineState.modules.append(main_module);

  // bind to functions in the main module:
  script.funcs.init = main_module->GetFunctionByDecl("void init()");
  script.funcs.unload = main_module->GetFunctionByDecl("void unload()");
  script.funcs.post_power = main_module->GetFunctionByDecl("void post_power(bool reset)");
  script.funcs.cartridge_loaded = main_module->GetFunctionByDecl("void cartridge_loaded()");
  script.funcs.cartridge_unloaded = main_module->GetFunctionByDecl("void cartridge_unloaded()");
  script.funcs.pre_nmi = main_module->GetFunctionByDecl("void pre_nmi()");
  script.funcs.pre_frame = main_module->GetFunctionByDecl("void pre_frame()");
  script.funcs.post_frame = main_module->GetFunctionByDecl("void post_frame()");
  script.funcs.palette_updated = main_module->GetFunctionByDecl("void palette_updated()");

  // enable profiler:
  platform->scriptProfilerEnable(platform->scriptPrimaryContext());

  platform->scriptInvokeFunction(script.funcs.init);
  if (loaded()) {
    platform->scriptInvokeFunction(script.funcs.cartridge_loaded);
    platform->scriptInvokeFunction(script.funcs.post_power, [](auto ctx) {
      ctx->SetArgByte(0, false); // reset = false
    });
  }
}

auto Interface::unloadScript() -> void {
  // free any references to script callbacks:
  ::SuperFamicom::bus.reset_interceptors();
  ::SuperFamicom::cpu.reset_dma_interceptor();
  ::SuperFamicom::cpu.reset_pc_callbacks();

  for (auto socket : script.sockets) {
    // release all script handles to the socket and close it but do not remove it from the script.sockets vector:
    while (!socket->release(false)) {}
  }
  script.sockets.reset();

  // unload script:
  platform->scriptInvokeFunction(script.funcs.unload);

  ScriptInterface::DiscordInterface::reset();

  platform->scriptProfilerDisable(platform->scriptPrimaryContext());

  // discard all loaded modules:
  for (auto module : platform->scriptEngineState.modules) {
    module->Discard();
  }
  platform->scriptEngineState.modules.reset();
  platform->scriptEngineState.main_module = nullptr;

  script.funcs.init = nullptr;
  script.funcs.unload = nullptr;
  script.funcs.post_power = nullptr;
  script.funcs.cartridge_loaded = nullptr;
  script.funcs.cartridge_unloaded = nullptr;
  script.funcs.pre_nmi = nullptr;
  script.funcs.pre_frame = nullptr;
  script.funcs.post_frame = nullptr;
  script.funcs.palette_updated = nullptr;

  // reset extra-tile data:
  ScriptInterface::extraLayer.reset();
}
