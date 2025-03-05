#if defined(Hiro_Pasteboard)

namespace hiro {

auto pPasteboard::pasteString(const mWindow& parent, const string& text) -> void {
  if (auto pWindow = parent.self()) {
    if (!OpenClipboard(pWindow->hwnd)) {
      return;
    }

    utf16_t output(text);
    if (auto resource = GlobalAlloc(GMEM_MOVEABLE, (wcslen(output) + 1) * sizeof(wchar_t))) {
      if (auto write = (wchar_t*)GlobalLock(resource)) {
        wcscpy(write, output);
        GlobalUnlock(write);

        if (SetClipboardData(CF_UNICODETEXT, resource) == nullptr) {
          GlobalFree(resource);
        }
      }
    }

    CloseClipboard();
  }
}

}

#endif
