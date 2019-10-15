// Simple test for net::Address
void init_fail_address() {
  auto@ addr = net::Address(":90", 80);
  message(fmtInt(addr.is_valid ? 1 : 0));
  addr.throw_if_invalid();
}

array<net::Socket@> pollfds(1);
net::Socket@ server;

void init() {
  auto@ addr = net::Address("", 4590);
  @server = net::Socket(addr);
  server.throw_if_invalid();
  server.bind(addr);
  server.throw_if_invalid();
  server.listen();
  server.throw_if_invalid();
  @pollfds[0] = server;
}

void pre_frame() {
  if (net::poll_in(pollfds)) {
    auto@ accepted = server.accept();
    if (@accepted != null) {
      pollfds.insertLast(accepted);
    }
    //pollfds[1].
  }
}