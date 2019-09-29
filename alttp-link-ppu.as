// ALTTP script to draw current Link sprites on top of rendered frame:
net::UDPSocket@ sock;
SettingsWindow @settings;
SpriteWindow @sprites;

void init() {
  @settings = SettingsWindow();
  @sprites = SpriteWindow();
}

class SpriteWindow {
private gui::Window @window;
private gui::VerticalLayout @vl;
  gui::Canvas @canvas;

  SpriteWindow() {
    // relative position to bsnes window:
    @window = gui::Window(256*2, 0, true);
    window.title = "Sprite VRAM";
    window.size = gui::Size(256, 512);

    @vl = gui::VerticalLayout();
    window.append(vl);

    @canvas = gui::Canvas();
    canvas.size = gui::Size(128, 256);
    vl.append(canvas, gui::Size(-1, -1));

    vl.resize();
    canvas.update();
    window.visible = true;
  }

  void update() {
    canvas.update();
  }
};

class SettingsWindow {
  private gui::Window @window;
  private gui::LineEdit @txtServerIP;
  private gui::LineEdit @txtClientIP;
  private gui::Button @ok;

  string clientIP;
  string serverIP;
  bool started;

  SettingsWindow() {
    @window = gui::Window(164, 22, true);
    window.title = "Connect to IP address";
    window.size = gui::Size(256, 24*3);

    auto vl = gui::VerticalLayout();
    {
      auto @hz = gui::HorizontalLayout();
      {
        auto @lbl = gui::Label();
        lbl.text = "Server IP:";
        hz.append(lbl, gui::Size(80, 0));

        @txtServerIP = gui::LineEdit();
        txtServerIP.text = "127.0.0.1";
        hz.append(txtServerIP, gui::Size(128, 20));
      }
      vl.append(hz, gui::Size(0, 0));

      @hz = gui::HorizontalLayout();
      {
        auto @lbl = gui::Label();
        lbl.text = "Client IP:";
        hz.append(lbl, gui::Size(80, 0));

        @txtClientIP = gui::LineEdit();
        txtClientIP.text = "127.0.0.2";
        hz.append(txtClientIP, gui::Size(128, 20));
      }
      vl.append(hz, gui::Size(-1, -1));

      @hz = gui::HorizontalLayout();
      {
        @ok = gui::Button();
        ok.text = "Start";
        @ok.on_activate = @gui::ButtonCallback(this.startClicked);
        hz.append(ok, gui::Size(-1, -1));

        auto swap = gui::Button();
        swap.text = "Swap";
        @swap.on_activate = @gui::ButtonCallback(this.swapClicked);
        hz.append(swap, gui::Size(-1, -1));
      }
      vl.append(hz, gui::Size(-1, -1));
    }
    window.append(vl);

    vl.resize();
    window.visible = true;
  }

  private void swapClicked(gui::Button @self) {
    auto tmp = txtServerIP.text;
    txtServerIP.text = txtClientIP.text;
    txtClientIP.text = tmp;
  }

  private void startClicked(gui::Button @self) {
    message("Start!");
    clientIP = txtClientIP.text;
    serverIP = txtServerIP.text;
    started = true;
    hide();
  }

  void show() {
    window.visible = true;
  }

  void hide() {
    window.visible = false;
  }
};

int16 abs16(int16 n) {
  auto mask = n >> (2 * 8 - 1);
  return ((n + mask) ^ mask);
}

class Sprite {
  uint8 index;
  uint16 chr;
  int16 x;
  int16 y;
  uint8 size;
  uint8 palette;
  uint8 priority;
  bool hflip;
  bool vflip;

  // fetches all the OAM sprite data for OAM sprite at `index`
  void fetchOAM(uint8 j, int16 rx, int16 ry) {
    auto tile = ppu::oam[j];

    index = j;

    int16 ax = int16(tile.x);
    int16 ay = int16(tile.y);

    // adjust x to allow for slightly off-screen sprites:
    if (ax >= 256) ax -= 512;

    // Make sprite x,y relative to incoming rx,ry coordinates (where Link is in screen coordinates):
    x = ax - rx;
    y = ay - ry;

    chr      = tile.character;
    size     = tile.size;
    palette  = tile.palette;
    priority = tile.priority;
    hflip    = tile.hflip;
    vflip    = tile.vflip;
  }

  void serialize(array<uint8> &r) {
    r.insertLast(index);
    r.insertLast(chr);
    r.insertLast(uint16(x));
    r.insertLast(uint16(y));
    r.insertLast(size);
    r.insertLast(palette);
    r.insertLast(priority);
    r.insertLast(hflip ? uint8(1) : uint8(0));
    r.insertLast(vflip ? uint8(1) : uint8(0));
  }

