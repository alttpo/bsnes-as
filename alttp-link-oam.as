// ALTTP script to draw current Link sprites on top of rendered frame:
net::UDPSocket@ sock;
SettingsWindow @settings;

void init() {
  @settings = SettingsWindow();
}

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
    window.visible = true;
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
      vl.append(hz, gui::Size(0, 0));

      @hz = gui::HorizontalLayout();
      {
        @ok = gui::Button();
        ok.text = "Start";
        @ok.on_activate = @gui::ButtonCallback(this.startClicked);
        hz.append(ok, gui::Size(0, 0));

        auto swap = gui::Button();
        swap.text = "Swap";
        @swap.on_activate = @gui::ButtonCallback(this.swapClicked);
        hz.append(swap, gui::Size(0, 0));
      }
      vl.append(hz, gui::Size(0, 0));
    }
    window.append(vl);
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
  int16 x;
  int16 y;
  uint8 width;
  uint8 height;
  uint8 palette;
  uint8 priority;
  bool hflip;
  bool vflip;
  uint8 index;
  array<uint32> tiledata;

  // fetches all the OAM sprite data for OAM sprite at `index`
  void fetchOAM(uint8 index, int16 rx, int16 ry) {
    auto tile = ppu::oam[index];

    int16 ax = int16(tile.x);
    int16 ay = int16(tile.y);

    // adjust x to allow for slightly off-screen sprites:
    if (ax >= 256) ax -= 512;

    // Make sprite x,y relative to incoming rx,ry coordinates (where Link is in screen coordinates):
    x = ax - rx;
    y = ay - ry;

    auto chr = tile.character;

    width  = tile.width;
    height = tile.height;

    palette  = tile.palette;
    priority = tile.priority;
    hflip    = tile.hflip;
    vflip    = tile.vflip;

    // get base address for current tiledata:
    auto baseaddr = ppu::tile_address(tile.nameselect);

    // load character from VRAM:
    int count = ppu::vram.read_sprite(baseaddr, chr, width, height, tiledata);
    //message("count = " + fmtInt(count) + " " + fmtInt(width) + "x" + fmtInt(height));
  }

  void serialize(array<uint8> &r) {
    r.insertLast(uint16(x));
    r.insertLast(uint16(y));
    r.insertLast(width);
    r.insertLast(height);
    r.insertLast(palette);
    r.insertLast(priority);
    r.insertLast(hflip ? uint8(1) : uint8(0));
    r.insertLast(vflip ? uint8(1) : uint8(0));
    r.insertLast(index);
    r.insertLast(tiledata);
  }

  int deserialize(array<uint8> &r, int c) {
    x = int16(uint16(r[c++]) | uint16(r[c++] << 8));
    y = int16(uint16(r[c++]) | uint16(r[c++] << 8));
    width = r[c++];
    height = r[c++];
    palette = r[c++];
    priority = r[c++];
    hflip = (r[c++] != 0 ? true : false);
    vflip = (r[c++] != 0 ? true : false);
    index = r[c++];

    //message("de x=" + fmtInt(x) + " y=" + fmtInt(y) + " w=" + fmtInt(width) + " h=" + fmtInt(height) + " p=" + fmtInt(priority));

    // compute total size of sprite:
    auto count = int(width / 8) * int(height);
    tiledata.resize(count);

    // read in tiledata:
    auto len = int(r.length());
    for (int i = 0; i < count; i++) {
      if (c + 4 > len) return c;
      tiledata[i] = uint32(r[c++])
                    | (uint32(r[c++]) << 8)
                    | (uint32(r[c++]) << 16)
                    | (uint32(r[c++]) << 24);
    }

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
  array<uint8> data;
};

enum push_state { push_none = 0, push_start = 1, push_blocked = 2, push_pushing = 3 };

class GameState {
  // graphics data for current frame:
  array<array<uint16>> palettes(8, array<uint16>(16));
  array<Sprite@> sprites;

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

  bool safe_to_update_tilemap = false;

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

    // determines when it's safe to synchronize tilemap and VRAM:
    safe_to_update_tilemap = (
      ow_screen_transition == 0 &&
      (
        // overworld:
        (module == 0x09 && sub_module == 0x00) ||
        // dungeon:
        (module == 0x07 && sub_module == 0x00)
      )
    );

    // Don't update location until screen transition is complete:
    if (safe_to_update_tilemap) {
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

      if (tile.nameselect) continue;

      auto chr = tile.character;

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
        // holding pot / bush or small stone
        chr == 0x46 || chr == 0x44 ||
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

      //auto distx = int16(tile.x) - rx;
      //auto disty = int16(tile.y) - ry;

      // fetch the sprite data from OAM and VRAM:
      Sprite sprite;
      sprite.fetchOAM(i, rx, ry);

      // append the sprite to our array:
      sprites.resize(++numsprites);
      @sprites[numsprites-1] = sprite;
    }

    //message("fetch: numsprites = " + fmtInt(numsprites) + " len = " + fmtInt(sprites.length()));

