#if defined(Hiro_Canvas)

namespace hiro {

auto pCanvas::construct() -> void {
  hwnd = CreateWindow(L"hiroWidget", L"", WS_CHILD, 0, 0, 0, 0, _parentHandle(), nullptr, GetModuleHandle(0), 0);
  pWidget::construct();
  update();
}

auto pCanvas::destruct() -> void {
  DestroyWindow(hwnd);
}

auto pCanvas::minimumSize() const -> Size {
  if(auto& icon = state().icon) return {(int)icon.width(), (int)icon.height()};
  return {0, 0};
}

auto pCanvas::setAlignment(Alignment) -> void {
  _redraw();
}

auto pCanvas::setColor(Color color) -> void {
  _rasterize();
  _redraw();
}

auto pCanvas::setDroppable(bool droppable) -> void {
  DragAcceptFiles(hwnd, droppable);
}

auto pCanvas::setFocusable(bool focusable) -> void {
  //handled by windowProc()
}

auto pCanvas::setGeometry(Geometry geometry) -> void {
  pWidget::setGeometry(geometry);
  _redraw();
}

auto pCanvas::setGradient(Gradient gradient) -> void {
  _rasterize();
  _redraw();
}

auto pCanvas::setIcon(const image& icon) -> void {
  _rasterize();
  _redraw();
}

auto pCanvas::update() -> void {
  _rasterize();
  _redraw();
}

//

auto pCanvas::doMouseLeave() -> void {
  return self().doMouseLeave();
}

auto pCanvas::doMouseMove(int x, int y) -> void {
  return self().doMouseMove({x, y});
}

auto pCanvas::windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) -> maybe<LRESULT> {
  if(msg == WM_DROPFILES) {
    if(auto paths = DropPaths(wparam)) self().doDrop(paths);
    return false;
  }

  if(msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP) {
    if(self().focusable()) return true;
  }

  if(msg == WM_GETDLGCODE) {
    return DLGC_STATIC | DLGC_WANTCHARS;
  }

  if(msg == WM_ERASEBKGND || msg == WM_PAINT) {
    _paint();
    return msg == WM_ERASEBKGND;
  }

  if(msg == WM_LBUTTONDOWN) {
    if(self().focusable()) setFocused();
  }

  return pWidget::windowProc(hwnd, msg, wparam, lparam);
}

//

auto pCanvas::_paint() -> void {
  PAINTSTRUCT ps;
  BeginPaint(hwnd, &ps);

  int sx = 0, sy = 0, dx = 0, dy = 0;
  int width = this->width;
  int height = this->height;
  auto geometry = self().geometry();
  auto alignment = state().alignment ? state().alignment : Alignment{0.5, 0.5};

  // [jsd] scale destination image proportionally up or down:
  int dwidth = geometry.width();
  int dheight = geometry.height();
  float scale = 1.0;
  float wscale = (float)dwidth / (float)width;
  float hscale = (float)dheight / (float)height;
  if (wscale <= hscale) {
    scale = wscale;
  } else {
    scale = hscale;
  }
  dwidth  = width * scale;
  dheight = height * scale;

  if(dwidth <= geometry.width()) {
    sx = 0;
    dx = (geometry.width() - dwidth) * alignment.horizontal();
  } else {
    sx = (dwidth - geometry.width()) * alignment.horizontal();
    dx = 0;
  }

  if(dheight <= geometry.height()) {
    sy = 0;
    dy = (geometry.height() - dheight) * alignment.vertical();
  } else {
    sy = (dheight - geometry.height()) * alignment.vertical();
    dy = 0;
  }

  //RECT rc;
  //GetClientRect(hwnd, &rc);
  //DrawThemeParentBackground(hwnd, ps.hdc, &rc);

  // fill in the outside spaces to clear up any remnants:
  RECT rc;
  if (dx > 0) {
    // left bar:
    SetRect(&rc, 0, 0, dx-1, geometry.height() - 1);
    DrawThemeParentBackground(hwnd, ps.hdc, &rc);
  }
  if (dy > 0) {
    // top bar:
    SetRect(&rc, dx, 0, geometry.width() - 1, dy - 1);
    DrawThemeParentBackground(hwnd, ps.hdc, &rc);
  }
  if (dx + dwidth < geometry.width()) {
    // right bar:
    SetRect(&rc, dx + dwidth, dy, geometry.width(), geometry.height());
    DrawThemeParentBackground(hwnd, ps.hdc, &rc);
  }
  if (dy + dheight < geometry.height()) {
    // bottom bar:
    SetRect(&rc, dx, dy + dheight, geometry.width(), geometry.height());
    DrawThemeParentBackground(hwnd, ps.hdc, &rc);
  }

  BLENDFUNCTION bf{AC_SRC_OVER, 0, (BYTE)255, AC_SRC_ALPHA};
  AlphaBlend(ps.hdc, dx, dy, dwidth, dheight, bitmapDC, 0, 0, width, height, bf);

  EndPaint(hwnd, &ps);
}

