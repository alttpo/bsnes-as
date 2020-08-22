
class testMe {
  string toString() const {
    return "toString() method";
  }
}

void init() {
  string f = "Hello, {0}! {1} {2} {3} {4} {5} {6} {7} {8} {9} {10}";
  for (int i = 0; i < 100; i++) {
    string m = f.format({i, uint8(1), uint16(2), uint32(3), uint64(4), int8(5), int16(6), int32(7), int64(8), "a", "bc"});
    message(m);
  }
  message("{0}".format({testMe()}));
}
