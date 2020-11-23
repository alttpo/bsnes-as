
class Bridge {
  GUI::Window@ window;
  GUI::LineEdit@ txtHost;
  GUI::LineEdit@ txtPort;
  GUI::Button@ go;
  GUI::Button@ stop;

  Bridge() {
    @window = GUI::Window(120, 120, true);
    window.title = "Connector";
    window.size = GUI::Size(280, 5*25);
    window.dismissable = false;

    auto vl = GUI::VerticalLayout();
    vl.setSpacing();
    vl.setPadding(5, 5);
    window.append(vl);
    {
      {
        auto @hz = GUI::HorizontalLayout();
        vl.append(hz, GUI::Size(-1, 0));

        auto @lbl = GUI::Label();
        lbl.text = "Host:";
        hz.append(lbl, GUI::Size(100, 0));

        @txtHost = GUI::LineEdit();
        txtHost.text = "127.0.0.1";
        hz.append(txtHost, GUI::Size(-1, 20));
      }

      {
        auto @hz = GUI::HorizontalLayout();
        vl.append(hz, GUI::Size(-1, 0));

        auto @lbl = GUI::Label();
        lbl.text = "Port:";
        hz.append(lbl, GUI::Size(100, 0));

        @txtPort = GUI::LineEdit();
        txtPort.text = "43884";
        hz.append(txtPort, GUI::Size(-1, 20));
      }

      {
        auto @hz = GUI::HorizontalLayout();
        vl.append(hz, GUI::Size(-1, -1));

        @go = GUI::Button();
        go.text = "Connect";
        go.onActivate(@GUI::Callback(goClicked));
        hz.append(go, GUI::Size(-1, -1));

        @stop = GUI::Button();
        stop.text = "Disconnect";
        stop.onActivate(@GUI::Callback(stopClicked));
        stop.enabled = false;
        hz.append(stop, GUI::Size(-1, -1));
      }
    }

    vl.resize();
    window.visible = true;
    window.setFocused();
  }

  private int state = 0;
  private net::Address@ addr = null;
  private net::Socket@ sock = null;

  void reset() {
    sock.close();
    go.enabled = true;
    stop.enabled = false;
    state = 0;
  }

  void fail(string what) {
    message("net::" + what + " error " + net::error_code + "; " + net::error_text);
    reset();
  }

  private void goClicked() {
    @addr = net::resolve_tcp(txtHost.text, txtPort.text);
    if (net::is_error) {
      fail("resolve_tcp");
      return;
    }

    state = 1;
    go.enabled = false;
    stop.enabled = true;
  }

  private void stopClicked() {
    if (state >= 1) {
      if (sock !is null) {
        sock.close();
      }
      @sock = null;
    }

    state = 0;
    go.enabled = true;
    stop.enabled = false;
  }

  void main() {
    if (state == 0) {
      return;
    } else if (state == 1) {
      // connect:
      @sock = net::Socket(addr);
      sock.connect(addr);
      if (net::is_error && net::error_code != "EINPROGRESS" && net::error_code != "EWOULDBLOCK") {
        fail("connect");
        return;
      }

      // wait for connection success:
      state = 2;
    } else if (state == 2) {
      // socket writable means connected:
      bool connected = net::is_writable(sock);
      if (!connected && net::is_error) {
        fail("is_writable");
        return;
      }

      // connected:
      state = 3;
    } else if (state == 3) {
      //if (!net::is_readable(sock)) {
      //  if (net::is_error) {
      //    fail("is_readable");
      //  }
      //  return;
      //}

      // receive bytes and delineate NUL-terminated messages:
      array<uint8> sizeUint(4);
      int n = sock.recv(0, 4, sizeUint);
      if (net::is_error && net::error_code != "EWOULDBLOCK") {
        fail("recv");
        return;
      }

      // no data available:
      if (n <= 0) {
        return;
      }

      // read big-endian size of message to read:
      uint size = (uint(sizeUint[0]) << 24)
        | (uint(sizeUint[1]) << 16)
        | (uint(sizeUint[2]) << 8)
        | (uint(sizeUint[3]));

      // await all that data:
      array<uint8> buf(size);
      uint m = 0;
      while (m < size) {
        // async recv; will return instantly if no data available:
        n = sock.recv(m, size, buf);
        if (net::is_error && net::error_code != "EWOULDBLOCK") {
          fail("recv");
          return;
        }
        // no data available:
        if (n <= 0) {
          return;
        }

        m += n;
      }

      // process this message as a string:
      processMessage(buf.toString(0, m));
    }
  }

  private void processMessage(const string &in t) {
    // parse as JSON:
    auto j = JSON::parse(t).object;

    // extract useful values or sensible defaults:
    auto commandType = j["type"].naturalOr(0xFF);
    auto domain = j["domain"].stringOr("System Bus");
    auto address = j["address"].naturalOr(0);

    JSON::Object response;
    response["id"] = j["id"];
    response["stamp"] = JSON::Value(chrono::realtime::second);
    response["type"] = j["type"];
    response["message"] = JSON::Value("");
    response["address"] = j["address"];
    response["size"] = j["size"];
    response["domain"] = j["domain"];
    response["value"] = j["value"];

    switch (commandType) {
      case 0x00:
        response["value"] = JSON::Value(bus::read_u8(address));
        break;
      default:
        message("unrecognized command " + fmtHex(commandType, 2));
        break;
    }

    // make sure socket is writable before sending reply:
    // TODO: while() loop until it is?!
    if (!net::is_writable(sock)) {
      if (net::is_error) {
        fail("is_writable");
      }
      return;
    }

    {
      // compose response packet:
      auto data = JSON::serialize(JSON::Value(response));
      uint size = data.length();

      array<uint8> r(4 + size);
      r.insertLast(uint8((size >> 24) & 0xFF));
      r.insertLast(uint8((size >> 16) & 0xFF));
      r.insertLast(uint8((size >> 8) & 0xFF));
      r.insertLast(uint8((size) & 0xFF));
      r.write_str(data);

      sock.send(0, r.length(), r);
      if (net::is_error) {
        fail("send");
        return;
      }
    }
  }
}

Bridge@ bridge;

void init() {
  @bridge = Bridge();
}

void pre_frame() {
  bridge.main();
}
