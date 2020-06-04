
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
    auto getStackTrace(asIScriptContext *ctx) -> vector<string> {
      vector<string> frames;

      for (asUINT n = 1; n < ctx->GetCallstackSize(); n++) {
        asIScriptFunction *func;
        const char *scriptSection;
        int line, column;

        func = ctx->GetFunction(n);
        line = ctx->GetLineNumber(n, &column, &scriptSection);

        frames.append(string("  in `{0}` at {1}:{2}:{3}").format({
          func->GetDeclaration(),
          scriptSection,
          line,
          column
        }));
      }

      return frames;
    }

    void exceptionCallback(asIScriptContext *ctx) {
      asIScriptEngine *engine = ctx->GetEngine();

      // Determine the exception that occurred
      const asIScriptFunction *function = ctx->GetExceptionFunction();
      const char *scriptSection;
      int line, column;
      line = ctx->GetExceptionLineNumber(&column, &scriptSection);

      // format main message:
      auto message = string("EXCEPTION `{0}` occurred in `{1}` at {2}:{3}:{4}\n")
        .format({
          ctx->GetExceptionString(),
          function->GetDeclaration(),
          scriptSection,
          line,
          column
        });

      // append stack trace:
      message.append(getStackTrace(ctx).merge("\n"));

      platform->scriptMessage(
        message,
        true
      );
    }
  } exceptionHandler;

  struct Profiler {
    struct node_t {
      // key:
      const char *section;
      int line;
      // value:
      uint64 samples;

      node_t() = default;
      node_t(const char *section, int line) : section(section), line(line) {}
      node_t(const char *section, int line, uint64 samples) : section(section), line(line), samples(samples) {}

      auto operator< (const node_t& source) const -> bool {
        int c = strcmp(section, source.section);
        if (c < 0) return true;
        if (c > 0) return false;
        return line <  source.line;
      }
      auto operator==(const node_t& source) const -> bool {
        int c = strcmp(section, source.section);
        if (c != 0) return false;
        return line == source.line;
      }
    };

    // red-black tree acting as a map storing sample counts by file:line key:
    set< node_t > sectionLineSamples;
    uint64 last_time = 0;
    uint64 last_save = 0;
    nall::thread thrProfiler;
    asIScriptContext *context;
    volatile bool enabled = false;

    auto samplingThread(uintptr p) -> void {
      while (enabled) {
        // sleep for 1ms
        usleep(1'009);

        // wake up and sample script location:
        sampleLocation();

        // auto-save every 5 seconds:
        auto time = chrono::microsecond();
        if (time - last_save >= 5'000'000) {
          save();
          last_save = time;
        }
      }
    }

    auto enable(asIScriptContext *ctx) -> void {
      context = ctx;
      enabled = true;
      thrProfiler = nall::thread::create({&Profiler::samplingThread, this});
    }

    auto disable(asIScriptContext *ctx) -> void {
      context = nullptr;
      enabled = false;
    }

    void reset() {
      sectionLineSamples.reset();
    }

    void save() {
      auto fb = file_buffer({"perf-",getpid(),".csv"}, file_buffer::mode::write);
      fb.truncate(0);
      fb.writes({"section,line,samples\n"});
      for (auto &node : sectionLineSamples) {
        fb.writes({node.section, ",", node.line, ",", node.samples, "\n"});
      }
    }

    auto sampleLocation() -> void {
      // sample where we are:
      const char *scriptSection;
      int line, column;
      line = context->GetLineNumber(0, &column, &scriptSection);

      if (scriptSection == nullptr) {
        scriptSection = "";
      }

      // increment sample counter for the section/line:
      auto node = sectionLineSamples.find({scriptSection, line});
      if (!node) {
        sectionLineSamples.insert({scriptSection, line, 1});
      } else {
        node->samples++;
      }
    }
  } profiler;

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

  #include "script-bus.cpp"
  #include "script-ppu.cpp"
  #include "script-frame.cpp"
  #include "script-extra.cpp"
  #include "script-net.cpp"
  #include "script-gui.cpp"

};

