// bsnes-headless: the bsnes core and its AngelScript host with no GUI, audio or video output, for testing scripts.
//
// It compiles a script (a .as file, or a directory whose *.as files make one module) and either stops there
// (--compile-only) or loads a ROM, runs the script against it, and takes commands on stdin, one per line, answering
// each on stdout. See target-headless/README.md for the command reference.

#include <emulator/emulator.hpp>
#include <sfc/sfc.hpp>
#include <lzma/lzma.hpp>
#include <nall/directory.hpp>
#include <nall/decode/zip.hpp>
#include <nall/encode/bmp.hpp>
using namespace nall;

#include <heuristics/heuristics.hpp>
#include <heuristics/heuristics.cpp>
#include <heuristics/super-famicom.cpp>
#include <heuristics/game-boy.cpp>

#include <cstdio>
#include <iostream>

// the embedded boards database and SPC700 IPL ROM, shared with the libretro core:
#include "../target-libretro/resources.hpp"

namespace Headless {

enum ExitCode : int {
  ExitOK = 0,
  ExitScriptError = 1,
  ExitUsage = 2,
  ExitLoadError = 3,
};

static SuperFamicom::Interface* emulator = nullptr;

struct Program : Emulator::Platform {
  Program() { platform = this; }  // sfc.hpp #defines platform as Emulator::platform

  // Emulator::Platform:
  auto open(uint id, string name, vfs::file::mode mode, bool required) -> shared_pointer<vfs::file> override;
  auto load(uint id, string name, string type, vector<string> options = {}) -> Emulator::Platform::Load override;
  auto videoFrame(const uint16* data, uint pitch, uint width, uint height, uint scale) -> void override;
  auto audioFrame(const double* samples, uint channels) -> void override {}
  auto inputPoll(uint port, uint device, uint input) -> int16 override;
  auto inputRumble(uint port, uint device, uint input, bool enable) -> void override {}
  auto userSettingsPath() -> string override { return settingsDirectory; }

  // Script::Platform:
  auto scriptMessage(const string& msg, bool alert = false, ::Script::MessageLevel level = ::Script::MSG_INFO) -> void override;
  auto scriptMessageCallback(const asSMessageInfo* msg) -> void override;
  auto exceptionCallback(asIScriptContext* ctx) -> void override;
  auto formatStackFrame(const char* section, int line, int column, const asIScriptFunction* func = nullptr) -> string override;
  auto sectionPath(const char* section) -> string;

  auto loadGame(string location) -> bool;
  auto loadFile(string location) -> vector<uint8_t>;
  auto power() -> void;

  auto setScriptLocation(string location) -> bool;
  auto compileScript() -> bool;
  auto loadScript() -> bool;

  auto runFrame() -> bool;
  auto writeScreenshot(string location) -> bool;

  auto command(string line) -> bool;  // returns false on quit
  auto reply(const string& text) -> void;
  auto fail(const string& text) -> void;

  struct Game {
    string location;
    string title;
    string region;
    string manifest;
    vector<uint8_t> program;
    vector<uint8_t> data;
    vector<uint8_t> expansion;
    vector<uint8_t> firmware;
  } game;

  string sramLocation;       // initial SRAM contents; never written back
  string settingsDirectory;  // where scripts' UserSettings live; never the GUI's folder unless asked for
  string scriptLocation;     // file, or directory ending in '/'
  string scriptDirectory;    // prefixed to section names in diagnostics

  bool compileOnly = false;

  // held buttons per controller port, a bit per SuperFamicom::Gamepad input (Up=0 ... Start=11):
  uint16 buttons[2] = {0, 0};

  uint64 frame = 0;          // frames completed since power on
  bool frameDone = false;

  struct Screen {
    const uint16* data = nullptr;
    uint pitch = 0;          // in bytes
    uint width = 0;
    uint height = 0;
  } screen;
  uint32_t palette[32768];

