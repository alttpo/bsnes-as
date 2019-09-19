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

      @ok = gui::Button();
      ok.text = "Start";
      @ok.on_activate = @gui::ButtonCallback(this.startClicked);
      vl.append(ok, gui::Size(0, 0));
    }
    window.append(vl);
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
  uint8 value;

  TilemapWrite(uint32 addr, uint8 value) {
    this.addr = addr;
    this.value = value;
  }
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

  // for intercepting writes to the tilemap:
  array<TilemapWrite@> frameWrites;
  array<TilemapWrite@> roomWrites;
  bool intercepting;

  void tilemap_written(uint32 addr, uint8 value) {
    // record the tilemap write:
    frameWrites.insertLast(TilemapWrite(addr, value));
  }

  void fetch() {
    // RAM locations ($7E00xx):
    //   $7E0012[0x01] - NMI Boolean
    //       If set to zero, the game will wait at a certain loop until NMI executes. The NMI interrupt generally sets
    //       it to one after it's done so that the game can continue doing things other than updating the screen.
    //   $7E001A[1] = frame counter (increments from 0x00 to 0xff and wraps)
    ctr = bus::read_u8(0x7E001A);
    //   $7E0020[2] = Link's Y coordinate
    y = bus::read_u16(0x7E0020, 0x7E0021);
    //   $7E0022[2] = Link's X coordinate
    x = bus::read_u16(0x7E0022, 0x7E0023);
    //   $7E0024[2] = Link's Z coordinate ($FFFF usually)
    z = bus::read_u16(0x7E0024, 0x7E0025);
    //   $7E0026[1] = Link's walking direction (flags)
    //       still    = 0x00
    //       right    = 0x01
    //       left     = 0x02
    //       down     = 0x04
    //       up       = 0x08
    walking = bus::read_u8(0x7E0026);
    //   $7E002D[1] = animation frame transition counter; when = 0x01, `frame` is advanced
    subframe = bus::read_u8(0x7E002D);
    //   $7E002E[1] = animation steps (cycles 0 to 5 and repeats)
    frame = bus::read_u8(0x7E002E);
    //   $7E002F[1] = Link facing direction
    //       0 = up
    //       2 = down
    //       4 = left
    //       6 = right
    facing = bus::read_u8(0x7E002F);
    //   $7E0048[1] = push state
    //       0x00 - not blocked
    //       0x02 - blocked, not yet pushing
    //       0x01 - transition from blocked to pushing or restarting pushing sequence (moves to 0x03 immediately after)
    //       0x03 - pushing (moves to 0x01 every 32 (33?) frames)
    pushing = bus::read_u8(0x7E0048);
    //   $7E004D[1] = aux Link state (0 = normal, 1 = recoil, 2 = jumping in/out water, 3 = swimming)
    state_aux = bus::read_u8(0x7E004D);
    //   $7E0056[1] = 1 - bunny link, 0 = normal link
    bunny = bus::read_u8(0x7E0056);
    //   $7E0057[1] = movement speed
    speed = int8(bus::read_u8(0x7E0057));
    //   $7E005D[1] = Link's State
    //       0x00 - ground state
    //       0x01 - falling into a hole
    //       0x02 - recoil from hitting wall / enemies
    //       0x03 - spin attacking
    //       0x04 - swimming
    //       0x05 - Turtle Rock platforms
    //       0x06 - recoil again (other movement)
    //       0x07 - hit by Aghanim’s bug zapper
    //       0x08 - using ether medallion
    //       0x09 - using bombos medallion
    //       0x0A - using quake medallion
    //       0x0B - ???
    //       0x0C - ???
    //       0x0D - ???
    //       0x0E - ???
    //       0x0F - ???
    //       0x10 - ???
    //       0x11 - falling off a ledge
    //       0x12 - used when coming out of a dash by pressing a direction other than the dash direction
    //       0x13 - hookshot
    //       0x14 - magic mirror
    //       0x15 - holding up an item
    //       0x16 - asleep in his bed
    //       0x17 - permabunny
    //       0x18 - stuck under a heavy rock
    //       0x19 - Receiving Ether Medallion
    //       0x1A - Receiving Bombos Medallion
    //       0x1B - Opening Desert Palace
    //       0x1C - temporary bunny 538
    //       0x1D - Rolling back from Gargoyle gate or PullForRupees object
    //       0x1E - The actual spin attack motion.
    state = bus::read_u8(0x7E005D);
    //   $7E005E[1] - Link's speed setting
    //       0x00 - normal
    //       0x02 - walking (or dashing) on stairs
    //       0x0c - charging sword spin (0x50 = 1) or holding something
    //       0x10 - dashing
    dashing = bus::read_u8(0x7E005E);
    //   $7E0067[1] = Indicates which direction Link is walking (even if not going anywhere)
    //       JSD: seems to contain the exact same value as `walking` ($26)
    //       bitwise: 0000abcd.
    //           a - Up,
    //           b - Down,
    //           c - Left,
    //           d - Right
    direction = bus::read_u8(0x7E0067);
    //   $7E00EE[1] - In dungeons
    //           0 - means you’re on the upper level
    //           1 - means you’re on a lower level. Important for interaction with different tile types.
    //           3 - walking down stairwell exit (e.g. in castle escape sequence, going down stairs)
    level = bus::read_u8(0x7E00EE);
    //   $7E0308[1] - bit 7 = holding

    // $7E0410 = screen transitioning
    auto screen_transition = bus::read_u8(0x7E0410);

    // Record last screen before transition:
    if (screen_transition == 0) {
      last_location = location;
    }

    // fetch various room indices and flags about where exactly Link currently is:
    auto in_dark_world = bus::read_u8(0x7E0FFF);
    auto in_dungeon = bus::read_u8(0x7E001B);
    auto overworld_room = bus::read_u16(0x7E008A, 0x7E008B);
    auto dungeon_room = bus::read_u16(0x7E00A0, 0x7E00A1);

    auto previous_location = location;

    // compute aggregated location for Link into a single 24-bit number:
    location =
      uint32(in_dark_world & 1) << 17 |
      uint32(in_dungeon & 1) << 16 |
      uint32(in_dungeon != 0 ? dungeon_room : overworld_room);

    // clear out list of room tilemap writes if location changed:
    if (previous_location != location) {
      roomWrites.resize(0);
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

      // skip OAM sprites that are not related to Link:
      if (!(body || fx || weapons || bombs)) continue;

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

  void fetch_room_updates() {
    if (!intercepting) {
      // intercept writes to the tilemap:
      bus::add_write_interceptor("7e:2000-3fff", 0, @bus::WriteInterceptCallback(local.tilemap_written));
      intercepting = true;
    }

    auto frameWritesLen = frameWrites.length();
    if (frameWritesLen > 0) {
      // Append the writes from this frame to the list of writes for the current room:
      if (frameWritesLen <= 32) {
        roomWrites.insertLast(frameWrites);
      }
      frameWrites.resize(0);
    }
  }

  bool can_see(uint32 other_location) {
    if (location == other_location) return true;

    // If the player changed light/dark world or overworld/dungeon then not able to see other player:
    if ((last_location & 0x30000) != (location & 0x30000)) {
      return false;
    }

    return last_location == other_location;
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
      auto mod_count = uint8(roomWrites.length());
      r.insertLast(mod_count);
      for (uint i = 0; i < mod_count; i++) {
        r.insertLast(uint8(roomWrites[i].addr & 0xFF));
        r.insertLast(uint8((roomWrites[i].addr >> 8) & 0xFF));
        r.insertLast(uint8((roomWrites[i].addr >> 16) & 0xFF));
        r.insertLast(roomWrites[i].value);
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
        roomWrites.resize(mod_count);
        for (uint i = 0; i < mod_count; i++) {
          auto addr = uint32(r[c++]) | (uint32(r[c++]) << 8) | (uint32(r[c++]) << 16);
          auto value = r[c++];
          @roomWrites[i] = TilemapWrite(addr, value);
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
    auto len = roomWrites.length();
    message("tilemap writes " + fmtInt(len));
    for (uint i = 0; i < len; i++) {
      auto addr = roomWrites[i].addr;
      auto value = roomWrites[i].value;
      message("  [0x" + fmtHex(addr, 6) + "] <- 0x" + fmtHex(value, 2));
      bus::write_u8(addr, value);
    }
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
    // draw Link on screen; overly simplistic drawing code here does not accurately render Link in all poses.
    // need to determine Link's current animation frame from somewhere in RAM.

    // subtract BG2 offset from sprite x,y coords to get local screen coords:
    int16 rx = int16(remote.x) - local.xoffs;
    int16 ry = int16(remote.y) - local.yoffs;

    // update local tilemap according to remote player's writes:
    remote.updateTilemap();

    // draw remote player relative to current BG offsets:
    remote.render(rx, ry);
  }

  // send updated state for our Link to player 2:
  local.sendto(settings.clientIP, 4590);
}

void post_frame() {
  /*
  ppu::frame.text_shadow = true;

  // draw some debug info:
  for (int i = 0; i < 16; i++) {
    ppu::frame.text(i * 16, 224-16, fmtHex(bus::read_u8(0x7E0400 + i), 2));
    ppu::frame.text(i * 16, 224- 8, fmtHex(bus::read_u8(0x7E0410 + i), 2));
  }
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