auto Interface::paletteUpdated(uint32_t *palette, uint depth) -> void {
  //printf("Interface::paletteUpdated(%p, %d)\n", palette, depth);
  ScriptInterface::emulatorPalette = palette;
  ScriptInterface::emulatorDepth = depth;

  if (script.funcs.palette_updated) {
    script.context->Prepare(script.funcs.palette_updated);
    script.context->Execute();
  }
}

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
  r = script.engine->RegisterObjectMethod("array<T>", "void write_u8(const uint8 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint8), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void write_u16(const uint16 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint16), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void write_u24(const uint32 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint24), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void write_u32(const uint32 &in value)", asFUNCTION(ScriptInterface::uint8_array_append_uint32), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void write_arr(const ? &in array)", asFUNCTION(ScriptInterface::uint8_array_append_array), asCALL_CDECL_OBJFIRST); assert(r >= 0);
  r = script.engine->RegisterObjectMethod("array<T>", "void write_str(const string &in other)", asFUNCTION(ScriptInterface::uint8_array_append_string), asCALL_CDECL_OBJFIRST); assert(r >= 0);

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

#if defined(AS_ENABLE_PROFILER)
  ScriptInterface::profiler.enable(script.context);
#endif
}

auto Interface::loadScript(string location) -> void {
  int r;

  if (!inode::exists(location)) return;

  if (script.modules) {
    unloadScript();
  }

  // create a main module:
  script.main_module = script.engine->GetModule("main", asGM_ALWAYS_CREATE);

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
      r = script.main_module->AddScriptSection(filename, scriptSource.begin(), scriptSource.length());
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
    r = script.main_module->AddScriptSection(Location::file(location), scriptSource.begin(), scriptSource.length());
    assert(r >= 0);
  }

  // compile module:
  r = script.main_module->Build();
  assert(r >= 0);

  // track main module:
  script.modules.append(script.main_module);

  // bind to functions in the main module:
  script.funcs.init = script.main_module->GetFunctionByDecl("void init()");
  script.funcs.unload = script.main_module->GetFunctionByDecl("void unload()");
  script.funcs.post_power = script.main_module->GetFunctionByDecl("void post_power(bool reset)");
  script.funcs.cartridge_loaded = script.main_module->GetFunctionByDecl("void cartridge_loaded()");
  script.funcs.cartridge_unloaded = script.main_module->GetFunctionByDecl("void cartridge_unloaded()");
  script.funcs.pre_nmi = script.main_module->GetFunctionByDecl("void pre_nmi()");
  script.funcs.pre_frame = script.main_module->GetFunctionByDecl("void pre_frame()");
  script.funcs.post_frame = script.main_module->GetFunctionByDecl("void post_frame()");
  script.funcs.palette_updated = script.main_module->GetFunctionByDecl("void palette_updated()");

  if (script.funcs.init) {
    script.context->Prepare(script.funcs.init);
    script.context->Execute();
  }
  if (loaded()) {
    if (script.funcs.cartridge_loaded) {
      script.context->Prepare(script.funcs.cartridge_loaded);
      script.context->Execute();
    }
    if (script.funcs.post_power) {
      script.context->Prepare(script.funcs.post_power);
      script.context->SetArgByte(0, false); // reset = false
      script.context->Execute();
    }
  }
}

auto Interface::unloadScript() -> void {
  // free any references to script callbacks:
  ::SuperFamicom::bus.reset_interceptors();
  ::SuperFamicom::cpu.reset_dma_interceptor();
  ::SuperFamicom::cpu.reset_pc_callbacks();

  // Close any GUI windows:
  for (auto window : script.windows) {
    if (!window) continue;
    window->setVisible(false);
    window->setDismissable(true);
    window->destruct();
    window.reset();
  }
  script.windows.reset();

  // unload script:
  if (script.funcs.unload) {
    script.context->Prepare(script.funcs.unload);
    script.context->Execute();
  }

  // discard all loaded modules:
  for (auto module : script.modules) {
    module->Discard();
  }
  script.modules.reset();
  script.main_module = nullptr;

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
