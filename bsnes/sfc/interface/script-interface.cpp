
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

#include "vga-charset.cpp"
#include "script-string.cpp"

// R5G5B5 is what ends up on the final PPU frame buffer (R and B are swapped from SNES)
typedef uint16 r5g5b5;
// B5G5R5 is used by the SNES system
typedef uint16 b5g5r5;

namespace ScriptInterface {
  struct ExceptionHandler {
    void exceptionCallback(asIScriptContext *ctx) {
      asIScriptEngine *engine = ctx->GetEngine();

      // Determine the exception that occurred
      const asIScriptFunction *function = ctx->GetExceptionFunction();
      platform->scriptMessage(
        string("EXCEPTION `{0}` occurred in `{1}` (line {2})").format({
          ctx->GetExceptionString(),
          function->GetDeclaration(),
          ctx->GetExceptionLineNumber()
        }),
        true
      );

      // Determine the function where the exception occurred
      //printf("func: %s\n", function->GetDeclaration());
      //printf("modl: %s\n", function->GetModuleName());
      //printf("sect: %s\n", function->GetScriptSectionName());
      // Determine the line number where the exception occurred
      //printf("line: %d\n", ctx->GetExceptionLineNumber());
    }
  } exceptionHandler;

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

  static void uint8_array_append_uint16(CScriptArray *array, const uint16 *value) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertLast((void *)value);
      return;
    }

    array->InsertLast((void *)&((const uint8 *)value)[0]);
    array->InsertLast((void *)&((const uint8 *)value)[1]);
  }

  static void uint8_array_append_uint32(CScriptArray *array, const uint32 *value) {
    if (array->GetElementTypeId() != asTYPEID_UINT8) {
      array->InsertLast((void *)value);
      return;
    }

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
      return;
    } else if (other->GetElementTypeId() == asTYPEID_UINT16) {
      for (uint i = 0; i < other->GetSize(); i++) {
        auto value = (const uint16 *) other->At(i);
        array->InsertLast((void *) &((const uint8 *) value)[0]);
        array->InsertLast((void *) &((const uint8 *) value)[1]);
      }
    } else if (other->GetElementTypeId() == asTYPEID_UINT32) {
      for (uint i = 0; i < other->GetSize(); i++) {
        auto value = (const uint32 *) other->At(i);
        array->InsertLast((void *) &((const uint8 *) value)[0]);
        array->InsertLast((void *) &((const uint8 *) value)[1]);
        array->InsertLast((void *) &((const uint8 *) value)[2]);
        array->InsertLast((void *) &((const uint8 *) value)[3]);
      }
    }
  }

  #include "script-bus.cpp"
  #include "script-ppu.cpp"
  #include "script-frame.cpp"
  #include "script-extra.cpp"
  #include "script-net.cpp"
  #include "script-gui.cpp"

};

auto Interface::registerScriptDefs() -> void {
  int r;

  script.engine = platform->scriptEngine();

  // register global functions for the script to use:
  auto defaultNamespace = script.engine->GetDefaultNamespace();

  // register array type:
  RegisterScriptArray(script.engine, true /* bool defaultArrayType */);

  // register string type:
  registerScriptString();

  // additional array functions for serialization purposes:
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const uint16 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint16), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const uint32 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint32), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const string &in other)", asFUNCTION(ScriptInterface::uint8_array_append_string), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void insertLast(const ? &in other)", asFUNCTION(ScriptInterface::uint8_array_append_array), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  r = script.engine->RegisterObjectMethod("array<T>", "string &toString(uint offs, uint size) const", asFUNCTION(ScriptInterface::array_to_string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

  // global function to write debug messages:
  r = script.engine->RegisterGlobalFunction("void message(const string &in msg)", asFUNCTION(ScriptInterface::message), asCALL_CDECL); assert(r >= 0);

  ScriptInterface::RegisterBus(script.engine);

  {
    // Order here is important as RegisterPPU sets namespace to 'ppu' and the following functions expect that.
    ScriptInterface::RegisterPPU(script.engine);
    ScriptInterface::RegisterPPUFrame(script.engine);
    ScriptInterface::RegisterPPUExtra(script.engine);
  }

  ScriptInterface::RegisterNet(script.engine);
  ScriptInterface::RegisterGUI(script.engine);

  r = script.engine->SetDefaultNamespace(defaultNamespace); assert(r >= 0);

  // create context:
  script.context = script.engine->CreateContext();

  script.context->SetExceptionCallback(asMETHOD(ScriptInterface::ExceptionHandler, exceptionCallback), &ScriptInterface::exceptionHandler, asCALL_THISCALL);
}

auto Interface::loadScript(string location) -> void {
  int r;

  if (!inode::exists(location)) return;

  if (script.module) {
    unloadScript();
  }

  // load script from file:
  string scriptSource = string::read(location);

  // add script into module:
  script.module = script.engine->GetModule(nullptr, asGM_ALWAYS_CREATE);
  r = script.module->AddScriptSection("script", scriptSource.begin(), scriptSource.length());
  assert(r >= 0);

  // compile module:
  r = script.module->Build();
  assert(r >= 0);

  // bind to functions in the script:
  script.funcs.init = script.module->GetFunctionByDecl("void init()");
  script.funcs.pre_nmi = script.module->GetFunctionByDecl("void pre_nmi()");
  script.funcs.pre_frame = script.module->GetFunctionByDecl("void pre_frame()");
  script.funcs.post_frame = script.module->GetFunctionByDecl("void post_frame()");

  if (script.funcs.init) {
    script.context->Prepare(script.funcs.init);
    script.context->Execute();
  }
}

auto Interface::unloadScript() -> void {
  // free any references to script callbacks:
  ::SuperFamicom::bus.reset_interceptors();
  ::SuperFamicom::cpu.reset_dma_interceptor();
  // TODO: GUI callbacks

  if (script.module) {
    script.module->Discard();
    script.module = nullptr;
  }

  script.funcs.init = nullptr;
  script.funcs.pre_frame = nullptr;
  script.funcs.post_frame = nullptr;

  // reset extra-tile data:
  ScriptInterface::extraLayer.reset();
}
