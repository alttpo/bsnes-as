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
#include <nall/thread.hpp>
#include <nall/thread-pool.hpp>
using namespace nall;

// [jsd] add support for AngelScript
#include <angelscript.h>
#include <scriptarray.h>

// types:
using uint64 = uint64_t;

namespace Script {

struct Profiler {
  ~Profiler();

  struct node_t {
    // key:
    string section;
    int line;
    // value:
    uint64 samples;

    node_t() = default;
    node_t(const string &section, int line);
    node_t(const string &section, int line, uint64 samples);

    auto operator< (const node_t& source) const -> bool;
    auto operator==(const node_t& source) const -> bool;
  };

  auto samplingThread(uintptr p) -> void;

  auto lineCallback(asIScriptContext *ctx) -> void;
  auto enable(asIScriptContext *ctx) -> void;
  auto disable(asIScriptContext *ctx) -> void;
  void reset();
  void save();

  auto sampleLocation(asIScriptContext *ctx) -> void;
  auto resetLocation() -> void;
  auto recordSample() -> void;

public:
  // red-black tree acting as a map storing sample counts by file:line key:
  set< node_t > sectionLineSamples;
  uint64 last_time = 0;
  uint64 last_save = 0;
  ::nall::thread thrProfiler;
  volatile bool enabled = false;

  string scriptSection = "<not in script>";
  int line, column;
  std::mutex mtx;
};

// the interface that the script host implements:
struct Platform {
public:
  // functions the interface calls:
  virtual auto scriptEngine() -> asIScriptEngine*;
  virtual auto scriptPrimaryContext() -> asIScriptContext*;
  virtual auto scriptMessage(const string& msg, bool alert = false) -> void;
  virtual auto scriptCreateContext() -> asIScriptContext*;

  virtual auto scriptInvokeFunction(asIScriptFunction *func, function<void (asIScriptContext*)> prepareArgs = {}) -> asUINT;
  virtual auto scriptExecute(asIScriptContext *ctx) -> asUINT;

public:
  // functions the host calls:
  virtual auto scriptCreateEngine() -> void;
  virtual auto scriptCreatePrimaryContext() -> void;

  virtual auto scriptProfilerEnable(asIScriptContext*) -> void;
  virtual auto scriptProfilerDisable(asIScriptContext*) -> void;

  struct ScriptEngineState {
    asIScriptEngine   *engine   = nullptr;
    asIScriptContext  *context  = nullptr;

    Profiler profiler;
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