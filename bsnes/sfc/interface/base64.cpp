
auto base64_decode(const string& text) -> vector<uint8_t> {
  static const unsigned char pr2six[256] =
    {
      /* ASCII table */
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
      52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
      64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
      15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
      64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
      41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
      64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    };

  vector<uint8_t> out;

  int nbytesdecoded;
  const unsigned char *bufin;
  int nprbytes;

  bufin = (const unsigned char *) text.data();
  while (pr2six[*(bufin++)] <= 63);
  nprbytes = (bufin - (const unsigned char *) text.data()) - 1;
  nbytesdecoded = ((nprbytes + 3) / 4) * 3;

  bufin = (const unsigned char *) text.data();

  while (nprbytes > 4) {
    out.append((unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4));
    out.append((unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2));
    out.append((unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]));
    bufin += 4;
    nprbytes -= 4;
  }

  /* Note: (nprbytes == 1) would be an error, so just ingore that case */
  if (nprbytes > 1) {
    out.append((unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4));
  }
  if (nprbytes > 2) {
    out.append((unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2));
  }
  if (nprbytes > 3) {
    out.append((unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]));
  }

  nbytesdecoded -= (4 - nprbytes) & 3;
  out.resize(nbytesdecoded);

  return out;
}

auto base64_encode(const void* vdata, uint size) -> string {
  return ::nall::Encode::Base64(vdata, size);
}
