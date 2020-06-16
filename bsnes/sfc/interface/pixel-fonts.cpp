
#include <nall/nall.hpp>
#include <sfc/resource/resource.hpp>

namespace PixelFonts {

#include "vga-charset.cpp"

using nall::array_view;
using nall::function;
using nall::string;
using nall::vector;
using nall::map;

// public interface for a Font:
struct Font {
  virtual auto drawGlyph(uint32_t r, const function<void(int x, int y)> &px) -> int = 0;
  virtual auto width(uint32_t r) -> uint = 0;
  virtual auto height() -> uint = 0;
};

struct VGAFont : Font {
  VGAFont(const uint8_t *bitmap, uint height) : _bitmap(bitmap), _height(height) {}

  auto drawGlyph(uint32_t r, const function<void(int x, int y)> &px) -> int final {
    if (r < 0x20) return 0;
    if (r > 0x7f) return 0;

    const uint8_t *bitmap = _bitmap + ((r - 0x20) * _height);
    for (int i = 0; i < _height; i++) {
      uint8_t m = 0x80u;
      for (int j = 0; j < 8; j++, m >>= 1u) {
        uint8_t row = bitmap[i];
        if (row & m) {
          px(j, i);
        }
      }
    }

    return 8;
  }

  auto width(uint32_t r) -> uint final { return 8; }
  auto height() -> uint final { return _height; }

private:
  const uint8_t *_bitmap;
  uint _height;
};

struct PCFFont : Font {
  #define PCF_PROPERTIES          (1<<0)
  #define PCF_ACCELERATORS        (1<<1)
  #define PCF_METRICS             (1<<2)
  #define PCF_BITMAPS             (1<<3)
  #define PCF_INK_METRICS         (1<<4)
  #define PCF_BDF_ENCODINGS       (1<<5)
  #define PCF_SWIDTHS             (1<<6)
  #define PCF_GLYPH_NAMES         (1<<7)
  #define PCF_BDF_ACCELERATORS    (1<<8)

  #define PCF_DEFAULT_FORMAT      0x00000000
  #define PCF_INKBOUNDS           0x00000200
  #define PCF_ACCEL_W_INKBOUNDS   0x00000100
  #define PCF_COMPRESSED_METRICS  0x00000100
  #define PCF_BYTE_MASK           (1<<2)

  explicit PCFFont(array_view<uint8_t> file_p) : file(file_p) {
    parse_pcf();
  }

  auto drawGlyph(uint32_t r, const function<void(int x, int y)> &px) -> int final {
    auto i = encoding.find_index(r);
    if (i == 0xffff) return 0;

    const auto &me = metrics.entries[i];

    // bitmap bits are stored in 0=bytes, 1=shorts, 2=ints:
    auto elem = 1 << ((bitmaps.format >> 4) & 3); // 1,  2,  4
    auto elemBits = elem << 3;                    // 8, 16, 32
    auto kmax = elemBits - 1;                     // 7, 15, 31

    // stride (in bytes) between rows:
    auto stride = 1 << (bitmaps.format & 3);
    // bitmap data:
    auto data = bitmaps.bitmap(i);

    // read functions:
    auto p = data;
    auto read = (bitmaps.table_format & PCF_BYTE_MASK)
                ? function<uint64_t (uint)>([&](uint size){ return p.readm(size); })
                : function<uint64_t (uint)>([&](uint size){ return p.readl(size); });

    int y = 0;
    for (int i = 0; i < data.size(); i += stride, y++) {
      int x = 0;
      for (int j = 0; j < stride; j += elem) {
        uint64_t bits = read(elem);
        for (int k = kmax; k >= 0; k--, x++) {
          if (j * elemBits + kmax - k > me.character_width) { continue; }
          if (bits & (1<<k)) {
            px(x, y);
          }
        }
      }
    }

    return me.character_width;
  }

  auto width(uint32_t r) -> uint final {
    auto i = encoding.find_index(r);
    if (i == 0xffff) return 0;

    return metrics.entries[i].character_width;
  }
  auto height() -> uint final {
    auto stride = 1 << (bitmaps.format & 3);
    return bitmaps.bitmap(0).size() / stride;
  }

private:
  const array_view<uint8_t> file;

  void parse_pcf() {
    // make a copy of the array_view so we can read from the start without losing the start offset:
    auto p = file;

    // read the header:
    p.readl(4);
    auto table_count = p.readl(4);
    for (int i = 0; i < table_count; i++) {
      uint32_t type   = p.readl(4); /* indicates which table */
      uint32_t format = p.readl(4);	/* indicates how the data are formatted in the table */
      uint32_t size   = p.readl(4); /* In bytes */
      uint32_t offset = p.readl(4); /* from start of file */

      switch (type) {
        case PCF_METRICS:
          metrics = Metrics(file.view(offset, size), format);
          break;
        case PCF_BITMAPS:
          bitmaps = Bitmaps(file.view(offset, size), format);
          break;
        case PCF_BDF_ENCODINGS:
          encoding = Encoding(file.view(offset, size), format);
          break;
      }
    }
  }

  struct Metrics {
    array_view<uint8_t> table;
    uint32_t table_format;

    int32_t format;
    int32_t count;

    struct Entry {
      int16_t left_sided_bearing;
      int16_t right_side_bearing;
      int16_t character_width;
      int16_t character_ascent;
      int16_t character_descent;
      uint16_t character_attributes;
    };
    vector<Entry> entries;

