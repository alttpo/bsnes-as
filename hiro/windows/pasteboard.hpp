#if defined(Hiro_Pasteboard)

namespace hiro {

struct pPasteboard {
  static auto pasteString(const mWindow& parent, const string& text) -> void;
};

}

#endif