  int deserialize(array<uint8> &r, int c) {
    index = r[c++];
    chr = uint16(r[c++]) | uint16(r[c++] << 8);
    x = int16(uint16(r[c++]) | uint16(r[c++] << 8));
    y = int16(uint16(r[c++]) | uint16(r[c++] << 8));
    size = r[c++];
    palette = r[c++];
    priority = r[c++];
    hflip = (r[c++] != 0 ? true : false);
    vflip = (r[c++] != 0 ? true : false);
    return c;
  }
};

class TilemapWrite {
  uint32 addr;
  uint16 value;

  TilemapWrite(uint32 addr, uint16 value) {
    this.addr = addr;
    this.value = value;
  }
};

class VRAMWrite {
  uint16 vmaddr;
  array<uint16> data;
};

class Tile {
  uint16 addr;
  array<uint16> tiledata;

  Tile(uint16 addr, array<uint16> tiledata) {
    this.addr = addr;
    this.tiledata = tiledata;
  }
};

class GameState {
  // graphics data for current frame:
  array<Sprite@> sprites;
  array<array<uint16>> chrs(512);
  // backup of VRAM tiles overwritten:
  array<Tile@> chr_backup;

  // values copied from RAM:
  uint32 location;
  uint32 last_location;

  // screen scroll coordinates relative to top-left of room (BG screen):
  int16 xoffs;
  int16 yoffs;

  uint8 ctr;
  uint16 x, y, z;
  uint8 walking;
  uint8 subframe;
  uint8 frame;
  uint8 facing;
  uint8 pushing;
  uint8 state_aux;
  uint8 bunny;
  int8  speed;
  uint8 state;
  uint8 dashing;
  uint8 direction;
  uint8 level;

  uint8 ow_screen_transition;
  uint8 module;
  uint8 sub_module;
  uint8 sub_sub_module;
  uint8 to_dark_world;

  void update_module() {
    module = bus::read_u8(0x7E0010);
    sub_module = bus::read_u8(0x7E0011);
  }

  bool safe_to_update_tilemap() {
    if (module == 0x09) {
      if (sub_module == 0x00 || sub_module == 0x06) {
        return true;
      }
      return false;
    } else if (module == 0x07) {
      if (sub_module == 0x00) {
        return true;
      }
      return false;
    } else {
      return false;
    }
  }
  bool safe_to_fetch_tilemap() {
    if (module == 0x09) {
      if (sub_module == 0x00 || sub_module == 0x06) {
        return true;
      }
      return false;
    } else if (module == 0x07) {
      if (sub_module == 0x00) {
        return true;
      }
      return false;
    } else {
      return false;
    }
  }

  // for intercepting writes to the tilemap:
  array<TilemapWrite@> tileWrites;
  uint tileWritesIndex;
  array<VRAMWrite@> vramWrites;
  uint vramWritesIndex;
  uint8 vmaddrl, vmaddrh;
  bool intercepting;

