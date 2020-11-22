void init() {
  // JSON test cases:
  {
    JSON::Node v = JSON::parse("{\"a\":12.5}");
    auto a = v.object["a"];
    if (a.isNull) {
      message("a is null!");
    } else {
      auto r = a.real;
      message("a is " + fmtFloat(r));
    }
  }

  {
    JSON::Node v = JSON::parse("{\"arr\":[12,true,null]}");
    auto arrv = v.object["arr"];
    if (arrv.isNull) {
      message("arr is null!");
    } else {
      auto@ arr = arrv.array;
      auto len = arr.length;
      for (uint i = 0; i < len; i++) {
        message("a[" + fmtInt(i) + "] is " + arr[i].text);
      }
    }
  }

}
