#pragma once

#ifndef DISABLE_HIRO
#  include <hiro/hiro.hpp>
#endif

namespace Emulator {

struct Platform : Script::Platform {
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
  virtual auto presentationWindow() -> hiro::Window { return {}; }
#endif

  // scripting for libretro:
  virtual auto registerMenu(const string& menuName, const string& display, const string& desc) -> void {};
  virtual auto registerMenuOption(const string& menuName, const string& key, const string& desc, const string& info, const vector<string>& values) -> void {};
  virtual auto setMenuOption(const string& menuName, const string& key, const string& value) -> void {};
  virtual auto getMenuOption(const string& menuName, const string& key) -> string { return {}; };
};

extern Platform* platform;

}