  void fetch() {
    ctr = bus::read_u8(0x7E001A);
    y = bus::read_u16(0x7E0020, 0x7E0021);
    x = bus::read_u16(0x7E0022, 0x7E0023);
    z = bus::read_u16(0x7E0024, 0x7E0025);
    walking = bus::read_u8(0x7E0026);
    subframe = bus::read_u8(0x7E002D);
    frame = bus::read_u8(0x7E002E);
    facing = bus::read_u8(0x7E002F);
    pushing = bus::read_u8(0x7E0048);
    state_aux = bus::read_u8(0x7E004D);
    bunny = bus::read_u8(0x7E0056);
    speed = int8(bus::read_u8(0x7E0057));
    state = bus::read_u8(0x7E005D);
    dashing = bus::read_u8(0x7E005E);
    direction = bus::read_u8(0x7E0067);
    level = bus::read_u8(0x7E00EE);

    // $7E0410 = OW screen transitioning directional
    ow_screen_transition = bus::read_u8(0x7E0410);

    // module     = 0x07 in dungeons
    //            = 0x09 in overworld
    module = bus::read_u8(0x7E0010);

    // when module = 0x07: dungeon
    //    sub_module = 0x00 normal gameplay in dungeon
    //               = 0x01 going through door
    //               = 0x03 triggered a star tile to change floor hole configuration
    //               = 0x05 initializing room? / locked doors?
    //               = 0x07 falling down hole in floor
    //               = 0x0e going up/down stairs
    //               = 0x0f entering dungeon first time (or from mirror)
    //               = 0x16 when orange/blue barrier blocks transition
    //               = 0x19 when using mirror
    // when module = 0x09: overworld
    //    sub_module = 0x00 normal gameplay in overworld
    //               = 0x0e
    //      sub_sub_module = 0x01 in item menu
    //                     = 0x02 in dialog with NPC
    //               = 0x23 transitioning from light world to dark world or vice-versa
    // when module = 0x12: Link is dying
    //    sub_module = 0x00
    //               = 0x02 bonk
    //               = 0x03 black oval closing in
    //               = 0x04 red screen and spinning animation
    //               = 0x05 red screen and Link face down
    //               = 0x06 fade to black
    //               = 0x07 game over animation
    //               = 0x08 game over screen done
    //               = 0x09 save and continue menu
    sub_module = bus::read_u8(0x7E0011);

    // sub-sub-module goes from 01 to 0f during special animations such as link walking up/down stairs and
    // falling from ceiling and used as a counter for orange/blue barrier blocks transition going up/down
    sub_sub_module = bus::read_u8(0x7E00B0);

    //auto transition_ctr = bus::read_u8(0x7E0126);

    //message(fmtHex(screen_transition, 2) + ", " + fmtHex(subsubmodule, 2));

    // Don't update location until screen transition is complete:
    if (safe_to_update_tilemap()) {
      last_location = location;

      // fetch various room indices and flags about where exactly Link currently is:
      auto in_dark_world = bus::read_u8(0x7E0FFF);
      auto in_dungeon = bus::read_u8(0x7E001B);
      auto overworld_room = bus::read_u16(0x7E008A, 0x7E008B);
      auto dungeon_room = bus::read_u16(0x7E00A0, 0x7E00A1);

      // compute aggregated location for Link into a single 24-bit number:
      location =
        uint32(in_dark_world & 1) << 17 |
        uint32(in_dungeon & 1) << 16 |
        uint32(in_dungeon != 0 ? dungeon_room : overworld_room);

      // clear out list of room changes if location changed:
      if (last_location != location) {
        message("room from 0x" + fmtHex(last_location, 6) + " to 0x" + fmtHex(location, 6));
        tileWrites.resize(0);
        vramWrites.resize(0);
        tileWritesIndex = 0;
        vramWritesIndex = 0;
      }
    }

    // get screen x,y offset by reading BG2 scroll registers:
    xoffs = int16(bus::read_u16(0x7E00E2, 0x7E00E3));
    yoffs = int16(bus::read_u16(0x7E00E8, 0x7E00E9));

    fetch_sprites();
    fetch_room_updates();
  }

  void fetch_sprites() {
    // get link's on-screen coordinates in OAM space:
    int16 rx = int16(x) - xoffs;
    int16 ry = int16(y) - yoffs;

    // read in relevant sprites from OAM and VRAM:
    sprites.resize(0);
    sprites.reserve(8);
    int numsprites = 0;
    for (int i = 0; i < 128; i++) {
      // access current OAM sprite index:
      auto tile = ppu::oam[i];

      // skip OAM sprite if not enabled (X, Y coords are out of display range):
      if (!tile.is_enabled) continue;

      auto chr = tile.character;
      if (chr >= 0x100) continue;

      bool body = (i >= 0x64 && i <= 0x6f);
      bool fx = (
        // sparkles around sword spin attack AND magic boomerang:
        chr == 0x80 || chr == 0x81 || chr == 0x82 || chr == 0x83 || chr == 0xb7 ||
        // exclusively for spin attack:
        chr == 0x8c || chr == 0x93 || chr == 0xd6 || chr == 0xd7 ||  // chr == 0x92 is also used here
        // sword tink on hard tile when poking:
        chr == 0x90 || chr == 0x92 || chr == 0xb9 ||
        // dash dust
        chr == 0xa9 || chr == 0xcf || chr == 0xdf ||
        // bush leaves
        chr == 0x59 ||
        // item rising from opened chest
        chr == 0x24
      );
      bool weapons = (
        // arrows
        chr == 0x2a || chr == 0x2b || chr == 0x2c || chr == 0x2d ||
        chr == 0x3a || chr == 0x3b || chr == 0x3c || chr == 0x3d ||
        // boomerang
        chr == 0x26 ||
        // magic powder
        chr == 0x09 || chr == 0x0a ||
        // lantern fire
        chr == 0xe3 || chr == 0xf3 || chr == 0xa4 || chr == 0xa5 || chr == 0xb2 || chr == 0xb3 || chr == 0x9c ||
        // push block
        chr == 0x0c ||
        // large stone
        chr == 0x4a ||
        // holding pot / bush or small stone or sign
        chr == 0x46 || chr == 0x44 || chr == 0x42 ||
        // shadow underneath pot / bush or small stone
        (i >= 1 && (ppu::oam[i-1].character == 0x46 || ppu::oam[i-1].character == 0x44) && chr == 0x6c) ||
        // pot shards or stone shards (large and small)
        chr == 0x58 || chr == 0x48
      );
      bool bombs = (
        // explosion:
        chr == 0x84 || chr == 0x86 || chr == 0x88 || chr == 0x8a || chr == 0x8c || chr == 0x9b ||
        // bomb and its shadow:
        (i <= 125 && chr == 0x6e && ppu::oam[i+1].character == 0x6c && ppu::oam[i+2].character == 0x6c) ||
        (i >= 1 && ppu::oam[i-1].character == 0x6e && chr == 0x6c && ppu::oam[i+1].character == 0x6c) ||
        (i >= 2 && ppu::oam[i-2].character == 0x6e && ppu::oam[i-1].character == 0x6c && chr == 0x6c)
      );
      bool follower = (
        chr == 0x20 || chr == 0x22
      );

      // skip OAM sprites that are not related to Link:
      if (!(body || fx || weapons || bombs || follower)) continue;

      // fetch the sprite data from OAM and VRAM:
      Sprite sprite;
      sprite.fetchOAM(i, rx, ry);

      // load character(s) from VRAM:
      if (sprite.size == 0) {
        // 8x8 sprite:
        if (chrs[sprite.chr].length() == 0) {
          chrs[sprite.chr].resize(16);
          ppu::vram.read_block(ppu::vram.chr_address(sprite.chr), 0, 16, chrs[sprite.chr]);
        }
      } else {
        // 16x16 sprite:
        if (chrs[sprite.chr].length() == 0) {
          chrs[sprite.chr + 0x00].resize(16);
          chrs[sprite.chr + 0x01].resize(16);
          chrs[sprite.chr + 0x10].resize(16);
          chrs[sprite.chr + 0x11].resize(16);
          ppu::vram.read_block(ppu::vram.chr_address(sprite.chr + 0x00), 0, 16, chrs[sprite.chr + 0x00]);
          ppu::vram.read_block(ppu::vram.chr_address(sprite.chr + 0x01), 0, 16, chrs[sprite.chr + 0x01]);
          ppu::vram.read_block(ppu::vram.chr_address(sprite.chr + 0x10), 0, 16, chrs[sprite.chr + 0x10]);
          ppu::vram.read_block(ppu::vram.chr_address(sprite.chr + 0x11), 0, 16, chrs[sprite.chr + 0x11]);
        }
      }

      // append the sprite to our array:
      sprites.resize(++numsprites);
      @sprites[numsprites-1] = sprite;
    }

    //message("fetch: numsprites = " + fmtInt(numsprites) + " len = " + fmtInt(sprites.length()));
  }