auto pCanvas::_rasterize() -> void {
  if(auto& icon = state().icon) {
    width = icon.width();
    height = icon.height();
  } else {
    width = self().geometry().width();
    height = self().geometry().height();
  }
  if(width <= 0 || height <= 0) return;

  bool recreateBitmap = false;
  if (width != bmi.bmiHeader.biWidth || height != -bmi.bmiHeader.biHeight || bitmapDC == 0 || bitmap == 0 || bits == nullptr) {
    recreateBitmap = true;
    if (bitmap) {
      DeleteObject(bitmap);
      bitmap = 0;
    }
    if (bitmapDC) {
      DeleteDC(bitmapDC);
      bitmapDC = 0;
    }
    bits = nullptr;
  }

  if (recreateBitmap) {
    //bitmapDC = CreateCompatibleDC(ps.hdc);
    bitmapDC = CreateCompatibleDC(GetDC(hwnd));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;  //GDI stores bitmaps upside now; negative height flips bitmap
    bmi.bmiHeader.biSizeImage = width * height * sizeof(uint32_t);
    bits = nullptr;
    bitmap = CreateDIBSection(bitmapDC, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    SelectObject(bitmapDC, bitmap);
  }

  // [jsd] update the contents of the HBITMAP from the image (icon):
  if(auto& icon = state().icon) {
    // [jsd] optimized to avoid allocation
    image fakeTarget(0, 32, 255u << 24, 255u << 16, 255u << 8, 255u << 0);  // Windows uses ARGB format
    fakeTarget.use((uint8_t*)bits, width, height);
    icon.transformTo(fakeTarget);
  } else if(auto& gradient = state().gradient) {
    auto& colors = gradient.state.colors;
    image fill;
    fill.allocate(width, height);
    fill.gradient(colors[0].value(), colors[1].value(), colors[2].value(), colors[3].value());
    memory::copy(bits, fill.data(), fill.size());
  } else {
    uint32_t color = state().color.value();
    memory::fill<uint32_t>(bits, width * height, color);
  }

  if(bits) {
    // [jsd] this loop crops the image and pre-multiplies the alpha channel into the RGB components:
    for(uint y : range(height)) {
      auto source = (const uint8_t*)bits + y * width * sizeof(uint32_t);
      auto target = (uint8_t*)bits + y * width * sizeof(uint32_t);
      for(uint x : range(width)) {
        target[0] = (source[0] * source[3]) / 255;
        target[1] = (source[1] * source[3]) / 255;
        target[2] = (source[2] * source[3]) / 255;
        target[3] = (source[3]);
        source += 4, target += 4;
      }
    }
  }
}

auto pCanvas::_redraw() -> void {
  InvalidateRect(hwnd, 0, false);
}

}

#endif
