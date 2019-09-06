// ALTTP script to draw current Link sprites on top of rendered frame:
array<array<uint32>> tiledata(16, array<uint32>(8));
array<array<uint16>> palette(8, array<uint16>(16));
uint8 last_luma = 15;

void pre_frame() {
  last_luma = ppu::luma;

  // load 8 8x8 sprites for Link from VRAM:
  for (int i = 0; i < 8; i++) {
    ppu::obj.character = i;
    for (int y = 0; y < 8; y++) {
      tiledata[0+i][y] = ppu::obj[y];
    }
  }
  for (int i = 0; i < 8; i++) {
    ppu::obj.character = i+16;
    for (int y = 0; y < 8; y++) {
      tiledata[8+i][y] = ppu::obj[y];
    }
  }

  // load 8 sprite palettes from CGRAM:
  for (int i = 0; i < 8; i++) {
    for (int c = 0; c < 16; c++) {
      palette[i][c] = ppu::cgram[128 + (i << 4) + c];
    }
  }
}

int thissprite = 0;

// function to debug OAM data
void post_frame() {
  // set drawing state

  // select 8x8 or 8x16 font for text:
  ppu::frame.font_height = 8;
  // enable shadow under text for clearer reading:
  ppu::frame.text_shadow = true;

  // draw using alpha blending:
  //ppu::frame.draw_op = ppu::draw_op::op_alpha;
  ppu::frame.draw_op = ppu::draw_op::op_solid;
  // alpha is xx/31:
  //ppu::frame.alpha = 20;
  // color is 0x7fff aka white (15-bit RGB)
  ppu::frame.color = ppu::rgb(31, 31, 31);
  // use last luminance value from PPU to draw colors with:
  ppu::frame.luma = last_luma;

  {
    // draw Link on screen; overly simplistic drawing code here does not accurately render Link in all poses.
    // need to determine Link's current animation frame from somewhere in RAM.
    auto link_x = 140, link_y = 12;

    // draw body first:
    ppu::frame.draw_4bpp_8x8(link_x+1, link_y+8, tiledata[2], palette[7]);
    ppu::frame.draw_4bpp_8x8(link_x+9, link_y+8, tiledata[3], palette[7]);
    ppu::frame.draw_4bpp_8x8(link_x+1, link_y+16, tiledata[10], palette[7]);
    ppu::frame.draw_4bpp_8x8(link_x+9, link_y+16, tiledata[11], palette[7]);

    // put head on top of body:
    ppu::frame.draw_4bpp_8x8(link_x+0, link_y+0, tiledata[0], palette[7]);
    ppu::frame.draw_4bpp_8x8(link_x+8, link_y+0, tiledata[1], palette[7]);
    ppu::frame.draw_4bpp_8x8(link_x+0, link_y+8, tiledata[8], palette[7]);
    ppu::frame.draw_4bpp_8x8(link_x+8, link_y+8, tiledata[9], palette[7]);
  }

  return;

  // color test
  ppu::frame.color = ppu::rgb(0, 0, 31);
  ppu::frame.fill(16, 16, 8, 8);

  // draw palette #7 colors:
  for (int c = 0; c < 16; c++) {
    ppu::frame.color = palette[7][c];
    ppu::frame.fill(0 + c*8, 0, 8, 8);
  }

  // detect cgram changes:
  for (int i = 0; i < 8; i++) {
    bool same = true;
    for (int c = 0; c < 16; c++) {
      if (ppu::cgram[128 + (i << 4) + c] != palette[i][c]) {
        same = false;
        break;
      }
    }
    if (!same) {
      message("palette " + fmtInt(i) + " changed!");
    }
  }
}
