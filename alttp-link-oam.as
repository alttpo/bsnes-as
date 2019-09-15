// ALTTP script to draw current Link sprites on top of rendered frame:
net::UDPSocket@ sock;
SettingsWindow @settings;

class SettingsWindow {
  private gui::Window @window;
  private gui::LineEdit @txtServerIP;
  private gui::LineEdit @txtClientIP;
  private gui::Button @ok;

  string clientIP;
  string serverIP;
  bool started;

  SettingsWindow() {
    @window = gui::Window();
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
        //txtServerIP.visible = true;
        hz.append(txtServerIP, gui::Size(128, 20));
      }
      vl.append(hz, gui::Size(0, 0));

      @hz = gui::HorizontalLayout();
      {
        auto @lbl = gui::Label();
        lbl.text = "Client IP:";
        hz.append(lbl, gui::Size(80, 0));

        @txtClientIP = gui::LineEdit();
        //txtClientIP.visible = true;
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

enum push_state { push_none = 0, push_start = 1, push_blocked = 2, push_pushing = 3 };

array<uint16> link_chrs = {0x00, 0x02, 0x04, 0x05, 0x06, 0x07, 0x15, 0x6c};

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

  // fetches all the OAM sprite data, assumes `ppu::oam.index` is set
  void fetchOAM(int16 rx, int16 ry) {
    index = ppu::oam.index;

    int16 ax = int16(ppu::oam.x);
    int16 ay = int16(ppu::oam.y);

    // adjust x to allow for slightly off-screen sprites:
    if (ax >= 256) ax -= 512;

    // Make sprite x,y relative to incoming rx,ry coordinates (where Link is in screen coordinates):
    x = ax - rx;
    y = ay - ry;

    auto chr = ppu::oam.character;

    width = ppu::oam.width;
    height = ppu::oam.height;

    palette = ppu::oam.palette;
    priority = ppu::oam.priority;
    hflip = ppu::oam.hflip;
    vflip = ppu::oam.vflip;

    // get base address for current tiledata:
    auto baseaddr = ppu::tile_address(ppu::oam.nameselect);

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

    //message("de x=" + fmtInt(x) + " y=" + fmtInt(y) + " w=" + fmtInt(width) + " h=" + fmtInt(height));

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

class Link {
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

  void fetch() {
    // RAM locations ($7E00xx):
    //   $7E0012[0x01] - NMI Boolean
    //       If set to zero, the game will wait at a certain loop until NMI executes. The NMI interrupt generally sets
    //       it to one after it's done so that the game can continue doing things other than updating the screen.
    //   $7E001A[1] = frame counter (increments from 0x00 to 0xff and wraps)
    ctr       = bus::read_u8 (0x7E001A);
    //   $7E0020[2] = Link's Y coordinate
    y         = bus::read_u16(0x7E0020, 0x7E0021);
    //   $7E0022[2] = Link's X coordinate
    x         = bus::read_u16(0x7E0022, 0x7E0023);
    //   $7E0024[2] = Link's Z coordinate ($FFFF usually)
    z         = bus::read_u16(0x7E0024, 0x7E0025);
    //   $7E0026[1] = Link's walking direction (flags)
    //       still    = 0x00
    //       right    = 0x01
    //       left     = 0x02
    //       down     = 0x04
    //       up       = 0x08
    walking   = bus::read_u8 (0x7E0026);
    //   $7E002D[1] = animation frame transition counter; when = 0x01, `frame` is advanced
    subframe  = bus::read_u8 (0x7E002D);
    //   $7E002E[1] = animation steps (cycles 0 to 5 and repeats)
    frame     = bus::read_u8 (0x7E002E);
    //   $7E002F[1] = Link facing direction
    //       0 = up
    //       2 = down
    //       4 = left
    //       6 = right
    facing    = bus::read_u8 (0x7E002F);
    //   $7E0048[1] = push state
    //       0x00 - not blocked
    //       0x02 - blocked, not yet pushing
    //       0x01 - transition from blocked to pushing or restarting pushing sequence (moves to 0x03 immediately after)
    //       0x03 - pushing (moves to 0x01 every 32 (33?) frames)
    pushing   = bus::read_u8 (0x7E0048);
    //   $7E004D[1] = aux Link state (0 = normal, 1 = recoil, 2 = jumping in/out water, 3 = swimming)
    state_aux = bus::read_u8 (0x7E004D);
    //   $7E0056[1] = 1 - bunny link, 0 = normal link
    bunny     = bus::read_u8 (0x7E0056);
    //   $7E0057[1] = movement speed
    speed     = int8(bus::read_u8 (0x7E0057));
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
    state     = bus::read_u8 (0x7E005D);
    //   $7E005E[1] - Link's speed setting
    //       0x00 - normal
    //       0x02 - walking (or dashing) on stairs
    //       0x0c - charging sword spin (0x50 = 1) or holding something
    //       0x10 - dashing
    dashing   = bus::read_u8 (0x7E005E);
    //   $7E0067[1] = Indicates which direction Link is walking (even if not going anywhere)
    //       JSD: seems to contain the exact same value as `walking` ($26)
    //       bitwise: 0000abcd.
    //           a - Up,
    //           b - Down,
    //           c - Left,
    //           d - Right
    direction = bus::read_u8 (0x7E0067);
    //   $7E00EE[1] - In dungeons
    //           0 - means you’re on the upper level
    //           1 - means you’re on a lower level. Important for interaction with different tile types.
    //           3 - walking down stairwell exit (e.g. in castle escape sequence, going down stairs)
    level     = bus::read_u8 (0x7E00EE);
    //   $7E0308[1] - bit 7 = holding

    // $7E0410 = screen transitioning
    auto screen_transition = bus::read_u8 (0x7E0410);

    // Record last screen before transition:
    if (screen_transition == 0) {
      last_location = location;
    }

    // fetch various room indices and flags about where exactly Link currently is:
    auto in_dark_world  = bus::read_u8 (0x7E0FFF);
    auto in_dungeon     = bus::read_u8 (0x7E001B);
    auto overworld_room = bus::read_u16(0x7E008A, 0x7E008B);
    auto dungeon_room   = bus::read_u16(0x7E00A0, 0x7E00A1);

    // compute aggregated location for Link into a single 24-bit number:
    location =
      uint32(in_dark_world & 1) << 17 |
      uint32(in_dungeon & 1) << 16 |
      uint32(in_dungeon != 0 ? dungeon_room : overworld_room);

    // get screen x,y offset by reading BG2 scroll registers:
    xoffs = int16(bus::read_u16(0x7E00E2, 0x7E00E3));
    yoffs = int16(bus::read_u16(0x7E00E8, 0x7E00E9));

    // get link's on-screen coordinates in OAM space:
    int16 rx = int16(x) - xoffs;
    int16 ry = int16(y) - yoffs;

    // read in relevant sprites from OAM and VRAM:
    sprites.resize(0);
    sprites.reserve(8);
    int numsprites = 0;
    for (int i = 0; i < 128; i++) {
      // access current OAM sprite index:
      ppu::oam.index = i;

      // skip OAM sprite if not enabled (X, Y coords are out of display range):
      if (!ppu::oam.is_enabled) continue;

      if (ppu::oam.nameselect) continue;

      // not a Link-related sprite?
      auto chr = ppu::oam.character;
      if (!(
        (chr == 0x6c) || // shadow
        ((chr >= 0x80) && (chr < 0xE0)) || // effects
        ((chr < 0x20) && ((chr & 15) <= 8))
      )) continue;

      auto distx = int16(ppu::oam.x) - rx;
      auto disty = int16(ppu::oam.y) - ry;

      // exclude shadows that are not directly under Link:
      if ((chr == 0x6c) && ((disty < 17) || (disty > 18))) continue;
      if ((chr == 0x6c) && ((distx < -1) || (distx > 9))) continue;

      // fetch the sprite data from OAM and VRAM:
      Sprite sprite;
      sprite.fetchOAM(rx, ry);

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
  }

  void sendto(string server, int port) {
    // send updated state for our Link to player 2:
    array<uint8> msg;
    serialize(msg);
    //message("sent " + fmtInt(msg.length()));
    sock.sendto(msg, server, port);
  }

  void receive() {
    array<uint8> r(4096);
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
      extra.pixels_clear();
      if (extra.height == 0) continue;
      if (extra.width == 0) continue;

      extra.draw_sprite(0, 0, extra.width, extra.height, sprite.tiledata, palettes[sprite.palette & 7]);
    }

    ppu::extra.count = ppu::extra.count + sprites.length();
  }
};

Link link;
Link player2;

void pre_frame() {
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

  // TODO for ALTTP:
  // TODO: use animation state + frame to properly position extra-tiles

  // fetch our Link's current state from RAM:
  link.fetch();

  // receive network update from server:
  player2.receive();

  // reset number of extra tiles to render to 0:
  ppu::extra.count = 0;

  // only draw player 2 if location (room, dungeon, light/dark world) is identical to current player's:
  if (link.can_see(player2.location)) {
    // draw Link on screen; overly simplistic drawing code here does not accurately render Link in all poses.
    // need to determine Link's current animation frame from somewhere in RAM.

    // subtract BG2 offset from sprite x,y coords to get local screen coords:
    int16 rx = int16(player2.x) - link.xoffs;
    int16 ry = int16(player2.y) - link.yoffs;

    // draw player 2 relative to current BG offsets:
    player2.render(rx, ry);
  }

  // send updated state for our Link to player 2:
  link.sendto(settings.clientIP, 4590);
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
  ppu::frame.text( 0, 0, fmtHex(link.state,     2));
  ppu::frame.text(20, 0, fmtHex(link.walking,   1));
  ppu::frame.text(32, 0, fmtHex(link.facing,    1));
  ppu::frame.text(44, 0, fmtHex(link.state_aux, 1));
  ppu::frame.text(56, 0, fmtHex(link.frame,     1));
  ppu::frame.text(68, 0, fmtHex(link.dashing,   1));
  ppu::frame.text(80, 0, fmtHex(link.speed,     2));

  ppu::frame.text(108, 0, fmtHex(bus::read_u8(0x7E00EE), 2));
  ppu::frame.text(128, 0, fmtHex(bus::read_u8(0x7E00EF), 2));
  */
}

void init() {
  @settings = SettingsWindow();
}
