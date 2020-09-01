#pragma once

#ifndef DISABLE_HIRO
#  include <hiro/hiro.hpp>
#endif

namespace Emulator {

struct Platform {
  struct Load {
    Load() = default;
    Load(uint pathID, string option = "") : valid(true), pathID(pathID), option(option) {}
    explicit operator bool() const { return valid; }

    bool valid = false;
    uint pathID = 0;
    string option;
  };

  virtual auto path(uint id) -> string { return ""; }
  virtual auto open(uint id, string name, vfs::file::mode mode, bool required = false) -> shared_pointer<vfs::file> { return {}; }
  virtual auto load(uint id, string name, string type, vector<string> options = {}) -> Load { return {}; }
  virtual auto videoFrame(const uint16* data, uint pitch, uint width, uint height, uint scale) -> void {}
  virtual auto audioFrame(const double* samples, uint channels) -> void {}
  virtual auto inputPoll(uint port, uint device, uint input) -> int16 { return 0; }
  virtual auto inputRumble(uint port, uint device, uint input, bool enable) -> void {}
  virtual auto dipSettings(Markup::Node node) -> uint { return 0; }
  virtual auto notify(string text) -> void {}

#ifndef DISABLE_HIRO
  virtual auto presentationWindow() -> hiro::Window { return {}; };
#endif
  virtual auto scriptEngine() -> asIScriptEngine* { return nullptr; };
  virtual auto scriptMessage(const string& msg, bool alert = false) -> void { printf("script: %.*s\n", msg.size(), msg.data()); };
};

extern Platform* platform;

}
