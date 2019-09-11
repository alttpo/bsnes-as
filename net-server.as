// Network server script
net::UDPSocket@ sock;
array<uint8> msg = { 0x40, 0x41, 0x42, 0x43 };

void init() {
  @sock = net::UDPSocket("127.0.0.1", 4590);
}

void pre_frame() {
  sock.sendto(msg, "127.0.0.2", 4590);
  message("test");
}
