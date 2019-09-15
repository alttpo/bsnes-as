auto PPU::Line::renderObject(PPU::IO::Object& self) -> void {
  if(!self.aboveEnable && !self.belowEnable) return;

  bool windowAbove[256];
  bool windowBelow[256];
  renderWindow(self.window, self.window.aboveEnable, windowAbove);
  renderWindow(self.window, self.window.belowEnable, windowBelow);

  uint itemCount = 0;
  uint tileCount = 0;
  for(uint n : range(ppu.ItemLimit+1024)) items[n].valid = false;
  for(uint n : range(ppu.TileLimit+1024)) tiles[n].valid = false;

  int lineY = (int)y;
  uint nativeItemCount = 0;
  uint nativeTileCount = 0;
  for(uint n : range(128)) {
    ObjectItem item{true, uint8_t(self.first + n & 127)};
    const auto& object = ppu.objects[item.index];

    if(object.size == 0) {
      static const uint widths[]  = { 8,  8,  8, 16, 16, 32, 16, 16};
      static const uint heights[] = { 8,  8,  8, 16, 16, 32, 32, 32};
      item.width  = widths [self.baseSize];
      item.height = heights[self.baseSize];
      if(self.interlace && self.baseSize >= 6) item.height = 16;  //hardware quirk
    } else {
      static const uint widths[]  = {16, 32, 64, 32, 64, 64, 32, 32};
      static const uint heights[] = {16, 32, 64, 32, 64, 64, 64, 32};
      item.width  = widths [self.baseSize];
      item.height = heights[self.baseSize];
    }

    if(!(object.x > 256 && object.x + item.width - 1 < 512)) {
      uint height = item.height >> self.interlace;
      if ((y >= object.y && y < object.y + height)
          || (object.y + height >= 256 && y < (object.y + height & 255))
        ) {
        if (nativeItemCount++ >= ppu.ItemLimit) goto addExtra;
        items[itemCount++] = item;
      }
    }

  addExtra:
    // inject extra items:
    for (uint k : range(min(ppu.extraTileCount,128))) {
      // skip tile if not the right source:
      const auto& extra = ppu.extraTiles[k];
      if (extra.source < Source::OBJ1) continue;
      if (extra.source > Source::OBJ2) continue;

      // inject extra tile at this OAM index:
      if (extra.index == item.index) {
        int tileHeight = (int)extra.height;

        if (lineY < extra.y) continue;
        if (lineY >= extra.y + tileHeight) continue;

        int tileY = extra.vflip ? tileHeight - (lineY - extra.y) - 1 : lineY - extra.y;
        if (tileY < 0) continue;
        if (tileY >= extra.height) continue;

        item.extraIndex = k+1;
        items[itemCount++] = item;
      }
    }

    if (nativeItemCount >= ppu.ItemLimit) break;
  }

  for(int n : reverse(range(itemCount))) {
    const auto& item = items[n];
    if(!item.valid) continue;

    if (item.extraIndex) {
      ObjectTile tile{true};
      tile.extraIndex = item.extraIndex;

      tiles[tileCount++] = tile;
      continue;
    }

    const auto& object = ppu.objects[item.index];
    uint tileWidth = item.width >> 3;
    int x = object.x;
    int y = this->y - object.y & 0xff;
    if(self.interlace) y <<= 1;

    if(object.vflip) {
      if(item.width == item.height) {
        y = item.height - 1 - y;
      } else if(y < item.width) {
        y = item.width - 1 - y;
      } else {
        y = item.width + (item.width - 1) - (y - item.width);
      }
    }

    if(self.interlace) {
      y = !object.vflip ? y + field() : y - field();
    }

    x &= 511;
    y &= 255;

    uint16 tiledataAddress = self.tiledataAddress;
    if(object.nameselect) tiledataAddress += 1 + self.nameselect << 12;
    uint16 characterX =  (object.character & 15);
    uint16 characterY = ((object.character >> 4) + (y >> 3) & 15) << 4;

    for(uint tileX : range(tileWidth)) {
      uint objectX = x + (tileX << 3) & 511;
      if(x != 256 && objectX >= 256 && objectX + 7 < 512) continue;

      ObjectTile tile{true};
      tile.x = objectX;
      tile.y = y;
      tile.priority = object.priority;
      tile.palette = 128 + (object.palette << 4);
      tile.hflip = object.hflip;

      uint mirrorX = !object.hflip ? tileX : tileWidth - 1 - tileX;
      uint address = tiledataAddress + ((characterY + (characterX + mirrorX & 15)) << 4);
      tile.number = address >> 4 & 0x7ff;

      if(nativeTileCount++ >= ppu.TileLimit) break;
      tiles[tileCount++] = tile;
    }
  }

  ppu.io.obj.rangeOver |= nativeItemCount > ppu.ItemLimit;
  ppu.io.obj.timeOver  |= nativeTileCount > ppu.TileLimit;

  uint8_t source[256] = {};
  uint16 colors[256] = {};
  uint8_t priority[256] = {};

  for(uint n : range(tileCount)) {
    const auto& tile = tiles[n];
    if(!tile.valid) continue;

    if (tile.extraIndex) {
      const auto& extra = ppu.extraTiles[tile.extraIndex - 1];

      int tileHeight = (int)extra.height;
      int tileY = extra.vflip ? tileHeight - (lineY - extra.y) - 1 : lineY - extra.y;

      int tileWidth = (int)extra.width;

      // draw the sprite:
      for (int tx = 0; tx < tileWidth; tx++) {
        if (extra.x + tx < 0) continue;
        if (extra.x + tx >= 256) break;

        int tileX = extra.hflip ? tileWidth - tx - 1 : tx;
        if (tileX < 0) continue;
        if (tileX >= tileWidth) continue;

        int index = tileY * tileWidth + tileX;
        if (index < 0) continue;
        if (index >= 1024) continue;

        auto color = extra.colors[index];

        // make sure color is opaque:
        if (color & 0x8000) {
          source[extra.x + tx] = extra.source;
          priority[extra.x + tx] = self.priority[extra.priority];
          colors[extra.x + tx] = color & 0x7fff;
        }
      }

      continue;
    }

    auto tiledata = ppu.tilecache[TileMode::BPP4] + (tile.number << 6) + ((tile.y & 7) << 3);
    uint tileX = tile.x;
    uint mirrorX = tile.hflip ? 7 : 0;
    for(uint x : range(8)) {
      tileX &= 511;
      if(tileX < 256) {
        if(uint color = tiledata[x ^ mirrorX]) {
          uint8_t palette = tile.palette + color;
          source[tileX] = palette < 192 ? Source::OBJ1 : Source::OBJ2;
          colors[tileX] = cgram[palette];
          priority[tileX] = self.priority[tile.priority];
        }
      }
      tileX++;
    }
  }

  for(uint x : range(256)) {
    if(!priority[x]) continue;
    if(self.aboveEnable && !windowAbove[x]) plotAbove(x, source[x], priority[x], colors[x]);
    if(self.belowEnable && !windowBelow[x]) plotBelow(x, source[x], priority[x], colors[x]);
  }
}

