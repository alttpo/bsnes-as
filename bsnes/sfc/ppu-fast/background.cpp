
auto PPU::Line::renderBackground(PPU::IO::Background& self, uint8 source) -> void {
  if (!self.aboveEnable && !self.belowEnable) return;

  auto tMode = self.tileMode;
  if (tMode == TileMode::Mode7) {
    return renderMode7(self, source);
  } else if (tMode == TileMode::BPP2) {
    switch (io.bgMode) {
      case 0: _renderBackgroundTileMode<0, TileMode::BPP2>(self, source); break;
      case 1: _renderBackgroundTileMode<1, TileMode::BPP2>(self, source); break;
      case 2: _renderBackgroundTileMode<2, TileMode::BPP2>(self, source); break;
      case 3: _renderBackgroundTileMode<3, TileMode::BPP2>(self, source); break;
      case 4: _renderBackgroundTileMode<4, TileMode::BPP2>(self, source); break;
      case 5: _renderBackgroundTileMode<5, TileMode::BPP2>(self, source); break;
      case 6: _renderBackgroundTileMode<6, TileMode::BPP2>(self, source); break;
    }
  } else if (tMode == TileMode::BPP4) {
    switch (io.bgMode) {
      case 0: _renderBackgroundTileMode<0, TileMode::BPP4>(self, source); break;
      case 1: _renderBackgroundTileMode<1, TileMode::BPP4>(self, source); break;
      case 2: _renderBackgroundTileMode<2, TileMode::BPP4>(self, source); break;
      case 3: _renderBackgroundTileMode<3, TileMode::BPP4>(self, source); break;
      case 4: _renderBackgroundTileMode<4, TileMode::BPP4>(self, source); break;
      case 5: _renderBackgroundTileMode<5, TileMode::BPP4>(self, source); break;
      case 6: _renderBackgroundTileMode<6, TileMode::BPP4>(self, source); break;
    }
  } else if (tMode == TileMode::BPP8) {
    switch (io.bgMode) {
      case 0: _renderBackgroundTileMode<0, TileMode::BPP8>(self, source); break;
      case 1: _renderBackgroundTileMode<1, TileMode::BPP8>(self, source); break;
      case 2: _renderBackgroundTileMode<2, TileMode::BPP8>(self, source); break;
      case 3: _renderBackgroundTileMode<3, TileMode::BPP8>(self, source); break;
      case 4: _renderBackgroundTileMode<4, TileMode::BPP8>(self, source); break;
      case 5: _renderBackgroundTileMode<5, TileMode::BPP8>(self, source); break;
      case 6: _renderBackgroundTileMode<6, TileMode::BPP8>(self, source); break;
    }
  } else { // Inactive
    return;
  }
}