    // load 8 sprite palettes from CGRAM:
    for (int i = 0; i < 8; i++) {
      for (int c = 0; c < 16; c++) {
        palettes[i][c] = ppu::cgram[128 + (i << 4) + c];
      }
    }
  }

  uint32 tilemap_addrl;
  uint8 tilemap_valuel;

  // called when tilemap data is updated:
  void tilemap_written(uint32 addr, uint8 value) {
    if (!safe_to_update_tilemap) return;

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
    if (!safe_to_update_tilemap) return;

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
      uint32 addr = dma.sourceBank << 16 | dma.sourceAddress;
      bus::read_block(addr, dma.transferSize, write.data);
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
    for (uint i = 0; i < 8; i++) {
      r.insertLast(palettes[i]);
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

      auto numsprites = r[c++];

      //message("recv(" + fmtInt(n) + "): numsprites = " + fmtInt(numsprites));

      sprites.resize(numsprites);
      for (uint i = 0; i < numsprites; i++) {
        @sprites[i] = Sprite();
        c = sprites[i].deserialize(r, c);
      }

      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 16; j++) {
          palettes[i][j] = uint16(r[c++]) | (uint16(r[c++]) << 8);
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
            write.data[j] = r[c++];
          }
          @vramWrites[i] = write;
        }
      }

      //message("deser " + fmtInt(c) + " vs len " + fmtInt(n));
    }
  }

  void render(int x, int y) {
    for (uint i = 0; i < sprites.length(); i++) {
      auto sprite = sprites[i];
      //if (sprite is null) continue;

      auto extra = ppu::extra[i];
      extra.x = sprite.x + x;
      extra.y = sprite.y + y;
      // select OBJ1 or OBJ2 depending on palette
      extra.source = sprite.palette < 4 ? 4 : 5;
      extra.priority = sprite.priority;
      extra.width = sprite.width;
      extra.height = sprite.height;
      extra.hflip = sprite.hflip;
      extra.vflip = sprite.vflip;
      extra.index = sprite.index;
      extra.pixels_clear();
      if (extra.height == 0) continue;
      if (extra.width == 0) continue;

      extra.draw_sprite(0, 0, extra.width, extra.height, sprite.tiledata, palettes[sprite.palette & 7]);
    }

    ppu::extra.count = ppu::extra.count + sprites.length();
  }

  void updateTilemap() {
    // update tilemap in WRAM:
    auto len = tileWrites.length();
    message("tilemap writes " + fmtInt(len));
    uint i;
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
    message("vram writes " + fmtInt(len));
    for (i = vramWritesIndex; i < len; i++) {
      message("  vram[0x" + fmtHex(vramWrites[i].vmaddr, 4) + "] <- ...data...");
      ppu::write_data(
        vramWrites[i].vmaddr,
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
    //tileWritesIndex = rlen;

    // sync VRAM updates:
    rlen = remote.vramWrites.length();
    for (uint i = vramWritesIndex; i < rlen; i++) {
      vramWrites.insertLast(remote.vramWrites[i]);
    }
    //vramWritesIndex = rlen;
  }
};

GameState local;
GameState remote;

bool intercepting = false;

void pre_frame() {
  // Wait until the game starts:
  auto isRunning = bus::read_u8(0x7E0010);
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

  // reset number of extra tiles to render to 0:
  ppu::extra.count = 0;

  // only draw remote player if location (room, dungeon, light/dark world) is identical to local player's:
  if (local.can_see(remote.location)) {
    // synchronize room state from remote player:
    local.syncFrom(remote);

    // update local tilemap according to remote player's writes:
    if (local.safe_to_update_tilemap) {
      local.updateTilemap();
    }

    // draw Link on screen; overly simplistic drawing code here does not accurately render Link in all poses.
    // need to determine Link's current animation frame from somewhere in RAM.

    // subtract BG2 offset from sprite x,y coords to get local screen coords:
    int16 rx = int16(remote.x) - local.xoffs;
    int16 ry = int16(remote.y) - local.yoffs;

    // draw remote player relative to current BG offsets:
    remote.render(rx, ry);
  }

  // send updated state for our Link to player 2:
  local.sendto(settings.clientIP, 4590);
}

void post_frame() {
  ppu::frame.text_shadow = true;

  /*
  // draw some debug info:
  for (int i = 0; i < 16; i++) {
    ppu::frame.text(i * 16, 224-16, fmtHex(bus::read_u8(0x7E0400 + i), 2));
    ppu::frame.text(i * 16, 224- 8, fmtHex(bus::read_u8(0x7E0410 + i), 2));
  }
  */

  ppu::frame.text( 0, 0, fmtHex(local.ow_screen_transition, 2));
  ppu::frame.text(20, 0, fmtHex(local.module,               2));
  ppu::frame.text(40, 0, fmtHex(local.sub_module,           2));
  ppu::frame.text(60, 0, fmtHex(local.sub_sub_module,       2));
  ppu::frame.text(80, 0, fmtHex(local.to_dark_world,        2));

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