  uint errors = 0;           // compiler errors
  uint warnings = 0;         // compiler warnings
  uint exceptions = 0;       // script exceptions while running
};

static Program* program = nullptr;

static const char* buttonNames[12] = {"up", "down", "left", "right", "b", "a", "y", "x", "l", "r", "select", "start"};

// decimal, or hex with a '$' or '0x' prefix:
static auto parseNumber(string text, uint64_t& value) -> bool {
  text.strip();
  if (!text) return false;
  const char* digits = text.data();
  uint base = 10;
  if (text.beginsWith("$")) { digits += 1; base = 16; }
  else if (text.ibeginsWith("0x")) { digits += 2; base = 16; }
  if (!*digits) return false;
  char* end = nullptr;
  value = strtoull(digits, &end, base);
  return end && *end == 0;
}

static auto levelName(::Script::MessageLevel level) -> const char* {
  switch (level) {
    case ::Script::MSG_DEBUG: return "debug";
    case ::Script::MSG_INFO:  return "info";
    case ::Script::MSG_WARN:  return "warn";
    case ::Script::MSG_ERROR: return "error";
  }
  return "info";
}

auto Program::open(uint id, string name, vfs::file::mode mode, bool required) -> shared_pointer<vfs::file> {
  if (mode != vfs::file::mode::read) return {};  // nothing is ever saved

  if (name == "ipl.rom") return vfs::memory::file::open(iplrom, sizeof(iplrom));
  if (name == "boards.bml") return vfs::memory::file::open(Boards, sizeof(Boards));

  if (id == 1) {  // Super Famicom
    if (name == "manifest.bml") return vfs::memory::file::open(game.manifest.data<uint8_t>(), game.manifest.size());
    if (name == "program.rom") return vfs::memory::file::open(game.program.data(), game.program.size());
    if (name == "data.rom") return vfs::memory::file::open(game.data.data(), game.data.size());
    if (name == "expansion.rom") return vfs::memory::file::open(game.expansion.data(), game.expansion.size());
    if (name == "save.ram" && sramLocation) return vfs::fs::file::open(sramLocation, mode);
  }
  return {};
}

auto Program::load(uint id, string name, string type, vector<string> options) -> Emulator::Platform::Load {
  if (id == 1 && game.location) return {id, game.region};
  return {};
}

auto Program::videoFrame(const uint16* data, uint pitch, uint width, uint height, uint scale) -> void {
  // the core calls this once at the end of every frame, after the script's post_frame():
  screen = {data, pitch, width, height};
  frame++;
  frameDone = true;
}

auto Program::inputPoll(uint port, uint device, uint input) -> int16 {
  if (device != SuperFamicom::ID::Device::Gamepad) return 0;
  if (port > 1 || input >= 12) return 0;
  return (buttons[port] >> input) & 1;
}

auto Program::scriptMessage(const string& msg, bool alert, ::Script::MessageLevel level) -> void {
  // one output line per message line, so a client can read the stream line by line:
  for (auto& line : msg.split("\n")) {
    if (compileOnly) {
      printf("%.*s\n", line.size(), line.data());
    } else {
      printf("log %llu %s %.*s\n", (unsigned long long)frame, levelName(level), line.size(), line.data());
    }
  }
  fflush(stdout);
}

// compiler diagnostics as `path:line:col: error: message`, the form editors and terminals link to the source:
auto Program::scriptMessageCallback(const asSMessageInfo* msg) -> void {
  auto level = ::Script::convertMessageLevel(msg->type);
  const char* kind = "info";
  if (msg->type == asMSGTYPE_ERROR) { kind = "error"; errors++; }
  else if (msg->type == asMSGTYPE_WARNING) { kind = "warning"; warnings++; }

  scriptMessage(string{sectionPath(msg->section), ":", msg->row, ":", msg->col, ": ", kind, ": ", msg->message}, false, level);
}

// script sections are named by file name alone; give them the script's directory:
auto Program::sectionPath(const char* section) -> string {
  string path = section ? section : "";
  if (path && !path.beginsWith("/") && !path.beginsWith("<") && scriptDirectory) path.prepend(scriptDirectory);
  return path;
}

auto Program::formatStackFrame(const char* section, int line, int column, const asIScriptFunction* func) -> string {
  string frame = {sectionPath(section), ":", line, ":", column};
  if (func) frame.append(" ", func->GetDeclaration());
  return frame;
}

auto Program::exceptionCallback(asIScriptContext* ctx) -> void {
  exceptions++;

  vector<string> frames;
  const char* section = nullptr;
  int column = 0;
  int line = ctx->GetExceptionLineNumber(&column, &section);
  frames.append(formatStackFrame(section, line, column, ctx->GetExceptionFunction()));
  getStackTrace(ctx, frames);

  scriptMessage({"exception: ", ctx->GetExceptionString(), "\n  ", frames.merge("\n  ")}, false, ::Script::MSG_ERROR);
}

auto Program::loadFile(string location) -> vector<uint8_t> {
  auto suffix = Location::suffix(location).downcase();
  if (suffix == ".zip") {
    Decode::ZIP archive;
    if (archive.open(location)) {
      for (auto& file : archive.file) {
        auto type = Location::suffix(file.name).downcase();
        if (type == ".sfc" || type == ".smc") return archive.extract(file);
      }
    }
    return {};
  }
  if (suffix == ".7z") return LZMA::extract(location);
  return file::read(location);
}

auto Program::loadGame(string location) -> bool {
  auto rom = loadFile(location);
  if (rom.size() < 0x8000) return false;

  if ((rom.size() & 0x7fff) == 512) {
    // remove copier header:
    memory::move(&rom[0], &rom[512], rom.size() - 512);
    rom.resize(rom.size() - 512);
  }

  auto heuristics = Heuristics::SuperFamicom(rom, location);
  game.title = heuristics.title();
  game.region = heuristics.videoRegion();
  game.manifest = heuristics.manifest();

  // prefer the game database's manifest like the GUI does, when the database is found next to the binary:
  auto sha256 = Hash::SHA256(rom).digest();
  for (auto& candidate : vector<string>{
    {Path::program(), "Database/Super Famicom.bml"},
    {Path::program(), "../Database/Super Famicom.bml"},
  }) {
    if (!file::exists(candidate)) continue;
    if (auto document = BML::unserialize(string::read(candidate))) {
      if (auto entry = document[{"game(sha256=", sha256, ")"}]) {
        game.manifest = BML::serialize(entry);
        game.manifest.append("  title: ", game.title, "\n");
      }
    }
    break;
  }

  game.location = location;

  uint offset = 0;
  auto take = [&](vector<uint8_t>& target, uint size) {
    if (!size) return;
    target.resize(size);
    memory::copy(&target[0], &rom[offset], size);
    offset += size;
  };
  take(game.program, heuristics.programRomSize());
  take(game.data, heuristics.dataRomSize());
  take(game.expansion, heuristics.expansionRomSize());
  take(game.firmware, heuristics.firmwareRomSize());

  emulator->configure("Audio/Frequency", 48000.0);
  emulator->configure("Video/ColorEmulation", false);
  if (!emulator->load()) return false;
  emulator->connect(SuperFamicom::ID::Port::Controller1, SuperFamicom::ID::Device::Gamepad);
  emulator->connect(SuperFamicom::ID::Port::Controller2, SuperFamicom::ID::Device::Gamepad);
  power();
  return true;
}

auto Program::power() -> void {
  emulator->power();
  frame = 0;
  screen = {};
}

auto Program::setScriptLocation(string location) -> bool {
  if (!inode::exists(location)) return false;
  if (!location.beginsWith("/")) location.prepend(Path::active());
  if (directory::exists(location)) {
    if (!location.endsWith("/")) location.append("/");
    scriptDirectory = location;
  } else {
    scriptDirectory = Location::path(location);
  }
  scriptLocation = location;
  return true;
}

auto Program::compileScript() -> bool {
  errors = 0;
  warnings = 0;
  return emulator->compileScript(scriptLocation) && errors == 0;
}

auto Program::loadScript() -> bool {
  errors = 0;
  warnings = 0;
  emulator->loadScript(scriptLocation);
  return scriptEngineState.main_module != nullptr && errors == 0;
}

auto Program::runFrame() -> bool {
  frameDone = false;
  // System::run() returns at each scheduler event (pre-NMI, frame start, frame end), so run until the frame is out.
  // The bound only guards against a core that never finishes a frame:
  for (uint n = 0; n < 1000 && !frameDone; n++) emulator->run();
  return frameDone;
}

auto Program::writeScreenshot(string location) -> bool {
  if (!screen.data) return false;

  // the PPU writes 15-bit colors (R at bits 10-14) at 256 or 512 wide and 240 or 480 high, the top and bottom 8
  // scanlines (per 240) being overscan:
  uint multiplier = screen.height / 240;
  uint pitch = screen.pitch >> 1;
  auto data = screen.data + 8 * multiplier * pitch;
  uint height = screen.height - 16 * multiplier;
  uint width = screen.width;

  vector<uint32_t> pixels;
  pixels.resize(width * height);
  for (uint y : range(height)) {
    for (uint x : range(width)) {
      pixels[y * width + x] = palette[data[y * pitch + x] & 0x7fff];
    }
  }
  return Encode::BMP::create(location, pixels.data(), width * sizeof(uint32_t), width, height, false);
}

auto Program::reply(const string& text) -> void {
  if (text) printf("ok %.*s\n", text.size(), text.data());
  else printf("ok\n");
  fflush(stdout);
}

auto Program::fail(const string& text) -> void {
  printf("err %.*s\n", text.size(), text.data());
  fflush(stdout);
}

auto Program::command(string line) -> bool {
  line.strip();
  if (!line || line.beginsWith("#")) return true;

  // the first word is the command; `rest` is the remainder of the line, untouched (for exec and call):
  string name = line;
  string rest;
  if (auto space = line.find(" ")) {
    name = slice(line, 0, *space);
    rest = slice(line, *space + 1);
    rest.strip();
  }
  vector<string> args;
  for (auto& word : rest.split(" ")) {
    if (word.strip()) args.append(word);
  }

  auto needGame = [&]() -> bool {
    if (emulator->loaded()) return true;
    fail("no ROM loaded");
    return false;
  };
  auto needScript = [&]() -> bool {
    if (scriptEngineState.main_module) return true;
    fail("no script loaded");
    return false;
  };

  if (name == "quit" || name == "exit") {
    reply("");
    return false;
  }

  if (name == "frame" || name == "frames") {
    if (!needGame()) return true;
    uint64_t count = 1;
    if (args && !parseNumber(args[0], count)) return fail("bad frame count"), true;
    for (uint64_t n = 0; n < count; n++) {
      if (!runFrame()) return fail({"frame ", frame, " did not finish"}), true;
    }
    reply({frame});
    return true;
  }

  if (name == "input") {
    if (args.size() < 1) return fail("usage: input <port 1|2> [button...|mask]"), true;
    uint64_t port = 0;
    if (!parseNumber(args[0], port) || port < 1 || port > 2) return fail("port must be 1 or 2"), true;
    uint16 mask = 0;
    for (uint i = 1; i < args.size(); i++) {
      auto word = args[i].downcase();
      uint64_t value = 0;
      if ((word.beginsWith("$") || word.beginsWith("0x")) && parseNumber(word, value)) {
        mask |= value & 0x0fff;
        continue;
      }
      bool found = false;
      for (uint b : range(12)) {
        if (word == buttonNames[b]) { mask |= 1 << b; found = true; }
      }
      if (!found) return fail({"unknown button '", args[i], "'"}), true;
    }
    buttons[port - 1] = mask;
    reply({"$", hex(mask, 3L)});
    return true;
  }

  if (name == "read") {
    if (!needGame()) return true;
    uint64_t addr = 0, size = 1;
    if (args.size() < 1 || !parseNumber(args[0], addr)) return fail("usage: read <addr> [size]"), true;
    if (args.size() >= 2 && !parseNumber(args[1], size)) return fail("bad size"), true;
    if (size > 0x10000 || addr + size > 0x1000000) return fail("read out of range"), true;
    string out;
    out.reserve(size * 2);
    for (uint64_t i = 0; i < size; i++) out.append(hex(SuperFamicom::bus.read(addr + i, 0), 2L));
    reply(out);
    return true;
  }

  if (name == "write") {
    if (!needGame()) return true;
    uint64_t addr = 0;
    if (args.size() < 2 || !parseNumber(args[0], addr)) return fail("usage: write <addr> <hex bytes>"), true;
    string bytes;
    for (uint i = 1; i < args.size(); i++) bytes.append(args[i]);
    if (bytes.size() & 1) return fail("odd number of hex digits"), true;
    if (addr + bytes.size() / 2 > 0x1000000) return fail("write out of range"), true;
    for (uint i = 0; i < bytes.size(); i += 2) {
      uint64_t value = 0;
      if (!parseNumber({"$", slice(bytes, i, 2)}, value)) return fail("bad hex byte"), true;
      SuperFamicom::bus.write(addr + i / 2, value);
    }
    reply("");
    return true;
  }

  if (name == "reset") {
    if (!needGame()) return true;
    emulator->reset();
    reply("");
    return true;
  }

  if (name == "power") {
    if (!needGame()) return true;
    power();
    reply("");
    return true;
  }

  if (name == "save-state") {
    if (!needGame()) return true;
    if (!rest) return fail("usage: save-state <file>"), true;
    auto state = emulator->serialize();
    if (!file::write(rest, {state.data(), state.size()})) return fail({"cannot write ", rest}), true;
    reply({state.size()});
    return true;
  }

  if (name == "load-state") {
    if (!needGame()) return true;
    if (!rest) return fail("usage: load-state <file>"), true;
    auto data = file::read(rest);
    if (!data) return fail({"cannot read ", rest}), true;
    serializer state(data.data(), data.size());
    if (!emulator->unserialize(state)) return fail("state does not match this ROM or core"), true;
    reply("");
    return true;
  }

  if (name == "screenshot") {
    if (!rest) return fail("usage: screenshot <file.bmp>"), true;
    if (!writeScreenshot(rest)) return fail("no frame to capture yet, or cannot write the file"), true;
    reply("");
    return true;
  }

  if (name == "script-load" || name == "script-reload") {
    if (name == "script-load") {
      if (!rest) return fail("usage: script-load <file.as|directory>"), true;
      if (!setScriptLocation(rest)) return fail({"not found: ", rest}), true;
    }
    if (!scriptLocation) return fail("no script location"), true;
    if (!loadScript()) return fail({"script failed to compile: ", errors, " error(s)"}), true;
    reply({warnings, " warning(s)"});
    return true;
  }

  if (name == "script-unload") {
    emulator->unloadScript();
    reply("");
    return true;
  }

  if (name == "call") {
    // call a script function by its declaration, e.g. `call int count_players()`:
    if (!needScript()) return true;
    if (!rest) return fail("usage: call <function declaration>"), true;
    auto module = scriptEngineState.main_module;
    auto func = module->GetFunctionByDecl(rest);
    if (!func) func = module->GetFunctionByName(rest);
    if (!func) return fail({"no function '", rest, "' in the script"}), true;
    if (func->GetParamCount() != 0) return fail("only functions without parameters can be called"), true;

    auto ctx = scriptPrimaryContext();
    uint before = exceptions;
    auto r = scriptInvokeFunction(func);
    if (r != asEXECUTION_FINISHED || exceptions != before) return fail("script exception"), true;

    auto engine = scriptEngine();
    int typeId = func->GetReturnTypeId();
    string value;
    switch (typeId) {
      case asTYPEID_VOID:   break;
      case asTYPEID_BOOL:   value = ctx->GetReturnByte() ? "true" : "false"; break;
      case asTYPEID_INT8:   value = (int8_t)ctx->GetReturnByte(); break;
      case asTYPEID_INT16:  value = (int16_t)ctx->GetReturnWord(); break;
      case asTYPEID_INT32:  value = (int32_t)ctx->GetReturnDWord(); break;
      case asTYPEID_INT64:  value = (int64_t)ctx->GetReturnQWord(); break;
      case asTYPEID_UINT8:  value = (uint8_t)ctx->GetReturnByte(); break;
      case asTYPEID_UINT16: value = (uint16_t)ctx->GetReturnWord(); break;
      case asTYPEID_UINT32: value = (uint32_t)ctx->GetReturnDWord(); break;
      case asTYPEID_UINT64: value = (uint64_t)ctx->GetReturnQWord(); break;
      case asTYPEID_FLOAT:  value = ctx->GetReturnFloat(); break;
      case asTYPEID_DOUBLE: value = ctx->GetReturnDouble(); break;
      default:
        if (typeId == engine->GetTypeIdByDecl("string")) {
          if (auto s = (string*)ctx->GetReturnObject()) value = *s;
        } else {
          value = {"<", engine->GetTypeDeclaration(typeId), ">"};
        }
    }
    reply(value);
    return true;
  }

  if (name == "exec") {
    // compile and run statements in the script module's scope, e.g. `exec message(fmtHex(bus::read_u8(0x7e0010), 2))`:
    if (!needScript()) return true;
    if (!rest) return fail("usage: exec <statements>"), true;
    auto module = scriptEngineState.main_module;
    // the statements go on the source's second line, so a line offset of -1 reports them as line 1:
    string source = {"void __headless_exec() {\n", rest, ";\n}\n"};
    asIScriptFunction* func = nullptr;
    errors = 0;
    int r = module->CompileFunction("<exec>", source.data(), -1, 0, &func);
    if (r < 0 || !func) return fail("exec failed to compile"), true;
    uint before = exceptions;
    auto x = scriptInvokeFunction(func);
    func->Release();
    if (x != asEXECUTION_FINISHED || exceptions != before) return fail("script exception"), true;
    reply("");
    return true;
  }

  if (name == "status") {
    reply({
      "frame=", frame,
      " exceptions=", exceptions,
      " rom=", emulator->loaded() ? Location::file(game.location) : string{"none"},
      " script=", scriptEngineState.main_module ? scriptLocation : string{"none"}
    });
    return true;
  }

  fail({"unknown command '", name, "'"});
  return true;
}

static auto usage() -> void {
  fprintf(stderr,
    "usage: bsnes-headless [options] [rom.sfc]\n"
    "  --script=<path>    script: a .as file, or a directory whose *.as files form one module\n"
    "  --compile-only     compile the script, print diagnostics and exit (no ROM needed);\n"
    "                     exits 1 if it has errors\n"
    "  --sram=<file>      initial SRAM contents (the file is never written)\n"
    "  --frames=<n>       run n frames after loading, then exit instead of reading commands\n"
    "  --entropy=<none|low|high>  RAM initialization; default none so runs are repeatable\n"
    "  --accurate-ppu     use the cycle-accurate PPU instead of the fast one the GUI defaults to\n"
    "  --allow-opposite-directions  let up+down and left+right reach the game (snes9x movies use them)\n"
    "  --settings-dir=<dir>  folder for the script's UserSettings files (default: a new temporary folder,\n"
    "                     so the GUI's settings are never read or overwritten)\n"
    "\n"
    "Without --frames, commands are read from stdin one per line and each is answered on stdout\n"
    "with `ok [result]` or `err <reason>`; script output comes before it as `log <frame> <level> <text>`.\n"
    "Commands: frame [n], input <port> [buttons], read <addr> [size], write <addr> <hex>, reset, power,\n"
    "save-state <file>, load-state <file>, screenshot <file.bmp>, script-load <path>, script-reload,\n"
    "script-unload, call <decl>, exec <statements>, status, quit.\n"
  );
}

}

