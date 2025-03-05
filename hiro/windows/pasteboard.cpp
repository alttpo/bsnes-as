#if defined(Hiro_Pasteboard)

namespace hiro {

auto pPasteboard::pasteString(const mWindow& parent, const string& text) -> void {
  // printf("hiro/windows/pasteString\n");
  if (auto pWindow = parent.self()) {
    if (!OpenClipboard(pWindow->hwnd)) {
      // printf("OpenClipboard failed on %p; GetLastError() -> %lu\n", pWindow->hwnd, GetLastError());
      return;
    }

    if (!EmptyClipboard()) {
      // printf("EmptyClipboard failed; GetLastError() -> %lu\n", GetLastError());
      return;
    }

    utf16_t output(text);
    auto wlen = (wcslen(output) + 1) * sizeof(wchar_t);
    if (auto resource = GlobalAlloc(GMEM_MOVEABLE, wlen)) {
      if (auto write = (wchar_t*)GlobalLock(resource)) {
        wcscpy(write, output);
        GlobalUnlock(write);

        HANDLE clipData = SetClipboardData(CF_UNICODETEXT, resource);
        if (clipData == nullptr) {
          // failed:
          // printf("SetClipboardData failed; GetLastError() -> %lu\n", GetLastError());
          GlobalFree(resource);
        }
      } else {
        // printf("GlobalLock(%p) failed\n", resource);
      }
    } else {
      // printf("GlobalAlloc failed for %d bytes\n", wlen);
    }

    CloseClipboard();
  } else {
    // printf("Unable to grab non-null parent.self()\n");
  }
}

}

#endif