  uint32 tilemap_addrl;
  uint8 tilemap_valuel;

  // called when tilemap data is updated:
  void tilemap_written(uint32 addr, uint8 value) {
    if (!safe_to_fetch_tilemap()) return;

    // tilemap writes happen in lo-high byte order:
    if ((addr & 1) == 0) {
      tilemap_addrl = addr;
      tilemap_valuel = value;
    } else if ((tilemap_addrl | 1) == addr) {
      // record the 16-bit tilemap write:
      tileWrites.insertLast(TilemapWrite(tilemap_addrl, uint16(tilemap_valuel) | (uint16(value) << 8)));
      tileWritesIndex = tileWrites.length();
    }
    message("sync tilemap [0x" + fmtHex(addr, 6) + "] = 0x" + fmtHex(value, 2));
  }

  // called when VMADDRL and VMADDRH registers are written to:
  void ppu_vmaddr_written(uint32 addr, uint8 value) {
    if (addr == 0x2116) vmaddrl = value;
    else if (addr == 0x2117) vmaddrh = value;
    else message("ppu[0x" + fmtHex(addr, 4) + "] = 0x" + fmtHex(value, 2));
  }

  void dma_interceptor(cpu::DMAIntercept @dma) {
    if (!safe_to_fetch_tilemap()) return;

    // Find DMA writes to tile memory in VRAM:
    if (
      (
        // reading from 0x001006 temporary buffer (for bushes / stones picked up in OW):
        (dma.sourceBank == 0x00 && dma.sourceAddress >= 0x1006 && dma.sourceAddress < 0x1100) ||
        // reading from 0x001100 temporary buffer (for doors in dungeons opening/closing?):
        (dma.sourceBank == 0x00 && dma.sourceAddress >= 0x1100 && dma.sourceAddress < 0x1980)
      ) &&
      // writing to 0x2118, 0x2119 VMDATAL & VMDATAH regs:
      dma.targetAddress == 0x18 &&
      // writing to tilemap VRAM for BG layers:
      vmaddrh < 0x40 &&
      // don't take the larger transfers:
      dma.transferSize < 0x40
    ) {
      // record VRAM transfer for the room:
      auto write = VRAMWrite();
      write.vmaddr = uint16(vmaddrl) | (uint16(vmaddrh) << 8);

      auto size = dma.transferSize >> 1;
      write.data.resize(size);
      uint32 addr = dma.sourceBank << 16 | dma.sourceAddress;
      bus::read_block_u16(addr, 0, size, write.data);

      vramWrites.insertLast(write);
      vramWritesIndex = vramWrites.length();
      message(
        "sync DMA[" + fmtInt(dma.channel) + "] from 0x" + fmtHex(addr, 6) +
        " to PPU 0x" + fmtHex(write.vmaddr, 4) + " size 0x" + fmtHex(dma.transferSize, 4)
      );
    } /* else if (
      // writing to 0x2118, 0x2119 VMDATAL & VMDATAH regs:
      dma.targetAddress == 0x18 &&
      // writing to tilemap VRAM for BG layers:
      vmaddrh < 0x3b
    ) {
      uint32 addr = dma.sourceBank << 16 | dma.sourceAddress;
      auto vmaddr = uint16(vmaddrl) | (uint16(vmaddrh) << 8);
      //message(
      //  "DMA[" + fmtInt(dma.channel) + "] from 0x" + fmtHex(addr, 6) +
      //  " to PPU 0x" + fmtHex(vmaddr, 4) + " size 0x" + fmtHex(dma.transferSize, 4)
      //);
    } */

    // torch lights being animated in dungeons:
    // DMA[0] from 0x7ea680 to PPU 0x3b00 size 0x0400
    // DMA[0] from 0x7eaa80 to PPU 0x3b00 size 0x0400
    // DMA[0] from 0x7eae80 to PPU 0x3b00 size 0x0400

    /* else if (
      // reading from 0x7EB340 temporary buffer for orange/blue tiles:
      dma.sourceBank == 0x7E && dma.sourceAddress >= 0x9000 && dma.sourceAddress < 0xBFFF
    ) {
      uint32 addr = dma.sourceBank << 16 | dma.sourceAddress;
      auto vmaddr = uint16(vmaddrl) | (uint16(vmaddrh) << 8);
      message(
        "DMA[" + fmtInt(dma.channel) + "] from 0x" + fmtHex(addr, 6) +
        " to PPU 0x" + fmtHex(vmaddr, 4) + " size 0x" + fmtHex(dma.transferSize, 4)
      );
    } else {
      uint32 addr = dma.sourceBank << 16 | dma.sourceAddress;
      auto vmaddr = uint16(vmaddrl) | (uint16(vmaddrh) << 8);
      message(
        "DMA[" + fmtInt(dma.channel) + "] from 0x" + fmtHex(addr, 6) +
        " to 0x21" + fmtHex(dma.targetAddress, 2) + " size 0x" + fmtHex(dma.transferSize, 4)
      );
    } */
  }

