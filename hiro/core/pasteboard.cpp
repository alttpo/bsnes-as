#if defined(Hiro_Pasteboard)

auto Pasteboard::pasteString(const string& text) -> void {
    pPasteboard::pasteString(text);
}

#endif
