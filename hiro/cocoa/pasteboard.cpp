#if defined(Hiro_Pasteboard)

namespace hiro {

auto pPasteboard::pasteString(const mWindow& parentWindow, const string& text) -> void {
  NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];

  [pasteboard clearContents];
  [pasteboard setString:[NSString stringWithUTF8String:text] forType:NSPasteboardTypeString];
}

}

#endif
