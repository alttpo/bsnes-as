#if defined(Hiro_Pasteboard)

namespace hiro {

auto pPasteboard::pasteString(const string& text) -> void {
  auto display = gdk_display_get_default();
  auto clipboard = gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text(clipboard, text, text.size());
}

}

#endif
