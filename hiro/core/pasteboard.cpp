#if defined(Hiro_Pasteboard)

auto Pasteboard::pasteString(const mWindow& parent, const string& text) -> void {
    pPasteboard::pasteString(parent, text);
}

#endif