    Metrics() {}
    Metrics(array_view<uint8_t> table_p, uint32_t table_format_p)
      : table(table_p), table_format(table_format_p)
    {
      auto p = table;
      format = p.readl(4);

      auto read = (table_format & PCF_BYTE_MASK)
                  ? function<uint64_t (uint)>([&](uint size){ return p.readm(size); })
                  : function<uint64_t (uint)>([&](uint size){ return p.readl(size); });

      if (table_format & PCF_COMPRESSED_METRICS) {
        count = read(2);

        entries.resize(count);
        for (int i = 0; i < count; i++) {
          auto &me = entries[i];
          me.left_sided_bearing = p.read() - 0x80;
          me.right_side_bearing = p.read() - 0x80;
          me.character_width = p.read() - 0x80;
          me.character_ascent = p.read() - 0x80;
          me.character_descent = p.read() - 0x80;
        }
      } else {
        count = read(2);

        printf("%d uncompressed metrics\n", count);
        entries.resize(count);
        for (int i = 0; i < count; i++) {
          auto &me = entries[i];
          me.left_sided_bearing = read(2);
          me.right_side_bearing = read(2);
          me.character_width = read(2);
          me.character_ascent = read(2);
          me.character_descent = read(2);
          me.character_attributes = read(2);
        }
      }
    }
  } metrics;

  struct Bitmaps {
    array_view<uint8_t> table;
    uint32_t table_format;

    int32_t format;
    int32_t count;
    vector<int32_t> offsets;
    int32_t bitmapSizes[4];
    array_view<uint8_t> bitmapData;

    Bitmaps() {}
    Bitmaps(array_view<uint8_t> table_p, uint32_t table_format_p)
      : table(table_p), table_format(table_format_p)
    {
      auto p = table;
      format = p.readl(4);

      auto read = (table_format & PCF_BYTE_MASK)
                  ? function<uint64_t (uint)>([&](uint size){ return p.readm(size); })
                  : function<uint64_t (uint)>([&](uint size){ return p.readl(size); });

      count = read(4);
      offsets.resize(count);
      for (int i = 0; i < count; i++) {
        offsets[i] = read(4);
      }
      for (int i = 0; i < 4; i++) {
        bitmapSizes[i] = read(4);
      }

      bitmapData = p;
    }

    auto bitmap(uint i) -> array_view<uint8_t> {
      if (i < 0) return {};
      if (i >= count) return {};

      auto offs = offsets[i];
      uint end;
      if (i + 1 >= count) {
        end = bitmapSizes[format & 3];
      } else {
        end = offsets[i + 1];
      }
      return bitmapData.view(offs, end - offs);
    }

  } bitmaps;

  struct Encoding {
    array_view<uint8_t> table;
    uint32_t table_format;

    int32_t format;
    int16_t minCharOrByte2;
    int16_t maxCharOrByte2;
    int16_t minByte1;
    int16_t maxByte1;
    int16_t defChar;
    vector<int16_t> indexes;
    int32_t index_count;
    bool direct;

    Encoding() {}
    Encoding(array_view<uint8_t> table_p, uint32_t table_format_p)
      : table(table_p), table_format(table_format_p)
    {
      auto p = table;
      format = p.readl(4);

      auto read = (table_format & PCF_BYTE_MASK)
                  ? function<uint64_t (uint)>([&](uint size){ return p.readm(size); })
                  : function<uint64_t (uint)>([&](uint size){ return p.readl(size); });

      minCharOrByte2 = read(2);
      maxCharOrByte2 = read(2);
      minByte1 = read(2);
      maxByte1 = read(2);
      defChar = read(2);

      index_count = (maxCharOrByte2 - minCharOrByte2 + 1) * (maxByte1 - minByte1 + 1);
      indexes.resize(index_count);
      for (int i = 0; i < index_count; i++) {
        indexes[i] = read(2);
      }

      direct = ((maxCharOrByte2 - minCharOrByte2) == 0xff && (maxByte1 - minByte1) == 0xff);
    }

    auto find_index(uint32_t r) -> uint32_t {
      if (direct) {
        if (r >= index_count) return 0xffff;
        return indexes[r];
      }

      uint8_t b1 = 0xff & r, b2 = (r >> 8) & 0xff;
      uint32_t off = 0;
      if (b2 == 0) {
        off = b1 - minCharOrByte2;
      } else {
        off = (b2 - (uint32_t)minByte1) * (maxCharOrByte2 - minCharOrByte2 + 1) + (b1 - minCharOrByte2);
      }

      if (off >= index_count) return 0xffff;
      return indexes[off];
    }
  } encoding;
};

struct Fonts {
  Fonts() {
    fonts.insert("vga8", new VGAFont(reinterpret_cast<const uint8_t *>(font8x8_basic), 8));
    fonts.insert("vga16", new VGAFont(reinterpret_cast<const uint8_t *>(font8x16_basic), 16));
    fonts.insert("kakwa", new PCFFont({kakwa, sizeof(kakwa)}));
    fonts.insert("proggy-tinysz", new PCFFont({proggy_tinysz, sizeof(proggy_tinysz)}));
  }

  auto operator[](const string& name) -> Font* { return fonts.find(name)(); }

private:
  map<string, Font*> fonts;
};

Fonts fonts;

}