  void fetch_room_updates() {
    if (!intercepting) {
      // intercept writes to the tilemap:
      bus::add_write_interceptor("7e:2000-3fff", 0, @bus::WriteInterceptCallback(local.tilemap_written));
      bus::add_write_interceptor("7e:4000-5fff", 0, @bus::WriteInterceptCallback(local.tilemap_written));
      bus::add_write_interceptor("00-3f,80-bf:2116-2117", 0, @bus::WriteInterceptCallback(local.ppu_vmaddr_written));
      //bus::add_write_interceptor("00-3f,80-bf:2116-2119", 0, @bus::WriteInterceptCallback(local.ppu_vmaddr_written));
      cpu::register_dma_interceptor(@cpu::DMAInterceptCallback(local.dma_interceptor));

      intercepting = true;
    }
  }

  bool can_see(uint32 other_location) {
    return (location == other_location);
  }

  void serialize(array<uint8> &r) {
    // start with sentinel value to make sure deserializer knows it's the start of a message:
    r.insertLast(uint16(0xFEEF));
    r.insertLast(location);
    r.insertLast(x);
    r.insertLast(y);
    r.insertLast(z);
    r.insertLast(uint8(sprites.length()));
    //message("serialize: numsprites = " + fmtInt(sprites.length()));
    for (uint i = 0; i < sprites.length(); i++) {
      sprites[i].serialize(r);
    }

    // how many distinct characters:
    uint16 chr_count = 0;
    for (uint16 i = 0; i < 512; i++) {
      if (chrs[i].length() == 0) continue;
      ++chr_count;
    }

    // emit how many chrs:
    r.insertLast(chr_count);
    for (uint16 i = 0; i < 512; ++i) {
      if (chrs[i].length() == 0) continue;

      // which chr is it:
      r.insertLast(i);
      // emit the tile data:
      r.insertLast(chrs[i]);

      // clear the chr tile data for next frame:
      chrs[i].resize(0);
    }

    {
      // Serialize current room tilemap writes:
      auto mod_count = uint8(tileWrites.length());
      r.insertLast(mod_count);
      for (uint i = 0; i < mod_count; i++) {
        r.insertLast(uint8(tileWrites[i].addr & 0xFF));
        r.insertLast(uint8((tileWrites[i].addr >> 8) & 0xFF));
        r.insertLast(uint8((tileWrites[i].addr >> 16) & 0xFF));
        r.insertLast(uint8(tileWrites[i].value & 0xFF));
        r.insertLast(uint8((tileWrites[i].value >> 8) & 0xFF));
      }

      // Serialize VRAM updates:
      mod_count = uint8(vramWrites.length());
      r.insertLast(mod_count);
      for (uint i = 0; i < mod_count; i++) {
        r.insertLast(uint8(vramWrites[i].vmaddr & 0xFF));
        r.insertLast(uint8((vramWrites[i].vmaddr >> 8) & 0xFF));
        auto data_len = uint16(vramWrites[i].data.length());
        r.insertLast(uint8((data_len) & 0xFF));
        r.insertLast(uint8((data_len >> 8) & 0xFF));
        r.insertLast(vramWrites[i].data);
      }
    }
  }

