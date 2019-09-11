// Network server script
net::UDPSocket@ sock;

void init() {
  @sock = net::UDPSocket("127.0.0.1", 4590);
}

void pre_frame() {
  array<uint8> msg = { 0x10, 0x20, 0x40, 0x80 };
  sock.send(msg);
}
