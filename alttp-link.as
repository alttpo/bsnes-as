// ALTTP script to draw current Link sprites on top of rendered frame:
net::UDPSocket@ sock;

enum push_state { push_none = 0, push_start = 1, push_blocked = 2, push_pushing = 3 };

class Link {
  array<array<uint32>> spritedata(2, array<uint32>(32));
  array<array<uint16>> palette(8, array<uint16>(16));

  // values copied from RAM:
  uint32 location;

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
  }

  void receive() {
    array<uint8> r(1024);
    int n;
    if ((n = sock.recv(r)) != 0) {
      // deserialize message into player2 state:
      int c = 0;
      player2.location = uint32(r[c++])
                         | (uint32(r[c++]) << 8)
                         | (uint32(r[c++]) << 16)
                         | (uint32(r[c++]) << 24);

      player2.x = uint16(r[c++]) | (uint16(r[c++]) << 8);
      player2.y = uint16(r[c++]) | (uint16(r[c++]) << 8);

      for (int i = 0; i < 32; i++) {
        if (c+4 > n) return;
        player2.spritedata[0][i] = uint32(r[c++])
                                   | (uint32(r[c++]) << 8)
                                   | (uint32(r[c++]) << 16)
                                   | (uint32(r[c++]) << 24);
      }

      for (int i = 0; i < 32; i++) {
        if (c+4 > n) return;
        player2.spritedata[1][i] = uint32(r[c++])
                                   | (uint32(r[c++]) << 8)
                                   | (uint32(r[c++]) << 16)
                                   | (uint32(r[c++]) << 24);
      }

      for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 16; j++) {
          if (c+2 > n) return;
          player2.palette[i][j] = uint16(r[c++]) | (uint16(r[c++]) << 8);
        }
      }
    }
  }

  void render(int x, int y) {
    // when state = 0x00
    //   standing:
    //     frame = 0
    //   walking:
    //     frame = 1..8
    auto priority = 5 - (level & 1);

    // head:
    auto head = ppu::extra[0];
    switch (link.facing) {
      case 0: head.x = x - 1; break;
      case 2: head.x = x - 1; break;
      case 4: head.x = x + 1; break;
      case 6: head.x = x - 1; break;
    }
    int yo = 0;
    switch (link.frame) {
      case 0: yo = 0; break;
      case 1: yo = 0; break;
      case 2: yo = -1; break;
      case 3: yo = -2; break;
      case 4: yo = 0; break;
      case 5: yo = 0; break;
      case 6: yo = -1; break;
      case 7: yo = -2; break;
      case 8: yo = 0; break;
    }
    head.y = y + yo;
    head.source = 5;
    head.aboveEnable = true;
    head.belowEnable = true;
    head.priority = priority;
    head.width = 16;
    head.height = 16;
    head.pixels_clear();
    if (link.facing == 4) {
      head.hflip = true;
    } else {
      head.hflip = false;
    }
    head.draw_sprite(0, 0, 16, 16, spritedata[0], palette[7]);

    // body:
    auto body = ppu::extra[1];
    switch (link.facing) {
      case 0: body.x = x - 1; break;
      case 2: body.x = x - 1; break;
      case 4: body.x = x; break;
      case 6: body.x = x; break;
    }
    body.y = y + 8;
    body.source = 5;
    body.aboveEnable = true;
    body.belowEnable = true;
    body.priority = priority;
    body.width = 16;
    body.height = 16;
    if (link.facing == 4) {
      body.hflip = true;
    } else {
      body.hflip = false;
    }
    body.pixels_clear();
    body.draw_sprite(0, 0, 16, 16, spritedata[1], palette[7]);
  }
};

Link link;
Link player2;

uint16 baseaddr;

void pre_frame() {
  // TODO for ALTTP:
  // TODO: use animation state + frame to properly position extra-tiles

  // get screen x,y offset by reading BG2 scroll registers:
  auto xoffs = bus::read_u16(0x7E00E2, 0x7E00E3);
  auto yoffs = bus::read_u16(0x7E00E8, 0x7E00E9);

  // fetch our Link's current state from RAM:
  link.fetch();

  // get base address for current tiledata:
  baseaddr = ppu::tile_address(false);

  // load 2x 16x16 sprites for Link from VRAM:
  for (int i = 0; i < 2; i++) {
    ppu::vram.read_sprite(baseaddr, i*2, 16, 16, link.spritedata[i]);
  }

  // load 8 sprite palettes from CGRAM:
  for (int i = 0; i < 8; i++) {
    for (int c = 0; c < 16; c++) {
      link.palette[i][c] = ppu::cgram[128 + (i << 4) + c];
    }
  }

  // receive network update from server:
  //try {
    player2.receive();
  //} catch {}

  // only draw player 2 if location (room, dungeon, light/dark world) is identical to current player's:
  if (player2.location == link.location) {
    // draw Link on screen; overly simplistic drawing code here does not accurately render Link in all poses.
    // need to determine Link's current animation frame from somewhere in RAM.

    // draw 2 extra tiles:
    ppu::extra.count = 2;

    // draw player 2 relative to current BG offsets:
    player2.render(player2.x - xoffs, player2.y - yoffs);
  } else {
    ppu::extra.count = 0;
  }

  // send updated state for our Link to player 2:
  array<uint8> msg;
  msg.insertLast(link.location);
  msg.insertLast(link.x);
  msg.insertLast(link.y);
  msg.insertLast(link.spritedata[0]);
  msg.insertLast(link.spritedata[1]);
  msg.insertLast(link.palette[0]);
  msg.insertLast(link.palette[1]);
  sock.sendto(msg, "127.0.0.2", 4590);
}

void post_frame() {
  /*
  ppu::frame.text_shadow = true;

  ppu::frame.text(0, 0, fmtHex(baseaddr, 4));

  // draw some debug info:
  for (int i = 0; i < 16; i++) {
    ppu::frame.text(i * 16, 224-16, fmtHex(bus::read_u8(0x7E0100 + i), 2));
    ppu::frame.text(i * 16, 224-8, fmtHex(bus::read_u8(0x7E0110 + i), 2));
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
  // open a UDP socket to receive data from:
  @sock = net::UDPSocket("127.0.0.1", 4590);
}