  void sendto(string server, int port) {
    // send updated state for our GameState to remote player:
    array<uint8> msg;
    serialize(msg);
    //message("sent " + fmtInt(msg.length()));
    sock.sendto(msg, server, port);
  }

  void receive() {
    array<uint8> r(32768);
    int n;
    while ((n = sock.recv(r)) != 0) {
      int c = 0;

      auto sentinel = uint16(r[c++]) | (uint16(r[c++]) << 8);
      if (sentinel != 0xFEEF) {
        // garbled or split packet
        message("garbled packet, len=" + fmtInt(n));
        return;
      }

      location = uint32(r[c++])
                 | (uint32(r[c++]) << 8)
                 | (uint32(r[c++]) << 16)
                 | (uint32(r[c++]) << 24);

      x = uint16(r[c++]) | (uint16(r[c++]) << 8);
      y = uint16(r[c++]) | (uint16(r[c++]) << 8);
      z = uint16(r[c++]) | (uint16(r[c++]) << 8);

      // read in OAM sprites:
      auto numsprites = r[c++];
      sprites.resize(numsprites);
      for (uint i = 0; i < numsprites; i++) {
        @sprites[i] = Sprite();
        c = sprites[i].deserialize(r, c);
      }

      // clear previous chr tile data:
      for (uint i = 0; i < 512; i++) {
        chrs[i].resize(0);
      }

      // read in chr data:
      auto chr_count = uint16(r[c++]) | (uint16(r[c++]) << 8);
      for (uint i = 0; i < chr_count; i++) {
        // read chr number:
        auto h = uint16(r[c++]) | (uint16(r[c++]) << 8);

        // read chr tile data:
        chrs[h].resize(16);
        for (int k = 0; k < 16; k++) {
          chrs[h][k] = uint16(r[c++]) | (uint16(r[c++]) << 8);
        }
      }

      {
        // Read room tilemap writes:
        auto mod_count = r[c++];
        tileWrites.resize(mod_count);
        for (uint i = 0; i < mod_count; i++) {
          auto addr = uint32(r[c++]) | (uint32(r[c++]) << 8) | (uint32(r[c++]) << 16);
          auto value = uint16(r[c++]) | (uint16(r[c++]) << 8);
          @tileWrites[i] = TilemapWrite(addr, value);
        }

        // Read VRAM updates:
        mod_count = r[c++];
        vramWrites.resize(mod_count);
        for (uint i = 0; i < mod_count; i++) {
          auto write = VRAMWrite();
          write.vmaddr = uint16(r[c++]) | (uint16(r[c++]) << 8);
          auto data_len = uint16(r[c++]) | (uint16(r[c++]) << 8);
          write.data.resize(data_len);
          for (uint j = 0; j < data_len; j++) {
            write.data[j] = uint16(r[c++]) | (uint16(r[c++]) << 8);
          }
          @vramWrites[i] = write;
        }
      }

      //message("deser " + fmtInt(c) + " vs len " + fmtInt(n));
    }
  }

  void overwrite_tile(uint16 addr, array<uint16> tiledata) {
    // read previous VRAM tile:
    array<uint16> backup(16);
    ppu::vram.read_block(addr, 0, 16, backup);

    // overwrite VRAM tile:
    ppu::vram.write_block(addr, 0, 16, tiledata);

    // store backup:
    chr_backup.insertLast(Tile(addr, backup));
  }