template<uint8 bgMode, uint8 tMode>
auto PPU::Line::_renderBackgroundTileMode(PPU::IO::Background& self, uint8 source) -> void {
  constexpr bool hires = bgMode == 5 || bgMode == 6;
  constexpr bool offsetPerTileMode = bgMode == 2 || bgMode == 4 || bgMode == 6;
  bool directColorMode = io.col.directColor && source == Source::BG1 && (bgMode == 3 || bgMode == 4);
  constexpr uint colorShift = 3 + tMode;
  constexpr int width = 256 << hires;

  bool windowAbove[256];
  bool windowBelow[256];
  renderWindow(self.window, self.window.aboveEnable, windowAbove);
  renderWindow(self.window, self.window.belowEnable, windowBelow);

  uint tileHeight = 3 + self.tileSize;
  uint tileWidth;
  if constexpr(!hires) {
    tileWidth = tileHeight;
  } else {
    tileWidth = 4;
  }
  constexpr uint tileMask = 0x0fff >> tMode;
  uint tiledataIndex = self.tiledataAddress >> 3 + tMode;

  uint paletteBase;
  if constexpr (bgMode == 0)
    paletteBase = source << 5;
  else
    paletteBase = 0;
  uint paletteShift = 2 << tMode;

  uint hscroll = self.hoffset;
  uint vscroll = self.voffset;
  uint hmask = (width << self.tileSize << !!(self.screenSize & 1)) - 1;
  uint vmask = (width << self.tileSize << !!(self.screenSize & 2)) - 1;

  uint y = this->y;
  if(self.mosaicEnable) y -= io.mosaic.size - io.mosaic.counter;
  if constexpr(hires) {
    hscroll <<= 1;
    if(io.interlace) {
      y = y << 1 | field();
      if(self.mosaicEnable) y -= io.mosaic.size - io.mosaic.counter + field();
    }
  }

  uint mosaicCounterTop = self.mosaicEnable ? io.mosaic.size : 1;
  uint mosaicCounter = 1;
  uint mosaicPalette = 0;
  uint8 mosaicPriority = 0;
  uint16 mosaicColor = 0;

  auto getTile = [=](PPU::IO::Background& self, uint hoffset, uint voffset) -> uint {
    uint screenX = self.screenSize & 1 ? 32 << 5 : 0;
    uint screenY = self.screenSize & 2 ? 32 << 5 + (self.screenSize & 1) : 0;
    uint tileX = hoffset >> tileWidth;
    uint tileY = voffset >> tileHeight;
    uint offset = (tileY & 0x1f) << 5 | (tileX & 0x1f);
    if(tileX & 0x20) offset += screenX;
    if(tileY & 0x20) offset += screenY;
    return ppu.vram[self.screenAddress + offset & 0x7fff];
  };

  int x = 0 - (hscroll & 7);
  while(x < width) {
    uint hoffset = x + hscroll;
    uint voffset = y + vscroll;
    if constexpr(offsetPerTileMode) {
      uint validBit = 0x2000 << source;
      uint offsetX = x + (hscroll & 7);
      if(offsetX >= 8) {  //first column is exempt
        uint hlookup = getTile(io.bg3, (offsetX - 8) + (io.bg3.hoffset & ~7), io.bg3.voffset + 0);
        if constexpr(bgMode == 4) {
          if(hlookup & validBit) {
            if(!(hlookup & 0x8000)) {
              hoffset = offsetX + (hlookup & ~7);
            } else {
              voffset = y + hlookup;
            }
          }
        } else {
          uint vlookup = getTile(io.bg3, (offsetX - 8) + (io.bg3.hoffset & ~7), io.bg3.voffset + 8);
          if(hlookup & validBit) {
            hoffset = offsetX + (hlookup & ~7);
          }
          if(vlookup & validBit) {
            voffset = y + vlookup;
          }
        }
      }
    }
    hoffset &= hmask;
    voffset &= vmask;

    uint tileNumber = getTile(self, hoffset, voffset);
    //uint mirrorY = tileNumber & 0x8000 ? 7 : 0;
    //uint mirrorX = tileNumber & 0x4000 ? 7 : 0;
    uint mirrorY = ((tileNumber & 0x8000) >> 12) - ((tileNumber & 0x8000) >> 15);
    uint mirrorX = ((tileNumber & 0x4000) >> 11) - ((tileNumber & 0x4000) >> 14);
    uint8 tilePriority = self.priority[bool(tileNumber & 0x2000)];
    uint paletteNumber = tileNumber >> 10 & 7;
    uint paletteIndex = paletteBase + (paletteNumber << paletteShift) & 0xff;

    if(tileWidth  == 4 && (bool(hoffset & 8) ^ bool(mirrorX))) tileNumber +=  1;
    if(tileHeight == 4 && (bool(voffset & 8) ^ bool(mirrorY))) tileNumber += 16;
    tileNumber = (tileNumber & 0x03ff) + tiledataIndex & tileMask;

    uint16 address;
    address = (tileNumber << colorShift) + (voffset & 7 ^ mirrorY) & 0x7fff;

    uint64 data;
    data  = (uint64)ppu.vram[address +  0] <<  0;
    data |= (uint64)ppu.vram[address +  8] << 16;
    data |= (uint64)ppu.vram[address + 16] << 32;
    data |= (uint64)ppu.vram[address + 24] << 48;

    uint tileX = 0;
    while (x < 0) {
      tileX++;
      x++;
    }

    for(; tileX < 8; tileX++, x++) {
      if(x >= width) {
        break;
      }
      if(--mosaicCounter == 0) {
        uint color, shift = mirrorX ? tileX : 7 - tileX;

        if constexpr(tMode == TileMode::BPP2) {
          color  = data >> shift +  0 &   1;
          color += data >> shift +  7 &   2;
        } else if constexpr(tMode == TileMode::BPP4) {
          color  = data >> shift +  0 &   1;
          color += data >> shift +  7 &   2;
          color += data >> shift + 14 &   4;
          color += data >> shift + 21 &   8;
        } else if constexpr(tMode >= TileMode::BPP8) {
          color  = data >> shift +  0 &   1;
          color += data >> shift +  7 &   2;
          color += data >> shift + 14 &   4;
          color += data >> shift + 21 &   8;
          color += data >> shift + 28 &  16;
          color += data >> shift + 35 &  32;
          color += data >> shift + 42 &  64;
          color += data >> shift + 49 & 128;
        }

        mosaicCounter = mosaicCounterTop;
        mosaicPalette = color;
        mosaicPriority = tilePriority;
        if (!directColorMode) {
          mosaicColor = cgram[paletteIndex + mosaicPalette];
        } else {
          mosaicColor = directColor(paletteNumber, mosaicPalette);
        }
      }
      if(!mosaicPalette) {
        continue;
      }

      if constexpr(!hires) {
        if(self.aboveEnable && !windowAbove[x]) {
          plotAbove(x, source, mosaicPriority, mosaicColor);
        }
        if(self.belowEnable && !windowBelow[x]) {
          plotBelow(x, source, mosaicPriority, mosaicColor);
        }
      } else {
        uint X = x >> 1;
        if(!ppu.hd()) {
          if (!(x & 1)) {
            if (self.belowEnable && !windowBelow[X]) {
              plotBelow(X, source, mosaicPriority, mosaicColor);
            }
          } else {
            if (self.aboveEnable && !windowAbove[X]) {
              plotAbove(X, source, mosaicPriority, mosaicColor);
            }
          }
        } else {
          if(self.aboveEnable && !windowAbove[X]) {
            plotHD(above, X, source, mosaicPriority, mosaicColor, true, x & 1);
          }
          if(self.belowEnable && !windowBelow[X]) {
            plotHD(below, X, source, mosaicPriority, mosaicColor, true, x & 1);
          }
        }
      }
    }
  }
}
