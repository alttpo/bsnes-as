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
  virtual auto scriptEngine() -> asIScriptEngine*;
  virtual auto scriptPrimaryContext() -> asIScriptContext*;
  virtual auto scriptMessage(const string& msg, bool alert = false) -> void;
  virtual auto scriptCreateContext() -> asIScriptContext*;

public:
  // functions the host calls:
  virtual auto scriptCreateEngine() -> void;
  virtual auto scriptCreatePrimaryContext() -> void;

  struct ScriptEngineState {
    asIScriptEngine   *engine   = nullptr;
    asIScriptContext  *context  = nullptr;
  } scriptEngineState;

private:
  // private functions used for script callbacks:
  auto getStackTrace(asIScriptContext *ctx) -> vector<string>;
  auto exceptionCallback(asIScriptContext *ctx) -> void;
};

// the interface that sfc implements:
struct Interface {

  virtual auto registerScriptDefs(Platform *platform) -> void {}

};

}