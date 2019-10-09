// Communicate with ROM hack via memory at $7F7667[0x6719]
net::UDPSocket@ sock;
SettingsWindow@ settings;

bool debug = false;
bool debugOAM = false;

void init() {
  @settings = SettingsWindow();
  @sprites = SpritesWindow();
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

const int scale = 2;
class SpritesWindow {
  private gui::Window @window;
  private gui::VerticalLayout @vl;
  gui::Canvas @canvas;

  array<uint16> fgtiles(0x1000);

  SpritesWindow() {
    // relative position to bsnes window:
    @window = gui::Window(0, 256*2, true);
    window.title = "Sprite VRAM";
    window.size = gui::Size(256*scale, 256*scale);

    @vl = gui::VerticalLayout();
    window.append(vl);

    @canvas = gui::Canvas();
    canvas.size = gui::Size(128, 128);
    vl.append(canvas, gui::Size(-1, -1));

    vl.resize();
    canvas.update();
    window.visible = true;
  }

  void update() {
    canvas.update();
  }

  void render(const array<uint16> &palette) {
    // read VRAM:
    ppu::vram.read_block(0x4000, 0, 0x1000, fgtiles);

    // draw VRAM as 4bpp tiles:
    sprites.canvas.fill(0x0000);
    sprites.canvas.draw_sprite_4bpp(0, 0, 0, 128, 128, fgtiles, palette);
  }
};
SpritesWindow @sprites;
array<uint16> palette7(16);

const uint packet_total_size = 42 + (12 * 4) + (12 * 1);

class OAMSprite {
  uint8 b0; // xxxxxxxx
  uint8 b1; // yyyyyyyy
  uint8 b2; // cccccccc
  uint8 b3; // vhoopppc
  uint8 b4; // ------sx

  OAMSprite() {}

  int16 x{get const { return int16(b0) | (int16(b4 & 1) << 8); }};
  uint8 y{get const { return b1; }};
  uint16 chr{get const { return uint16(b2) | ((uint16(b3) & 1) << 8); }};
  uint8 palette{get const { return (b3 >> 1) & 7; }};
  uint8 priority{get const { return (b3 >> 4) & 3; }};
  uint8 hflip{get const { return (b3 >> 6) & 1; }};
  uint8 vflip{get const { return (b3 >> 7) & 1; }};
  uint8 size{get const { return (b4 & 2) >> 1; }};

  void serialize(array<uint8> &r) {
    r.insertLast(b0);
    r.insertLast(b1);
    r.insertLast(b2);
    r.insertLast(b3);
  }
  void serialize_ext(array<uint8> &r) {
    r.insertLast(b4);
  }

  int deserialize(array<uint8> &r, int c) {
    b0 = r[c++];
    b1 = r[c++];
    b2 = r[c++];
    b3 = r[c++];
    return c;
  }
  int deserialize_ext(array<uint8> &r, int c) {
    b4 = r[c++];
    return c;
  }
};

class Packet {
  // WRAM address (including bank) where this packet is read from or written to:
  uint32 addr;

  // first bytes of data packet:
  uint32 location;
  uint16 x, y, z;
  uint16 xoffs, yoffs;

  // visual aspects of player taken from SRAM at $7EFxxx:
  uint8 sword;  // $359
  uint8 shield; // $35A
  uint8 armor;  // $35B

  // WRAM locations for source addresses of animated sprite tile data:
  // bank:[addr] where bank is direct and [addr] is indirect

  // $10:[$0ACE] -> $4100 (0x40 bytes) (bottom of head)
  // $10:[$0AD2] -> $4120 (0x40 bytes) (bottom of body)
  // $10:[$0AD6] -> $4140 (0x20 bytes) (bottom sweat/arm/hand)

  // $10:[$0ACC] -> $4000 (0x40 bytes) (top of head)
  // $10:[$0AD0] -> $4020 (0x40 bytes) (top of body)
  // $10:[$0AD4] -> $4040 (0x20 bytes) (top sweat/arm/hand)

  // bank $7E (WRAM) is used to store decompressed 3bpp->4bpp tile data

  // $7E:[$0AC0] -> $4050 (0x40 bytes) (top of sword slash)
  // $7E:[$0AC4] -> $4070 (0x40 bytes) (top of shield)
  // $7E:[$0AC8] -> $4090 (0x40 bytes) (Zz sprites or bugnet top)
  // $7E:[$0AE0] -> $40B0 (0x20 bytes) (top of rupee)
  // $7E:[$0AD8] -> $40C0 (0x40 bytes) (top of movable block)

  // only if bird is active
  // $7E:[$0AF6] -> $40E0 (0x40 bytes) (top of hammer sprites)

  // $7E:[$0AC2] -> $4150 (0x40 bytes) (bottom of sword slash)
  // $7E:[$0AC6] -> $4170 (0x40 bytes) (bottom of shield)
  // $7E:[$0ACA] -> $4190 (0x40 bytes) (music note sprites or bugnet bottom)
  // $7E:[$0AE2] -> $41B0 (0x20 bytes) (bottom of rupee)
  // $7E:[$0ADA] -> $41C0 (0x40 bytes) (bottom of movable block)

  // only if bird is active
  // $7E:[$0AF8] -> $41E0 (0x40 bytes) (bottom of hammer sprites)

  // words from $0AC0..$0ACA:
  array<uint16> dma7E_addr(6);
  // words from $0ACC..$0AD6:
  array<uint16> dma10_addr(6);

  uint8 oam_count;
  array<OAMSprite> oam_table(12);

  Packet(uint32 addr) {
    this.addr = addr;
  }

  void readRAM() {
    // read entire packet from WRAM into script memory:
    array<uint8> r(packet_total_size);
    bus::read_block_u8(addr, 0, packet_total_size, r);

    deserialize(r, 0);
  }

  void writeRAM() {
    // serialize packet into a byte array:
    array<uint8> r;
    serialize(r);

    // copy byte array directly into WRAM:
    bus::write_block_u8(addr, 0, r.length(), r);
  }

  void serialize(array<uint8> &r) {
    r.insertLast(location);
    r.insertLast(x);
    r.insertLast(y);
    r.insertLast(z);
    r.insertLast(xoffs);
    r.insertLast(yoffs);
    r.insertLast(sword);
    r.insertLast(shield);
    r.insertLast(armor);
    r.insertLast(dma7E_addr);
    r.insertLast(dma10_addr);

    r.insertLast(oam_count);
    for (uint i = 0; i < 12; i++) {
      oam_table[i].serialize(r);
    }
    for (uint i = 0; i < 12; i++) {
      oam_table[i].serialize_ext(r);
    }
  }

  int deserialize(array<uint8> &r, int c) {
    location = uint32(r[c++])
               | (uint32(r[c++]) << 8)
               | (uint32(r[c++]) << 16)
               | (uint32(r[c++]) << 24);

    x = uint16(r[c++]) | (uint16(r[c++]) << 8);
    y = uint16(r[c++]) | (uint16(r[c++]) << 8);
    z = uint16(r[c++]) | (uint16(r[c++]) << 8);

    xoffs = uint16(r[c++]) | (uint16(r[c++]) << 8);
    yoffs = uint16(r[c++]) | (uint16(r[c++]) << 8);

    sword  = r[c++];
    shield = r[c++];
    armor  = r[c++];

    for (uint i = 0; i < 6; i++) {
      dma7E_addr[i] = uint16(r[c++]) | (uint16(r[c++]) << 8);
    }
    for (uint i = 0; i < 6; i++) {
      dma10_addr[i] = uint16(r[c++]) | (uint16(r[c++]) << 8);
    }

    oam_count = r[c++];
    for (uint i = 0; i < 12; i++) {
      c = oam_table[i].deserialize(r, c);
    }
    for (uint i = 0; i < 12; i++) {
      c = oam_table[i].deserialize_ext(r, c);
    }

    return c;
  }

  void sendto(string server, int port) {
    // send updated state for our GameState to remote player:
    array<uint8> msg;

    // message header outside data packet:
    msg.insertLast(uint16(0xFEEF)); // FEEF identifier
    msg.insertLast(uint16(0));      // message version

    // write the data packet into the message:
    serialize(msg);

    //message("sent " + fmtInt(msg.length()));
    sock.sendto(msg, server, port);
  }

  void receive() {
    array<uint8> r(9500);
    int n;
    while ((n = sock.recv(r)) != 0) {
      int c = 0;

      // read message header:
      auto feef = uint16(r[c++]) | (uint16(r[c++]) << 8);
      if (feef != 0xFEEF) {
        message("received bad message header!");
        continue;
      }

      auto version = uint16(r[c++]) | (uint16(r[c++]) << 8);
      if (version != 0) {
        message("received message version higher than expected!");
        continue;
      }

      // deserialize data packet from message:
      c = deserialize(r, c);
    }
  }

  bool can_see(Packet &other) {
    return this.location == other.location;
  }
};

Packet  local(0x7F7700);
Packet remote(0x7F8200);

uint8 module, sub_module;

void pre_frame() {
  if (sprites != null) {
    for (int i = 0; i < 16; i++) {
      palette7[i] = ppu::cgram[(15 << 4) + i];
    }
    sprites.render(palette7);
    sprites.update();
  }

  // Fetch local state from game RAM:
  local.readRAM();

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

  if (@sock != null) {
    // receive network update from remote player:
    remote.receive();

    // upload remote packet into local WRAM:
    remote.writeRAM();

    // send updated state for our Link to remote player:
    local.sendto(settings.clientIP, 4590);
  }
}

void post_frame() {
  module     = bus::read_u8(0x7E0010);
  sub_module = bus::read_u8(0x7E0011);

  if (debug) {
    ppu::frame.text_shadow = true;
    ppu::frame.color = 0x7fff;
    ppu::frame.alpha = 28;

    // module/sub_module:
    ppu::frame.text(  0, 0, fmtHex(module, 2));
    ppu::frame.text( 20, 0, fmtHex(sub_module, 2));

    // read local packet composed during NMI:
    ppu::frame.text(  0, 8, fmtHex(local.location, 6));
    ppu::frame.text( 52, 8, fmtHex(local.x, 4));
    ppu::frame.text( 88, 8, fmtHex(local.y, 4));
    ppu::frame.text(124, 8, fmtHex(local.z, 4));
    ppu::frame.text(160, 8, fmtHex(local.xoffs, 4));
    ppu::frame.text(196, 8, fmtHex(local.yoffs, 4));

    if (debugOAM) {
      // draw DMA source addresses:
      for (uint i = 0; i < 3; i++) {
        ppu::frame.text((i + 3) * (4 * 8 + 4), 224 - 8, fmtHex(remote.dma10_addr[i * 2 + 0], 4));
        ppu::frame.text((i + 3) * (4 * 8 + 4), 224 - 16, fmtHex(remote.dma10_addr[i * 2 + 1], 4));

        ppu::frame.text((i + 0) * (4 * 8 + 4), 224 - 8, fmtHex(remote.dma7E_addr[i * 2 + 0], 4));
        ppu::frame.text((i + 0) * (4 * 8 + 4), 224 - 16, fmtHex(remote.dma7E_addr[i * 2 + 1], 4));
      }

      // limited to 12
      auto len = remote.oam_count;
      ppu::frame.text(0, 16, fmtHex(len, 2));
      if (len <= 12) {
        for (uint i = 0; i < len; i++) {
          auto y = 224 - 16 - ((len - i) * 8);
          //ppu::frame.text( 0, y, fmtHex(local.oam_table[i].b0, 2));
          //ppu::frame.text(20, y, fmtHex(local.oam_table[i].b1, 2));
          //ppu::frame.text(40, y, fmtHex(local.oam_table[i].b2, 2));
          //ppu::frame.text(60, y, fmtHex(local.oam_table[i].b3, 2));
          //ppu::frame.text(80, y, fmtHex(local.oam_table[i].b4, 1));

          ppu::frame.text(100, y, fmtHex(local.oam_table[i].x, 3));
          ppu::frame.text(130, y, fmtHex(local.oam_table[i].y, 2));
          ppu::frame.text(150, y, fmtHex(local.oam_table[i].chr, 3));
          ppu::frame.text(180, y, fmtHex(local.oam_table[i].palette, 1));
          ppu::frame.text(190, y, fmtHex(local.oam_table[i].priority, 1));
          ppu::frame.text(200, y, fmtBinary(local.oam_table[i].hflip, 1));
          ppu::frame.text(210, y, fmtBinary(local.oam_table[i].vflip, 1));
        }
      }
    }
  }
}
