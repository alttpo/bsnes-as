auto Program::openGame(BrowserDialog& dialog) -> string {
  if(!settings.general.nativeFileDialogs) {
    return dialog.openObject();
  }

  BrowserWindow window;
  window.setTitle(dialog.title());
  window.setPath(dialog.path());
  window.setFilters(dialog.filters());
  window.setParent(dialog.alignmentWindow());
  return window.open();
}

auto Program::openFile(BrowserDialog& dialog) -> string {
  if(!settings.general.nativeFileDialogs) {
    return dialog.openFile();
  }

  BrowserWindow window;
  window.setTitle(dialog.title());
  window.setPath(dialog.path());
  window.setFilters(dialog.filters());
  window.setParent(dialog.alignmentWindow());
  return window.open();
}

auto Program::saveFile(BrowserDialog& dialog) -> string {
  if(!settings.general.nativeFileDialogs) {
    return dialog.saveFile();
  }

  BrowserWindow window;
  window.setTitle(dialog.title());
  window.setPath(dialog.path());
  window.setFilters(dialog.filters());
  window.setParent(dialog.alignmentWindow());
  return window.save();
}

auto Program::selectPath() -> string {
  if(!settings.general.nativeFileDialogs) {
    BrowserDialog dialog;
    dialog.setPath(Path::program());
    return dialog.selectFolder();
  }

  BrowserWindow window;
  window.setPath(Path::program());
  return window.directory();
}

auto Program::showMessage(string text) -> void {
  statusTime = chrono::millisecond();
  statusMessage = text;
}

auto Program::showFrameRate(string text) -> void {
  statusFrameRate = text;
}

auto Program::updateStatus() -> void {
  string message;
  if(chrono::millisecond() - statusTime <= 2000) {
    message = statusMessage;
  }
  if(message != presentation.statusLeft.text()) {
    presentation.statusLeft.setText(message);
  }

  string frameRate;
  if(!emulator->loaded()) {
    frameRate = tr("Unloaded");
  } else if(presentation.frameAdvance.checked() && frameAdvanceLock) {
    frameRate = tr("Frame Advance");
  } else if(presentation.pauseEmulation.checked()) {
    frameRate = tr("Paused");
  } else if(!focused() && inputSettings.pauseEmulation.checked()) {
    frameRate = tr("Paused");
  } else {
    frameRate = statusFrameRate;
  }
  if(frameRate != presentation.statusRight.text()) {
    presentation.statusRight.setText(frameRate);
  }
}

auto Program::captureScreenshot() -> bool {
  if(emulator->loaded() && screenshot.data) {
    if(auto filename = screenshotPath()) {
      //RGB555 -> RGB888
      //image capture{0, 16, 0x8000, 0x7c00, 0x03e0, 0x001f};
      //capture.copy(screenshot.data, screenshot.pitch, screenshot.width, screenshot.height);
      //capture.transform(0, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);

      image output(0, 32, 0xff000000, 0x00ff0000, 0x0000ff00, 0x000000ff);
      {
        const uint16 *data;
        uint pitch, width, height, scale;

        data = screenshot.data;
        pitch = screenshot.pitch;
        width = screenshot.width;
        height = screenshot.height;
        scale = screenshot.scale;

        pitch >>= 1;
        if(!settings.video.overscan) {
          uint multiplier = height / 240;
          data += 8 * multiplier * pitch;
          height -= 16 * multiplier;
        }

        uint filterWidth = width, filterHeight = height;
        auto filterRender = filterSelect(filterWidth, filterHeight, scale);

        // allocate buffer space for filtered output:
        output.allocate(filterWidth, filterHeight);

        // filter the output:
        filterRender(
          palette,
          (uint32_t*)output.data(), output.pitch(),
          (const uint16_t *)data, pitch << 1,
          width, height
        );

        // scale to final output size:
        uint outputWidth, outputHeight;
        viewportSize(outputWidth, outputHeight, scale);
        output.scale(outputWidth, outputHeight, false);
      }

      {
        auto data = output.data();
        auto pitch = output.pitch();
        auto width = output.width();
        auto height = output.height();

        if (Encode::BMP::create(filename, data, width << 2, width, height, /* alpha = */ false)) {
          showMessage({"Captured screenshot [", Location::file(filename), "]"});
          return true;
        }
      }
    }
  }
  return false;
}

auto Program::inactive() -> bool {
  if(locked()) return true;
  if(!emulator->loaded()) return true;
  if(presentation.pauseEmulation.checked()) return true;
  if(presentation.frameAdvance.checked() && frameAdvanceLock) return true;
  if(!focused() && inputSettings.pauseEmulation.checked()) return true;
  return false;
}

auto Program::focused() -> bool {
  //full-screen mode creates its own top-level window: presentation window will not have focus
  if(video.fullScreen() || presentation.focused()) {
    mute &= ~Mute::Unfocused;
    return true;
  } else {
    if(settings.audio.muteUnfocused) {
      mute |= Mute::Unfocused;
    } else {
      mute &= ~Mute::Unfocused;
    }
    return false;
  }
}
