// AngelScript for ALTTP to draw white rectangles around in-game sprites
uint16   xoffs, yoffs;
uint16[] sprx(16);
uint16[] spry(16);
uint8[]  sprs(16);
uint8[]  sprk(16);
uint32   location;

void init() {
  // initialize script state here.
  message("hello world!");
}

void pre_frame_1() {
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
  xoffs = bus::read_u16(0x7E00E2, 0x7E00E3);
  yoffs = bus::read_u16(0x7E00E8, 0x7E00E9);

  for (int i = 0; i < 16; i++) {
    // sprite x,y coords are absolute from BG2 top-left:
    spry[i] = bus::read_u16(0x7E0D00 + i, 0x7E0D20 + i);
    sprx[i] = bus::read_u16(0x7E0D10 + i, 0x7E0D30 + i);
    // sprite state (0 = dead, else alive):
    sprs[i] = bus::read_u8(0x7E0DD0 + i);
    // sprite kind:
    sprk[i] = bus::read_u8(0x7E0E20 + i);
  }
}

void post_frame_1() {
  // set drawing state
  // select 8x8 or 8x16 font for text:
  ppu::frame.font_height = 8;
  // draw using alpha blending:
  ppu::frame.draw_op = ppu::draw_op::op_alpha;
  // alpha is xx/31:
  ppu::frame.alpha = 20;
  // color is 0x7fff aka white (15-bit RGB)
  ppu::frame.color = ppu::rgb(31, 31, 31);

  // enable shadow under text for clearer reading:
  ppu::frame.text_shadow = true;

  // draw Link's location value in top-left:
  ppu::frame.text(0, 0, fmtHex(location, 6));

  for (int i = 0; i < 16; i++) {
    // skip dead sprites:
    if (sprs[i] == 0) continue;

    // subtract BG2 offset from sprite x,y coords to get local screen coords:
    int16 rx = int16(sprx[i]) - int16(xoffs);
    int16 ry = int16(spry[i]) - int16(yoffs);

    // draw box around the sprite:
    ppu::frame.rect(rx, ry, 16, 16);

    // draw sprite type value above box:
    ry -= ppu::frame.font_height;
    ppu::frame.text(rx, ry, fmtHex(sprk[i], 2));
  }
}

array<array<uint32>> tiledata(8, array<uint32>(8));
array<array<uint16>> palette(8, array<uint16>(16));

void pre_frame() {
  // load 8 characters for Link from VRAM:
  for (int i = 0; i < 4; i++) {
    ppu::obj.character = i;
    for (int y = 0; y < 8; y++) {
      tiledata[0+i][y] = ppu::obj[y];
    }
  }
  for (int i = 0; i < 4; i++) {
    ppu::obj.character = i+16;
    for (int y = 0; y < 8; y++) {
      tiledata[4+i][y] = ppu::obj[y];
    }
  }

  // load 8 palettes from CGRAM:
  for (int i = 0; i < 8; i++) {
    for (int c = 0; c < 16; c++) {
      palette[i][c] = ppu::cgram[128 + (i << 4) + c];
    }
  }
}

// function to debug OAM data
void post_frame() {
  // set drawing state
  // select 8x8 or 8x16 font for text:
  ppu::frame.font_height = 8;
  // draw using alpha blending:
  //ppu::frame.draw_op = ppu::draw_op::op_alpha;
  ppu::frame.draw_op = ppu::draw_op::op_solid;
  // alpha is xx/31:
  ppu::frame.alpha = 28;
  // color is 0x7fff aka white (15-bit RGB)
  ppu::frame.color = ppu::rgb(31, 31, 31);

  // enable shadow under text for clearer reading:
  ppu::frame.text_shadow = true;

  // draw current character 0 at top-left:
  {
    for (int c = 1; c >= 0; c--) {
      ppu::frame.draw_4bpp_8x8(0, (c * 8), tiledata[(c * 2) + 0], palette[7]);
      ppu::frame.draw_4bpp_8x8(8, (c * 8), tiledata[(c * 2) + 1], palette[7]);
    }
  }

  for (int i = 0; i < 128; i++) {
    /*
    auto b0 = bus::read_u8(0x7E0800 + i*4);
    auto b1 = bus::read_u8(0x7E0801 + i*4);
    auto b2 = bus::read_u8(0x7E0802 + i*4);
    auto b3 = bus::read_u8(0x7E0803 + i*4);
    auto ex = bus::read_u8(0x7E0A00 + i/4);

    uint8 sh = (i&3)*2;
    auto x = uint16(b0) | (uint16((ex >> sh) & 1) << 8);
    auto y = uint16(b1);
    auto s = ((ex >> (sh+1)) & 1) + 1;
    auto c = uint16(b2) | uint16(b3 & 1) << 8;
    auto p = uint8(b3 >> 4) & 7;

    if (x >= 256) x -= 512;
    if ((c & 0xff) != 0) continue;

    ppu::frame.text(x, y-8, fmtHex(p, 1));
    //ppu::frame.rect(x, y, (s*8), (s*8));
    */
    ppu::oam.index = i;

    auto x = ppu::oam.x;
    auto y = ppu::oam.y;
    auto palette = ppu::oam.palette;

    //ppu::frame.text(x, y-8, fmtHex(palette, 1));
  }
}
