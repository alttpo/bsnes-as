
class Bridge {
  GUI::Window@ window;
  GUI::LineEdit@ txtHost;
  GUI::LineEdit@ txtPort;
  GUI::Button@ go;

  Bridge() {
    @window = GUI::Window(120, 120, true);
    window.title = "USB2SNES";
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
        txtPort.text = "65398";
        hz.append(txtPort, GUI::Size(-1, 20));
      }

      {
        auto @hz = GUI::HorizontalLayout();
        vl.append(hz, GUI::Size(-1, -1));

        @go = GUI::Button();
        go.text = "Connect";
        go.onActivate(@GUI::Callback(goClicked));
        hz.append(go, GUI::Size(-1, -1));
      }
    }

    vl.resize();
    window.visible = true;
    window.setFocused();
  }

  private bool connecting = false;
  private bool connected = false;
  private net::Address@ addr = null;
  private net::Socket@ sock = null;

  private void goClicked() {
    connected = false;
    connecting = true;
    @addr = net::resolve_tcp(txtHost.text, txtPort.text);
  }

  void main() {
    if (!connected) {
      if (!connecting) {
      return;
      }

      // connect:
      @sock = net::Socket(addr);
      sock.connect(addr);
      connecting = false;
      connected = true;
    }

    // receive a message:
    array<uint8> m(1500);
    int n = sock.recv(0, 1500, m);
    if (n <= 0) {
      return;
    }
    m.resize(n);

    // process the message:
    processMessage(m);
  }

  private void processMessage(const array<uint8> &in m) {

  }
}

Bridge@ bridge;

void init() {
  @bridge = Bridge();
}

void pre_frame() {
  bridge.main();
}
