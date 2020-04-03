
struct ExtraLayer;

struct ExtraLayer {
  // drawing state:
  uint16 color = 0x7fffu;
  auto get_color() -> uint16 { return color; }
  auto set_color(uint16 color_p) -> void { color = uclamp<15>(color_p); }

  bool text_shadow = false;
  auto get_text_shadow() -> bool { return text_shadow; }
  auto set_text_shadow(bool text_shadow_p) -> void { text_shadow = text_shadow_p; }

  auto measure_text(const string *msg) -> int {
	  const char *c = msg->data();

	  auto len = 0;

	  while (*c != 0) {
		  if ((*c < 0x20) || (*c > 0x7F)) {
			  // Skip the character since it is out of ASCII range:
			  c++;
			  continue;
		  }

		  len++;
		  c++;
	  }

	  return len;
  }

public:
  // tile management:
  auto get_tile_count() -> uint { return ppufast.extraTileCount; }
  auto set_tile_count(uint count) { ppufast.extraTileCount = min(count, 128); }

  // reset all tiles to blank state:
  auto reset() -> void {
	  ppufast.extraTileCount = 0;
	  for (int i = 0; i < 128; i++) {
		  tile_reset(&ppufast.extraTiles[i]);
	  }
  }

  static auto get_tile(ExtraLayer *dummy, uint i) -> PPUfast::ExtraTile* {
	  (void)dummy;
	  if (i >= 128) {
		  asGetActiveContext()->SetException("Cannot access extra[i] where i >= 128", true);
		  return nullptr;
	  }
	  return &ppufast.extraTiles[min(i,128-1)];
  }

public:
  // tile methods:
  static auto tile_reset(PPUfast::ExtraTile *t) -> void {
	  t->x = 0;
	  t->y = 0;
	  t->source = 0;
	  t->hflip = false;
	  t->vflip = false;
	  t->priority = 0;
	  t->width = 0;
	  t->height = 0;
	  tile_pixels_clear(t);
  }

  static auto tile_pixels_clear(PPUfast::ExtraTile *t) -> void {
	  memory::fill<uint16>(t->colors, 1024, 0);
  }

  static auto tile_pixel(PPUfast::ExtraTile *t, int x, int y) -> void;

  static auto tile_pixel_set(PPUfast::ExtraTile *t, int x, int y, uint16 color) -> void {
	  // bounds check:
	  if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;

	  // make sure we're not writing past the end of the colors[] array:
	  uint index = y * t->width + x;
	  if (index >= 1024u) return;

	  // write the pixel with opaque bit set:
	  t->colors[index] = color | 0x8000u;
  }

  static auto tile_pixel_off(PPUfast::ExtraTile *t, int x, int y) -> void {
	  // bounds check:
	  if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;

	  // make sure we're not writing past the end of the colors[] array:
	  uint index = y * t->width + x;
	  if (index >= 1024u) return;

	  // turn off opaque bit:
	  t->colors[index] &= 0x7fffu;
  }

  static auto tile_pixel_on(PPUfast::ExtraTile *t, int x, int y) -> void {
	  // bounds check:
	  if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;

	  // make sure we're not writing past the end of the colors[] array:
	  uint index = y * t->width + x;
	  if (index >= 1024u) return;

	  // turn on opaque bit:
	  t->colors[index] |= 0x8000u;
  }

  static auto tile_draw_sprite(PPUfast::ExtraTile *t, int x, int y, uint8 width, uint8 height, const CScriptArray *tile_data, const CScriptArray *palette_data) -> void {
	  if ((width == 0u) || (width & 7u)) {
		  asGetActiveContext()->SetException("width must be a non-zero multiple of 8", true);
		  return;
	  }
	  if ((height == 0u) || (height & 7u)) {
		  asGetActiveContext()->SetException("height must be a non-zero multiple of 8", true);
		  return;
	  }
	  uint tileWidth = width >> 3u;

	  // Check validity of array inputs:
	  if (tile_data == nullptr) {
		  asGetActiveContext()->SetException("tile_data array cannot be null", true);
		  return;
	  }
	  if (tile_data->GetElementTypeId() != asTYPEID_UINT32) {
		  asGetActiveContext()->SetException("tile_data array must be uint32[]", true);
		  return;
	  }
	  if (tile_data->GetSize() < height * tileWidth) {
		  asGetActiveContext()->SetException("tile_data array must have at least 8 elements", true);
		  return;
	  }

	  if (palette_data == nullptr) {
		  asGetActiveContext()->SetException("palette_data array cannot be null", true);
		  return;
	  }
	  if (palette_data->GetElementTypeId() != asTYPEID_UINT16) {
		  asGetActiveContext()->SetException("palette_data array must be uint16[]", true);
		  return;
	  }
	  if (palette_data->GetSize() < 16) {
		  asGetActiveContext()->SetException("palette_data array must have at least 8 elements", true);
		  return;
	  }

	  for (int py = 0; py < height; py++) {
		  for (int tx = 0; tx < tileWidth; tx++) {
			  auto tile_data_p = static_cast<const uint32 *>(tile_data->At(py * tileWidth + tx));
			  if (tile_data_p == nullptr) {
				  asGetActiveContext()->SetException("tile_data array index out of range", true);
				  return;
			  }

			  uint32 tile = *tile_data_p;
			  for (int px = 0; px < 8; px++) {
				  uint32 c = 0u, shift = 7u - px;
				  c += tile >> (shift + 0u) & 1u;
				  c += tile >> (shift + 7u) & 2u;
				  c += tile >> (shift + 14u) & 4u;
				  c += tile >> (shift + 21u) & 8u;
				  if (c) {
					  auto palette_p = static_cast<const b5g5r5 *>(palette_data->At(c));
					  if (palette_p == nullptr) {
						  asGetActiveContext()->SetException("palette_data array value pointer must not be null", true);
						  return;
					  }

					  tile_pixel_set(t, x + px + (tx << 3), y + py, *palette_p);
				  }
			  }
		  }
	  }
  }

