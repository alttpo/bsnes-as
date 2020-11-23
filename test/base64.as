void init() {
  {
    array<uint8> t(4);
    t[0] = 1;
    t[1] = 2;
    t[2] = 3;
    t[3] = 4;
    auto text = base64::encode(0, t.length(), t);
    message(text);
  }

  {
    array<uint8> d(4);
    base64::decode("AQIDBA==", d);
    message(fmtInt(d[0]));
    message(fmtInt(d[1]));
    message(fmtInt(d[2]));
    message(fmtInt(d[3]));
  }
}
