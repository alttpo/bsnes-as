void init() {
  // JSON test cases:
  {
    JSON::Value v = JSON::parse("{\"a\":12.5}");
    auto a = v.object["a"];
    if (a.isNull) {
      message("a is null!");
    } else {
      auto r = a.real;
      message("a is " + fmtFloat(r));
    }
  }

  {
    JSON::Value v = JSON::parse("{\"arr\":[12,true,null]}");
    auto arrv = v.object["arr"];
    if (arrv.isNull) {
      message("arr is null!");
    } else {
      auto arr = arrv.array;
      auto len = arr.length;
      for (uint i = 0; i < len; i++) {
        auto t = arr[i];
             if (t.isNull)    message("arr[" + fmtInt(i) + "] is null");
        else if (t.isString)  message("arr[" + fmtInt(i) + "] is " + arr[i].string);
        else if (t.isBoolean) message("arr[" + fmtInt(i) + "] is " + fmtBool(arr[i].boolean));
        else if (t.isNumber)  message("arr[" + fmtInt(i) + "] is " + fmtFloat(arr[i].real));
      }
    }
  }

  {
    JSON::Value v = JSON::parse("{\"obj\":{\"a\":1}}");
    auto objv = v.object["obj"];
    if (objv.isNull) {
      message("obj is null!");
    } else {
      auto obj = objv.object;
      message("obj['a'] is " + fmtInt(obj["a"].integer));
      {
        auto tmp = JSON::Value();
        tmp.string = "hello";
        obj["b"] = tmp;
      }
      message("obj['b'] is " + obj["b"].string);
      objv.object = obj;
      message("obj['b'] is " + objv.object["b"].string);
    }
  }

  {
    JSON::Object o;
    o["hello"] = JSON::Value("world");
    message("o = " + JSON::serialize(o));
  }
}