  // draw a horizontal line from x=lx to lx+w on y=ty:
  static auto tile_hline(PPUfast::ExtraTile *t, int lx, int ty, int w) -> void {
	  for (int x = lx; x < lx + w; ++x) {
		  tile_pixel(t, x, ty);
	  }
  }

  // draw a vertical line from y=ty to ty+h on x=lx:
  static auto tile_vline(PPUfast::ExtraTile *t, int lx, int ty, int h) -> void {
	  for (int y = ty; y < ty + h; ++y) {
		  tile_pixel(t, lx, y);
	  }
  }

  // draw a rectangle with zero overdraw (important for op_xor and op_alpha draw ops):
  static auto tile_rect(PPUfast::ExtraTile *t, int x, int y, int w, int h) -> void {
	  tile_hline(t, x, y, w);
	  tile_hline(t, x + 1, y + h - 1, w - 1);
	  tile_vline(t, x, y + 1, h - 1);
	  tile_vline(t, x + w - 1, y + 1, h - 2);
  }

  // fill a rectangle with zero overdraw (important for op_xor and op_alpha draw ops):
  static auto tile_fill(PPUfast::ExtraTile *t, int lx, int ty, int w, int h) -> void {
	  for (int y = ty; y < ty+h; y++)
		  for (int x = lx; x < lx+w; x++)
			  tile_pixel(t, x, y);
  }

  static auto tile_glyph_8(PPUfast::ExtraTile *t, int x, int y, int glyph) -> void;

  // draw a line of text (currently ASCII only due to font restrictions)
  static auto tile_text(PPUfast::ExtraTile *t, int x, int y, const string *msg) -> int {
	  const char *c = msg->data();

	  auto len = 0;

	  while (*c != 0) {
		  if ((*c < 0x20) || (*c > 0x7F)) {
			  // Skip the character since it is out of ASCII range:
			  c++;
			  continue;
		  }

		  int glyph = *c - 0x20;
		  tile_glyph_8(t, x, y, glyph);

		  len++;
		  c++;
		  x += 8;
	  }

	  // return how many characters drawn:
	  return len;
  }
} extraLayer;

auto ExtraLayer::tile_pixel(PPUfast::ExtraTile *t, int x, int y) -> void {
	// bounds check:
	if (x < 0 || y < 0 || x >= t->width || y >= t->height) return;

	// make sure we're not writing past the end of the colors[] array:
	uint index = y * t->width + x;
	if (index >= 1024u) return;

	// write the pixel with opaque bit set:
	t->colors[index] = ScriptInterface::extraLayer.color | 0x8000u;
}

auto ExtraLayer::tile_glyph_8(PPUfast::ExtraTile *t, int x, int y, int glyph) -> void {
	for (int i = 0; i < 8; i++) {
		uint8 m = 0x80u;
		for (int j = 0; j < 8; j++, m >>= 1u) {
			uint8_t row = font8x8_basic[glyph][i];
			uint8 row1 = i < 7 ? font8x8_basic[glyph][i+1] : 0;
			if (row & m) {
				// Draw a shadow at x+1,y+1 if there won't be a pixel set there from the font:
				if (extraLayer.text_shadow && ((row1 & (m>>1u)) == 0u)) {
					tile_pixel_set(t, x + j + 1, y + i + 1, 0x0000);
				}
				tile_pixel(t, x + j, y + i);
			}
		}
	}
}
