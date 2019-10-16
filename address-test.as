// Simple test for net namespace
array<net::WebSocketHandshaker@> handshakes;
array<net::WebSocket@> clients;
net::Socket@ server;

void init() {
  auto@ addr = net::resolve_tcp("", 4590);
  net::throw_if_error();
  @server = net::Socket(addr);
  net::throw_if_error();
  server.bind(addr);
  net::throw_if_error();
  server.listen(32);
  net::throw_if_error();
}

void pre_frame() {
  auto@ accepted = server.accept();
  if (@accepted != null) {
    handshakes.insertLast(net::WebSocketHandshaker(accepted));
  }

  auto len = handshakes.length();
  for (int c = len-1; c >= 0; c--) {
    auto@ ws = handshakes[c].handshake();
    net::throw_if_error();
    if (@ws != null) {
      handshakes.removeAt(c);
      clients.insertLast(ws);
    }
  }

  len = clients.length();
  for (int c = len-1; c >= 0; c--) {
    auto@ msg = clients[c].process();
    net::throw_if_error();
    if (!clients[c].is_valid) {
      // socket closed:
      message("socket closed by remote peer");
      clients.removeAt(c);
      continue;
    }
    if (@msg == null) {
      continue;
    }

    if (msg.opcode == 1) {
      auto s = msg.as_string();
      message(s);
    } else {
      auto a = msg.as_array();
      message(fmtInt(a.length()));
    }
  }
}