#if defined(Hiro_Pasteboard)

namespace hiro {

struct pPasteboard {
  static auto pasteString(const mWindow& parentWindow, const string& text) -> void;
};

}

#endif
