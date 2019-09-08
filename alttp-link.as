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

  {
    // draw Link on screen; overly simplistic drawing code here does not accurately render Link in all poses.
    // need to determine Link's current animation frame from somewhere in RAM.
    auto link_x = 132, link_y = 48;

    // draw 2 extra tiles:
    ppu::extra.count = 2;

    // head:
    ppu::extra[0].x = link_x;
    ppu::extra[0].y = link_y;
    ppu::extra[0].source = 5;
    ppu::extra[0].aboveEnable = true;
    ppu::extra[0].belowEnable = true;
    ppu::extra[0].priority = 5;
    ppu::extra[0].width = 16;
    ppu::extra[0].height = 16;
    ppu::extra[0].pixels_clear();
    ppu::extra[0].draw_sprite(0, 0, tiledata[0], palette[7]);
    ppu::extra[0].draw_sprite(8, 0, tiledata[1], palette[7]);
    ppu::extra[0].draw_sprite(0, 8, tiledata[8], palette[7]);
    ppu::extra[0].draw_sprite(8, 8, tiledata[9], palette[7]);

    // body:
    ppu::extra[1].x = link_x+1;
    ppu::extra[1].y = link_y+8;
    ppu::extra[1].source = 5;
    ppu::extra[1].aboveEnable = true;
    ppu::extra[1].belowEnable = true;
    ppu::extra[1].priority = 5;
    ppu::extra[1].width = 16;
    ppu::extra[1].height = 16;
    ppu::extra[1].pixels_clear();
    ppu::extra[1].draw_sprite(0, 0, tiledata[2], palette[7]);
    ppu::extra[1].draw_sprite(8, 0, tiledata[3], palette[7]);
    ppu::extra[1].draw_sprite(0, 8, tiledata[10], palette[7]);
    ppu::extra[1].draw_sprite(8, 8, tiledata[11], palette[7]);
  }
}
