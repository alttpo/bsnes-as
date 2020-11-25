// Simple test for net namespace
net::WebSocketServer@ wsServer;

void init() {
  @wsServer = net::WebSocketServer("ws://localhost:4590/alttpo");
}

void idle() {
  pre_frame();
}

int frame_counter = 0;

void pre_frame() {
  frame_counter++;

  wsServer.process();

  auto clients = wsServer.clients;
  auto len = clients.length();
  for (int c = len-1; c >= 0; c--) {
    // ping each client every 15 seconds:
    if (frame_counter == 60 * 15) {
      frame_counter = 0;
      auto@ ping = net::WebSocketMessage(9);
      clients[c].send(ping);
    }
    if ((frame_counter & 255) == 0) {
      auto@ msg = net::WebSocketMessage(1);
      msg.payload_as_string = "test from server!";
      clients[c].send(msg);
      message("sent test message");
    }

    auto@ msg = clients[c].process();
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
      message("text: " + s);
    } else if (msg.opcode == 8) {
      clients[c].close();
    } else {
      auto a = msg.as_array();
      message("opcode=" + fmtHex(msg.opcode) + " length=" + fmtInt(a.length()));
    }
  }
}