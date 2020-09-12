#pragma once

#include <libco/libco.h>

#include <nall/platform.hpp>
#include <nall/adaptive-array.hpp>
#include <nall/any.hpp>
#include <nall/chrono.hpp>
#include <nall/dl.hpp>
#include <nall/endian.hpp>
#include <nall/image.hpp>
#include <nall/literals.hpp>
#include <nall/map.hpp>
#include <nall/random.hpp>
#include <nall/serializer.hpp>
#include <nall/shared-pointer.hpp>
#include <nall/string.hpp>
#include <nall/traits.hpp>
#include <nall/unique-pointer.hpp>
#include <nall/vector.hpp>
#include <nall/vfs.hpp>
#include <nall/hash/crc32.hpp>
#include <nall/hash/sha256.hpp>
#include <nall/thread-pool.hpp>
using namespace nall;

// [jsd] add support for AngelScript
#include <angelscript.h>
#include <scriptarray.h>

namespace Script {

// the interface that the script host implements:
struct Platform {
public:
  // functions the interface calls:
  virtual auto scriptEngine() -> asIScriptEngine* { return scriptEngineState.engine; }
  virtual auto scriptMessage(const string& msg, bool alert = false) -> void { printf("script: %.*s\n", msg.size(), msg.data()); }

public:
  // functions the host calls:
  auto scriptCreateEngine() -> void;

  struct ScriptEngineState {
    asIScriptEngine *engine = nullptr;
  } scriptEngineState;
};

// the interface that sfc implements:
struct Interface {

  //virtual auto registerScriptDefs(Platform *platform) -> void {}
  //
  //virtual auto onModuleLoaded(asIScriptModule *module) -> void {}
  //virtual auto onModuleUnloaded(asIScriptModule *module) -> void {}

};

}