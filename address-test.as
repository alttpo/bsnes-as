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
      message("hands shaken! ");
      //handshakes.removeAt(c);
      message("pass?");
      //clients.insertLast(ws);
      message("pass!");
    }
  }

  len = clients.length();
  for (int c = len-1; c >= 0; c--) {
/*
    array<uint8> buf(128);
    auto r = clients[c].recv(0, 128, buf);
    if (r == 0) {
      // client closed connection:
      clients.removeAt(c);
      continue;
    }
    if (r != -1) {
      message(fmtInt(r));
    }
*/
  }
}