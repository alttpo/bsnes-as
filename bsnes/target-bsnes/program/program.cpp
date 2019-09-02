#include "../bsnes.hpp"
#include "platform.cpp"
#include "game.cpp"
#include "game-pak.cpp"
#include "game-rom.cpp"
#include "paths.cpp"
#include "states.cpp"
#include "movies.cpp"
#include "rewind.cpp"
#include "video.cpp"
#include "audio.cpp"
#include "input.cpp"
#include "utility.cpp"
#include "patch.cpp"
#include "hacks.cpp"
#include "filter.cpp"
#include "viewport.cpp"
Program program;

// Implement a simple message callback function
void MessageCallback(const asSMessageInfo *msg, void *param)
{
  const char *type = "ERR ";
  if( msg->type == asMSGTYPE_WARNING )
    type = "WARN";
  else if( msg->type == asMSGTYPE_INFORMATION )
    type = "INFO";
  // [jsd] todo: hook this up to better systems available in bsens
  printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

void gfx_plot(int x, int y, uint16_t color) {

}

auto Program::create() -> void {
  Emulator::platform = this;

  // [jsd] initialize angelscript
  script.engine = asCreateScriptEngine();
  // Set the message callback to receive information on errors in human readable form.
  int r = script.engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);
  assert(r >= 0);

  // register a function the script can call:
  r = script.engine->RegisterGlobalFunction("void gfx_plot(int,int,uint16)", asFUNCTION(gfx_plot), asCALL_CDECL);
  assert(r >= 0);

  // load script from file:
  string scriptSource = string::read("test.as");

  // add script into module:
  asIScriptModule *mod = script.engine->GetModule(0, asGM_ALWAYS_CREATE);
  r = mod->AddScriptSection("script", scriptSource.begin(), scriptSource.length());
  assert(r >= 0);

  // compile module:
  r = mod->Build();
  assert(r >= 0);

  presentation.create();
  presentation.setVisible();
  presentation.viewport.setFocused();

  settingsWindow.create();
  videoSettings.create();
  audioSettings.create();
  inputSettings.create();
  hotkeySettings.create();
  pathSettings.create();
  speedSettings.create();
  emulatorSettings.create();
  driverSettings.create();

  toolsWindow.create();
  cheatFinder.create();
  cheatDatabase.create();
  cheatWindow.create();
  cheatEditor.create();
  stateWindow.create();
  stateManager.create();
  manifestViewer.create();

  if(settings.general.crashed) {
    MessageDialog(
      "Driver crash detected. Hardware drivers have been disabled.\n"
      "Please reconfigure drivers in the advanced settings panel."
    ).setAlignment(*presentation).information();
    settings.video.driver = "None";
    settings.audio.driver = "None";
    settings.input.driver = "None";
  }

  settings.general.crashed = true;
  settings.save();
  updateVideoDriver(presentation);
  updateAudioDriver(presentation);
  updateInputDriver(presentation);
  settings.general.crashed = false;
  settings.save();

  driverSettings.videoDriverChanged();
  driverSettings.audioDriverChanged();
  driverSettings.inputDriverChanged();

  if(gameQueue) load();
  if(startFullScreen && emulator->loaded()) {
    toggleVideoFullScreen();
  }
  Application::onMain({&Program::main, this});
}

auto Program::main() -> void {
  updateStatus();
  video.poll();

  if(Application::modal()) {
    audio.clear();
    return;
  }

  inputManager.poll();
  inputManager.pollHotkeys();

  if(inactive()) {
    audio.clear();
    usleep(20 * 1000);
    viewportRefresh();
    return;
  }

  rewindRun();
  emulator->run();
  if(emulatorSettings.autoSaveMemory.checked()) {
    auto currentTime = chrono::timestamp();
    if(currentTime - autoSaveTime >= settings.emulator.autoSaveMemory.interval) {
      autoSaveTime = currentTime;
      emulator->save();
    }
  }
}

auto Program::quit() -> void {
  //make closing the program feel more responsive
  presentation.setVisible(false);
  Application::processEvents();

  unload();
  settings.save();
  video.reset();
  audio.reset();
  input.reset();
  Application::exit();
}