  void render(int x, int y) {
    // true/false map to determine which local characters are free for replacement in current frame:
    array<bool> chr(512);
    // lookup remote chr number to find local chr number mapped to:
    array<uint16> reloc(512);
    // assume first 0x100 characters are in-use (Link body, sword, shield, weapons, rupees, etc):
    for (uint j = 0; j < 0x100; j++) {
      chr[j] = true;
    }
    // exclude follower sprite from default assumption of in-use:
    chr[0x20] = false;
    chr[0x21] = false;
    chr[0x30] = false;
    chr[0x31] = false;
    chr[0x22] = false;
    chr[0x23] = false;
    chr[0x32] = false;
    chr[0x33] = false;
    // run through OAM sprites and determine which characters are actually in-use:
    for (uint j = 0; j < 128; j++) {
      auto tile = ppu::oam[j];
      // NOTE: we could skip the is_enabled check which would make the OAM appear to be a LRU cache of characters
      if (!tile.is_enabled) continue;

      // mark chr as used in current frame:
      uint addr = tile.character;
      if (tile.size == 0) {
        // 8x8 tile:
        chr[addr] = true;
      } else {
        // 16x16 tile:
        chr[addr+0x00] = true;
        chr[addr+0x01] = true;
        chr[addr+0x10] = true;
        chr[addr+0x11] = true;
      }
    }

    // add in remote sprites:
    chr_backup.resize(0);
    for (uint i = 0; i < sprites.length(); i++) {
      auto sprite = sprites[i];
      auto px = sprite.size == 0 ? 8 : 16;

      // bounds check for OAM sprites:
      if (sprite.x + x < -px) continue;
      if (sprite.x + x >= 256) continue;
      if (sprite.y + y < -px) continue;
      if (sprite.y + y >= 240) continue;

      // determine which OAM sprite slot is free around the desired index:
      int j;
      for (j = sprite.index; j < 128; j++) {
        if (!ppu::oam[j].is_enabled) break;
      }
      if (j == 128) {
        for (j = sprite.index; j >= 0; j--) {
          if (!ppu::oam[j].is_enabled) break;
        }
        // no more free slots?
        if (j == -1) return;
      }

      // start building a new OAM sprite:
      auto oam = ppu::oam[j];
      oam.x = uint16(sprite.x + x);
      oam.y = sprite.y + y;
      oam.hflip = sprite.hflip;
      oam.vflip = sprite.vflip;
      oam.priority = sprite.priority;
      oam.palette = sprite.palette;
      oam.size = sprite.size;

      // find free character(s) for replacement:
      if (sprite.size == 0) {
        // 8x8 sprite:
        if (reloc[sprite.chr] == 0) { // assumes use of chr=0 is invalid, which it is since it is for local Link.
          for (uint k = 0x20; k < 512; k++) {
            // skip chr if in-use:
            if (chr[k]) continue;

            oam.character = k;
            chr[k] = true;
            reloc[sprite.chr] = k;
            overwrite_tile(ppu::vram.chr_address(k), chrs[sprite.chr]);
            break;
          }
        } else {
          // use existing chr:
          oam.character = reloc[sprite.chr];
        }
      } else {
        // 16x16 sprite:
        if (reloc[sprite.chr] == 0) { // assumes use of chr=0 is invalid, which it is since it is for local Link.
          for (uint k = 0x20; k < 512; k += 2) {
            // skip every odd row since 16x16 are aligned on even rows 0x00, 0x20, 0x40, etc:
            if ((k & 0x10) != 0) continue;
            // skip chr if in-use:
            if (chr[k]) continue;

            oam.character = k;
            chr[k + 0x00] = true;
            chr[k + 0x01] = true;
            chr[k + 0x10] = true;
            chr[k + 0x11] = true;
            reloc[sprite.chr + 0x00] = k + 0x00;
            reloc[sprite.chr + 0x01] = k + 0x01;
            reloc[sprite.chr + 0x10] = k + 0x10;
            reloc[sprite.chr + 0x11] = k + 0x11;
            overwrite_tile(ppu::vram.chr_address(k + 0x00), chrs[sprite.chr + 0x00]);
            overwrite_tile(ppu::vram.chr_address(k + 0x01), chrs[sprite.chr + 0x01]);
            overwrite_tile(ppu::vram.chr_address(k + 0x10), chrs[sprite.chr + 0x10]);
            overwrite_tile(ppu::vram.chr_address(k + 0x11), chrs[sprite.chr + 0x11]);
            break;
          }
        } else {
          // use existing chrs:
          oam.character = reloc[sprite.chr];
        }
      }

      // TODO: do this via NMI and DMA transfers in real hardware (aka not faking it via emulator hacks).
      // update sprite in OAM memory:
      @ppu::oam[j] = oam;
    }
  }

  void updateTilemap() {
    uint i;
    uint len;

    // update tilemap in WRAM:
    len = tileWrites.length();
    //message("tilemap writes " + fmtInt(len));
    for (i = tileWritesIndex; i < len; i++) {
      auto addr = tileWrites[i].addr;
      auto value = tileWrites[i].value;
      message("  wram[0x" + fmtHex(addr, 6) + "] <- 0x" + fmtHex(value, 4));
      bus::write_u8(addr, value & 0xFF);
      bus::write_u8(addr|1, (value >> 8) & 0xFF);
    }
    tileWritesIndex = i;

    // make VRAM updates:
    len = vramWrites.length();
    //message("vram writes " + fmtInt(len));
    for (i = vramWritesIndex; i < len; i++) {
      message("  vram[0x" + fmtHex(vramWrites[i].vmaddr, 4) + "] <- 0x" + fmtHex(vramWrites[i].data.length(), 4) + " words");
      ppu::vram.write_block(
        vramWrites[i].vmaddr,
        0,
        vramWrites[i].data.length(),
        vramWrites[i].data
      );
    }
    vramWritesIndex = i;
  }