#include <nall/main.hpp>
auto nall::main(Arguments arguments) -> void {
  using namespace Headless;

  string romLocation;
  string scriptArg;
  string entropy = "None";
  bool accuratePPU = false;
  bool allowOpposite = false;
  string settingsDir;
  bool compileOnly = false;
  uint64_t runFrames = 0;
  bool haveRunFrames = false;
  string sram;

  for (auto argument : arguments) {
    if (argument == "-h" || argument == "--help") { usage(); exit(ExitOK); }
    else if (argument.beginsWith("--script=")) scriptArg = slice(argument, 9);
    else if (argument == "--compile-only") compileOnly = true;
    else if (argument.beginsWith("--sram=")) sram = slice(argument, 7);
    else if (argument.beginsWith("--frames=")) {
      if (!parseNumber(slice(argument, 9), runFrames)) { usage(); exit(ExitUsage); }
      haveRunFrames = true;
    }
    else if (argument.beginsWith("--entropy=")) {
      auto value = slice(argument, 10).downcase();
      if (value == "none") entropy = "None";
      else if (value == "low") entropy = "Low";
      else if (value == "high") entropy = "High";
      else { usage(); exit(ExitUsage); }
    }
    else if (argument == "--accurate-ppu") accuratePPU = true;
    else if (argument == "--allow-opposite-directions") allowOpposite = true;
    else if (argument.beginsWith("--settings-dir=")) settingsDir = slice(argument, 15);
    else if (argument.beginsWith("-")) { fprintf(stderr, "unknown option %s\n", argument.data()); usage(); exit(ExitUsage); }
    else if (!romLocation) romLocation = argument;
    else { usage(); exit(ExitUsage); }
  }

  if (compileOnly && !scriptArg) { fprintf(stderr, "--compile-only needs --script\n"); exit(ExitUsage); }
  if (!compileOnly && !romLocation) { usage(); exit(ExitUsage); }

  emulator = new SuperFamicom::Interface;
  program = new Program;
  program->compileOnly = compileOnly;

  if (!settingsDir) {
    char pattern[] = "/tmp/bsnes-headless-XXXXXX";
    if (auto dir = mkdtemp(pattern)) settingsDir = dir;
  }
  if (settingsDir && !settingsDir.endsWith("/")) settingsDir.append("/");
  directory::create(settingsDir);
  program->settingsDirectory = settingsDir;

  program->scriptCreateEngine();
  emulator->registerScriptDefs(program);
  program->scriptCreatePrimaryContext();

  // the same 24-bit palette the GUI hands the core, which scripts can read:
  for (uint color : range(32768)) {
    uint r = (color >> 10) & 31, g = (color >> 5) & 31, b = color & 31;
    r = r << 3 | r >> 2;
    g = g << 3 | g >> 2;
    b = b << 3 | b >> 2;
    program->palette[color] = r << 16 | g << 8 | b;
  }
  emulator->paletteUpdated(program->palette, 24);

  if (scriptArg && !program->setScriptLocation(scriptArg)) {
    fprintf(stderr, "script not found: %s\n", scriptArg.data());
    exit(ExitUsage);
  }

  if (compileOnly) {
    bool ok = program->compileScript();
    printf("%u error(s), %u warning(s)\n", program->errors, program->warnings);
    fflush(stdout);
    exit(ok ? ExitOK : ExitScriptError);
  }

  emulator->configure("Hacks/Entropy", entropy);
  emulator->configure("Hacks/PPU/Fast", !accuratePPU);
  emulator->configure("Controller/AllowOppositeDirections", allowOpposite);
  program->sramLocation = sram;
  if (!program->loadGame(romLocation)) {
    fprintf(stderr, "cannot load ROM: %s\n", romLocation.data());
    exit(ExitLoadError);
  }

  if (program->scriptLocation) {
    if (!program->loadScript()) {
      printf("err script failed to compile: %u error(s)\n", program->errors);
      fflush(stdout);
      exit(ExitScriptError);
    }
  }

  if (haveRunFrames) {
    for (uint64_t n = 0; n < runFrames; n++) {
      if (!program->runFrame()) break;
    }
    program->reply({"frame=", program->frame, " exceptions=", program->exceptions});
    exit(program->exceptions ? ExitScriptError : ExitOK);
  }

  // ready for commands:
  program->reply({"ready ", program->game.title});

  std::string line;
  while (std::getline(std::cin, line)) {
    if (!program->command(string{line.c_str()})) break;
  }

  if (program->scriptEngineState.main_module) emulator->unloadScript();
  exit(ExitOK);
}
