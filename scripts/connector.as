
const uint8 VERSION_MAJOR = 2;
const uint8 VERSION_MINOR = 2;
const uint8 VERSION_PATCH = 0;
const uint version = (uint(VERSION_MAJOR) << 16) | (uint(VERSION_MINOR) << 8) | uint(VERSION_PATCH);

class Connector {
  GUI::Window@ window;
  GUI::LineEdit@ txtHost;
  GUI::LineEdit@ txtPort;
  GUI::Button@ go;
  GUI::Button@ stop;

  Connector() {
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

  array<uint8> sizeUint(4);
  int sizeN;
  int size;
  array<uint8> buf;
  int bufN;

  void main() {
    // run up to 16 steps (arbitrary limit) of this state machine per frame:
    for (uint i = 0; i < 16; i++) {
      if (!step()) return;
    }
  }

  // returns true if we can run another step immediately after, false if need to wait for next frame.
  bool step() {
    if (state == 0) {
      // not connected; need to wait for external event:
      return false;
    } else if (state == 1) {
      // connect:
      @sock = net::Socket(addr);
      sock.connect(addr);
      if (net::is_error && net::error_code != "EINPROGRESS" && net::error_code != "EWOULDBLOCK") {
        fail("connect");
        return false;
      }

      // wait for connection success:
      state = 2;
      return true;
    } else if (state == 2) {
      // socket writable means connected:
      bool connected = net::is_writable(sock);
      if (!connected && net::is_error) {
        fail("is_writable");
        return false;
      }

      // connected:
      state = 3;
      return true;
    } else if (state == 3) {
      // entry state
      // start expecting JSON messages with 4-byte size prefix:
      sizeN = 0;
      state = 4;
      return true;
    } else if (state == 4) {
      // read 4-byte size prefix:
      if (!net::is_readable(sock)) {
        if (net::is_error) {
          fail("is_readable");
        }
        return false;
      }

      // receive bytes and delineate NUL-terminated messages:
      int n = sock.recv(sizeN, 4 - sizeN, sizeUint);
      if (net::is_error && net::error_code != "EWOULDBLOCK") {
        fail("recv");
        return false;
      }

      // no data available:
      if (n <= 0) {
        return false;
      }

      sizeN += n;

      if (sizeN < 4) {
        // see if more data is available to read:
        return true;
      }

      // reset for next message:
      sizeN = 0;

      // read big-endian size of message to read:
      size = (uint(sizeUint[0]) << 24)
        | (uint(sizeUint[1]) << 16)
        | (uint(sizeUint[2]) << 8)
        | (uint(sizeUint[3]));

      // resize our buffer:
      buf.resize(size);
      bufN = 0;

      // move on to read message:
      state = 5;
      return true;
    } else if (state == 5) {
      // read JSON message of size `size`:

      // async recv; will return instantly if no data available:
      int n = sock.recv(bufN, size - bufN, buf);
      if (net::is_error && net::error_code != "EWOULDBLOCK") {
        fail("recv");
        return false;
      }

      // no data available:
      if (n <= 0) {
        return false;
      }

      bufN += n;

      if (bufN < size) {
        // see if more data is available to read:
        return true;
      }

      state = 6;
      return true;
    } else if (state == 6) {
      // make sure socket is writable before sending reply:
      if (!net::is_writable(sock)) {
        if (net::is_error) {
          fail("is_writable");
        }
        return false;
      }

      // process this message as a string:
      processMessage(buf.toString(0, bufN));

      // await next message:
      state = 3;
      return true;
    }

    // unknown state!
    return false;
  }

  private void processMessage(const string &in t) {
    message(t);

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
    if (j.containsKey("message")) response["message"] = JSON::Value("");
    if (j.containsKey("address")) response["address"] = j["address"];
    if (j.containsKey("size"))    response["size"] = j["size"];
    if (j.containsKey("domain"))  response["domain"] = j["domain"];
    if (j.containsKey("value"))   response["value"] = j["value"];

    switch (commandType) {
      case 0xe3:  // get emulator id
        response["value"]   = JSON::Value(version);
        response["message"] = JSON::Value("SNES");
        break;
      case 0x00:  // read byte
        // TODO: domain
        response["value"] = JSON::Value(bus::read_u8(address));
        break;
      case 0x01:  // read short
        // TODO: domain
        response["value"] = JSON::Value(bus::read_u16(address));
        break;
      case 0x02:  // read uint32
        // TODO: domain
        response["value"] = JSON::Value((uint(bus::read_u16(address)) << 16) | uint(bus::read_u16(address+2)));
        break;
      case 0x0f: { // read block
        // TODO: domain
        auto size = j["size"].natural;
        array<uint8> buf(size);
        bus::read_block_u8(address, 0, size, buf);
        auto blockStr = base64::encode(0, size, buf);
        response["block"] = JSON::Value(blockStr);
        break;
      }
      case 0xf0:  // display message
        if (!j["message"].isNull && j["message"].isString) {
          message("message: " + j["message"].string);
        }
      case 0xff:
        // do nothing.
        break;
      default:
        message("unrecognized command 0x" + fmtHex(commandType, 2));
        break;
    }

    {
      // compose response packet:
      auto data = JSON::serialize(JSON::Value(response));
      uint size = data.length();

      array<uint8> r;
      r.reserve(4 + size);
      r.insertLast(uint8((size >> 24) & 0xFF));
      r.insertLast(uint8((size >> 16) & 0xFF));
      r.insertLast(uint8((size >> 8) & 0xFF));
      r.insertLast(uint8((size) & 0xFF));
      r.write_str(data);

      message("send: r_length=" + fmtInt(r.length()) + " size=" + fmtInt(size) + " data=`" + data + "`");

      sock.send(0, r.length(), r);
      if (net::is_error) {
        fail("send");
        return;
      }
    }
  }
}

Connector@ connector;

void init() {
  @connector = Connector();
}

void pre_frame() {
  connector.main();
}
