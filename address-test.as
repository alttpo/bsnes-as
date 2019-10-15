// Simple test for net::Address
void init_fail_address() {
  auto@ addr = net::resolve_tcp(":90", 80);
  message(fmtInt(addr.is_valid ? 1 : 0));
  addr.throw_if_invalid();
}

array<net::Socket@> pollfds;
net::Socket@ server;

void init() {
  auto@ addr = net::resolve_tcp("", 4590);
  @server = net::Socket(addr);
  server.throw_if_invalid();
  server.bind(addr);
  server.throw_if_invalid();
  server.listen();
  server.throw_if_invalid();
}

void pre_frame() {
  auto@ accepted = server.accept();
  if (@accepted != null) {
    pollfds.insertLast(accepted);
  }

  if (pollfds.length() > 0 && net::poll_in(pollfds)) {
    message("poll() pass");
    //pollfds[1].
  }
}