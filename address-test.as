// Simple test for net namespace
array<net::Socket@> clients;
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
    clients.insertLast(accepted);
  }

  auto len = clients.length();
  for (uint c = 0; c < len; c++) {
    array<uint8> buf(128);
    auto r = clients[c].recv(0, 128, buf);
    if (r != -1) {
      message(fmtInt(r));
    }
  }
}