auto PPU::oamAddressReset() -> void {
  io.oamAddress = io.oamBaseAddress;
  oamSetFirstObject();
}

auto PPU::oamSetFirstObject() -> void {
  io.obj.first = !io.oamPriority ? 0 : io.oamAddress >> 2 & 0x7f;
}

auto PPU::readObject(uint10 address) -> uint8 {
  if(!(address & 0x200)) {
    uint n = address >> 2;  //object#
    address &= 3;
    if(address == 0) return objects[n].x;
    if(address == 1) return objects[n].y - 1;
    if(address == 2) return objects[n].character;
    return (
      objects[n].nameselect << 0
    | objects[n].palette    << 1
    | objects[n].priority   << 4
    | objects[n].hflip      << 6
    | objects[n].vflip      << 7
    );
  } else {
    uint n = (address & 0x1f) << 2;  //object#
    return (
      objects[n + 0].x >> 8 << 0
    | objects[n + 0].size   << 1
    | objects[n + 1].x >> 8 << 2
    | objects[n + 1].size   << 3
    | objects[n + 2].x >> 8 << 4
    | objects[n + 2].size   << 5
    | objects[n + 3].x >> 8 << 6
    | objects[n + 3].size   << 7
    );
  }
}

auto PPU::writeObject(uint10 address, uint8 data) -> void {
  if(!(address & 0x200)) {
    uint n = address >> 2;  //object#
    address &= 3;
    if(address == 0) { objects[n].x = objects[n].x & 0x100 | data; return; }
    if(address == 1) { objects[n].y = data + 1; return; }  //+1 => rendering happens one scanline late
    if(address == 2) { objects[n].character = data; return; }
    objects[n].nameselect = data >> 0 & 1;
    objects[n].palette    = data >> 1 & 7;
    objects[n].priority   = data >> 4 & 3;
    objects[n].hflip      = data >> 6 & 1;
    objects[n].vflip      = data >> 7 & 1;
  } else {
    uint n = (address & 0x1f) << 2;  //object#
    objects[n + 0].x = objects[n + 0].x & 0xff | data << 8 & 0x100;
    objects[n + 1].x = objects[n + 1].x & 0xff | data << 6 & 0x100;
    objects[n + 2].x = objects[n + 2].x & 0xff | data << 4 & 0x100;
    objects[n + 3].x = objects[n + 3].x & 0xff | data << 2 & 0x100;
    objects[n + 0].size = data >> 1 & 1;
    objects[n + 1].size = data >> 3 & 1;
    objects[n + 2].size = data >> 5 & 1;
    objects[n + 3].size = data >> 7 & 1;
  }
}
