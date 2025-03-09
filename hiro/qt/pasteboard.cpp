#if defined(Hiro_Pasteboard)

namespace hiro {

auto pPasteboard::pasteString(const mWindow& parent, const string& text) -> void {
  if (auto pWindow = parent.self()) {
    // TODO: fill pasteboard in Qt5
  }
}

}

#endif
