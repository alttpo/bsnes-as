#if defined(Hiro_Pasteboard)

namespace hiro {

auto pPasteboard::pasteString(const mWindow& parent, const string& text) -> void {
  if (auto pWindow = parent.self()) {
    auto gdkWindow = gtk_widget_get_window(pWindow->widget);
    auto display = gdk_window_get_display(gdkWindow);
    //auto display = gdk_display_get_default();

    auto clipboard = gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD);
    gtk_clipboard_set_text(clipboard, text, text.size());
  }
}

}

#endif