  void syncFrom(GameState @remote) {
    // sync tilemap updates:
    auto rlen = remote.tileWrites.length();
    for (uint i = tileWritesIndex; i < rlen; i++) {
      tileWrites.insertLast(remote.tileWrites[i]);
    }

    // sync VRAM updates:
    rlen = remote.vramWrites.length();
    for (uint i = vramWritesIndex; i < rlen; i++) {
      vramWrites.insertLast(remote.vramWrites[i]);
    }
  }
};

GameState local;
GameState remote;
uint8 isRunning;

bool intercepting = false;

void pre_frame() {
  // Wait until the game starts:
  isRunning = bus::read_u8(0x7E0010);
  if (isRunning < 0x06 || isRunning > 0x13) return;

  // Don't do anything until user fills out Settings window inputs:
  if (!settings.started) return;

  // Attempt to open a server socket:
  if (@sock == null) {
    try {
      // open a UDP socket to receive data from:
      @sock = net::UDPSocket(settings.serverIP, 4590);
    } catch {
      // Probably server IP field is invalid; prompt user again:
      @sock = null;
      settings.started = false;
      settings.show();
    }
  }

  // fetch local game state from WRAM:
  local.fetch();

  // receive network update from remote player:
  remote.receive();

  // only draw remote player if location (room, dungeon, light/dark world) is identical to local player's:
  if (local.can_see(remote.location) && local.safe_to_update_tilemap()) {
    // synchronize room state from remote player:
    local.syncFrom(remote);

    // update local tilemap according to remote player's writes:
    local.updateTilemap();

    // subtract BG2 offset from sprite x,y coords to get local screen coords:
    int16 rx = int16(remote.x) - local.xoffs;
    int16 ry = int16(remote.y) - local.yoffs;

    // draw remote player relative to current BG offsets:
    remote.render(rx, ry);
  }

  // send updated state for our Link to player 2:
  local.sendto(settings.clientIP, 4590);

  // load 8 sprite palettes from CGRAM:
  array<array<uint16>> palettes(8, array<uint16>(16));
  for (int i = 0; i < 8; i++) {
    for (int c = 0; c < 16; c++) {
      palettes[i][c] = ppu::cgram[128 + (i << 4) + c];
    }
  }

  array<uint16> fgtiles(0x2000);
  sprites.canvas.fill(0x0000);
  ppu::vram.read_block(0x4000, 0, 0x2000, fgtiles);
  sprites.canvas.draw_sprite_4bpp(0, 0, 0, 128, 256, fgtiles, palettes[7]);
  sprites.update();
}

void post_frame() {
  if (isRunning < 0x06 || isRunning > 0x13) return;

  // Don't do anything until user fills out Settings window inputs:
  if (!settings.started) return;

  // restore previous VRAM tiles:
  auto len = remote.chr_backup.length();
  for (uint i = 0; i < len; i++) {
    ppu::vram.write_block(
      remote.chr_backup[i].addr,
      0,
      16,
      remote.chr_backup[i].tiledata
    );
  }

  ppu::frame.text_shadow = true;

  /*
  // draw some debug info:
  for (int i = 0; i < 16; i++) {
    ppu::frame.text(i * 16, 224-16, fmtHex(bus::read_u8(0x7E0400 + i), 2));
    ppu::frame.text(i * 16, 224- 8, fmtHex(bus::read_u8(0x7E0410 + i), 2));
  }
  */

  /*
  ppu::frame.text( 0, 0, fmtHex(local.ow_screen_transition, 2));
  ppu::frame.text(20, 0, fmtHex(local.module,               2));
  ppu::frame.text(40, 0, fmtHex(local.sub_module,           2));
  ppu::frame.text(60, 0, fmtHex(local.sub_sub_module,       2));
  ppu::frame.text(80, 0, fmtHex(local.to_dark_world,        2));
  */

  /*
  ppu::frame.text( 0, 0, fmtHex(local.state,     2));
  ppu::frame.text(20, 0, fmtHex(local.walking,   1));
  ppu::frame.text(32, 0, fmtHex(local.facing,    1));
  ppu::frame.text(44, 0, fmtHex(local.state_aux, 1));
  ppu::frame.text(56, 0, fmtHex(local.frame,     1));
  ppu::frame.text(68, 0, fmtHex(local.dashing,   1));
  ppu::frame.text(80, 0, fmtHex(local.speed,     2));

  ppu::frame.text(108, 0, fmtHex(bus::read_u8(0x7E00EE), 2));
  ppu::frame.text(128, 0, fmtHex(bus::read_u8(0x7E00EF), 2));
  */
}
