#if defined(Hiro_Application)

auto Application::abort() -> void {
  quit();
  ::abort();
}

auto Application::doMain() -> void {
  if(state().onMain) return state().onMain();
}

auto Application::exit() -> void {
  state().quit = true;
  return pApplication::exit();
}

auto Application::font() -> Font {
  return state().font;
}

auto Application::locale() -> Locale& {
  return state().locale;
}

auto Application::modal() -> bool {
  return pApplication::modal();
}

auto Application::name() -> string {
  return state().name;
}

auto Application::onMain(const function<void ()>& callback) -> void {
  state().onMain = callback;
}

auto Application::run() -> void {
  return pApplication::run();
}

auto Application::pendingEvents() -> bool {
  return pApplication::pendingEvents();
}

auto Application::processEvents() -> void {
  return pApplication::processEvents();
}

auto Application::quit() -> void {
  state().quit = true;
  return pApplication::quit();
}

auto Application::scale() -> float {
  return state().scale;
}

auto Application::scale(float value) -> float {
  return value * state().scale;
}

auto Application::screenSaver() -> bool {
  return state().screenSaver;
}

auto Application::setFont(const Font& font) -> void {
  state().font = font;
}

auto Application::setName(const string& name) -> void {
  state().name = name;
}

auto Application::setScale(float scale) -> void {
  state().scale = scale;
}

auto Application::setScreenSaver(bool screenSaver) -> void {
  state().screenSaver = screenSaver;
  if(state().initialized) pApplication::setScreenSaver(screenSaver);
}

auto Application::setToolTips(bool toolTips) -> void {
  state().toolTips = toolTips;
}

//this ensures Application::state is initialized prior to use
auto Application::state() -> State& {
  static State state;
  return state;
}

auto Application::toolTips() -> bool {
  return state().toolTips;
}

auto Application::unscale(float value) -> float {
  return value * (1.0 / state().scale);
}

//Cocoa
//=====

auto Application::Cocoa::doAbout() -> void {
  if(state().cocoa.onAbout) return state().cocoa.onAbout();
}

auto Application::Cocoa::doActivate() -> void {
  if(state().cocoa.onActivate) return state().cocoa.onActivate();
}

auto Application::Cocoa::doOpen(const string& location) -> void {
  //the open-document event can arrive (via a raw Apple Event handler registered at the
  //earliest possible moment) before the application has finished constructing its UI and
  //registered a real onOpen callback: buffer it so it isn't silently dropped on a cold launch
  if(state().cocoa.onOpen) return state().cocoa.onOpen(location);
  state().cocoa.pendingOpen.append(location);
}

auto Application::Cocoa::doPreferences() -> void {
  if(state().cocoa.onPreferences) return state().cocoa.onPreferences();
}

auto Application::Cocoa::doQuit() -> void {
  if(state().cocoa.onQuit) return state().cocoa.onQuit();
}

auto Application::Cocoa::onAbout(const function<void ()>& callback) -> void {
  state().cocoa.onAbout = callback;
}

auto Application::Cocoa::onActivate(const function<void ()>& callback) -> void {
  state().cocoa.onActivate = callback;
}

auto Application::Cocoa::onOpen(const function<void (string)>& callback) -> void {
  state().cocoa.onOpen = callback;
  if(callback) {
    auto pending = move(state().cocoa.pendingOpen);
    state().cocoa.pendingOpen = {};
    for(auto& location : pending) callback(location);
  }
}

auto Application::Cocoa::onPreferences(const function<void ()>& callback) -> void {
  state().cocoa.onPreferences = callback;
}

auto Application::Cocoa::onQuit(const function<void ()>& callback) -> void {
  state().cocoa.onQuit = callback;
}

//Internal
//========

auto Application::initialize() -> void {
  if(!state().initialized) {
    state().initialized = true;
    pApplication::initialize();
    pApplication::setScreenSaver(state().screenSaver);
  }
}

#endif
