
namespace Net {
  int last_error = 0;
  int last_error_gai = 0;
  const char *last_error_location = nullptr;

  // socket error codes:
  const string *strEWOULDBLOCK = new string("EWOULDBLOCK");
  const string *strUnknown     = new string("EUNKNOWN");

  // getaddrinfo error codes:
  const string *strEAI_ADDRFAMILY = new string("EAI_ADDRFAMILY");
  const string *strEAI_AGAIN      = new string("EAI_AGAIN");
  const string *strEAI_BADFLAGS   = new string("EAI_BADFLAGS");
  const string *strEAI_FAIL       = new string("EAI_FAIL");
  const string *strEAI_FAMILY     = new string("EAI_FAMILY");
  const string *strEAI_MEMORY     = new string("EAI_MEMORY");
  const string *strEAI_NODATA     = new string("EAI_NODATA");
  const string *strEAI_NONAME     = new string("EAI_NONAME");
  const string *strEAI_SERVICE    = new string("EAI_SERVICE");
  const string *strEAI_SOCKTYPE   = new string("EAI_SOCKTYPE");
  const string *strEAI_SYSTEM     = new string("EAI_SYSTEM");
  const string *strEAI_BADHINTS   = new string("EAI_BADHINTS");
  const string *strEAI_PROTOCOL   = new string("EAI_PROTOCOL");
  const string *strEAI_OVERFLOW   = new string("EAI_OVERFLOW");

  static auto get_is_error() -> bool {
    return last_error != 0;
  }

  // get last error as a standardized code string:
  static auto get_error_code() -> const string* {
    if (last_error_gai != 0) {
      switch (last_error_gai) {
        case EAI_AGAIN: return strEAI_AGAIN;
        case EAI_BADFLAGS: return strEAI_BADFLAGS;
        case EAI_FAIL: return strEAI_FAIL;
        case EAI_FAMILY: return strEAI_FAMILY;
        case EAI_MEMORY: return strEAI_MEMORY;

        case EAI_NONAME: return strEAI_NONAME;
        case EAI_SERVICE: return strEAI_SERVICE;
        case EAI_SOCKTYPE: return strEAI_SOCKTYPE;
#if !defined(PLATFORM_WINDOWS)
        case EAI_SYSTEM: return strEAI_SYSTEM;
        case EAI_OVERFLOW: return strEAI_OVERFLOW;
#endif
#if !defined(PLATFORM_WINDOWS) && !defined(_POSIX_C_SOURCE)
        case EAI_ADDRFAMILY: return strEAI_ADDRFAMILY;
        case EAI_NODATA: return strEAI_NODATA;
        case EAI_BADHINTS: return strEAI_BADHINTS;
        case EAI_PROTOCOL: return strEAI_PROTOCOL;
#endif
        default: return strUnknown;
      }
    }

#if !defined(PLATFORM_WINDOWS)
    if (last_error == EWOULDBLOCK || last_error == EAGAIN) return strEWOULDBLOCK;
    //switch (last_error) {
    //}
#else
    switch (last_error) {
      case WSAEWOULDBLOCK: return strEWOULDBLOCK;
    }
#endif
    return strUnknown;
  }

  static auto get_error_text() -> const string* {
    auto s = new string(sock_error_string(last_error));
    s->trimRight("\r\n ");
    return s;
  }

  static auto exception_thrown() -> bool {
    if (last_error_gai) {
      // throw script exception:
      asGetActiveContext()->SetException(string{last_error_location, ": ", gai_strerror(last_error_gai)}, true);
      last_error_gai = 0;
      return true;
    }
    if (last_error == 0) return false;

#if !defined(PLATFORM_WINDOWS)
    // Don't throw exception for EWOULDBLOCK:
    if (last_error == EWOULDBLOCK || last_error == EAGAIN) {
#else
      // Don't throw exception for WSAEWOULDBLOCK:
    if (last_error == WSAEWOULDBLOCK) {
#endif
      last_error = 0;
      last_error_location = "((FIXME: no location!))";
      return false;
    }

    // throw script exception:
    asGetActiveContext()->SetException(string{last_error_location, "; err=", last_error, ", code=", *get_error_code(), ": ", *get_error_text()}, true);
    last_error = 0;
    last_error_location = "((FIXME: no location!))";
    return true;
  }

  struct Address {
    addrinfo *info;

    // constructor:
    Address(const string *host, const string *port, int family = AF_UNSPEC, int socktype = SOCK_STREAM) {
      info = nullptr;

      addrinfo hints;
      const char *hostCstr;
      const char *portCstr;

      memset(&hints, 0, sizeof(addrinfo));
      hints.ai_family = family;
      hints.ai_socktype = socktype;

      if (host == nullptr || (*host) == "") {
        hostCstr = (const char *)nullptr;
        // AI_PASSIVE hint is ignored otherwise.
        hints.ai_flags = AI_PASSIVE;
      } else {
        hostCstr = (const char *)*host;
      }
      portCstr = (const char *)*port;

      last_error_gai = 0;
      last_error = 0;
      int rc = ::getaddrinfo(hostCstr, portCstr, &hints, &info); last_error_location = LOCATION " getaddrinfo";
      if (rc != 0) {
#if !defined(PLATFORM_WINDOWS)
        if (rc == EAI_SYSTEM) {
          last_error = sock_capture_error();
        } else
#endif
        {
          last_error_gai = rc;
        }
        exception_thrown();
      }
    }

    ~Address() {
      if (info) {
        ::freeaddrinfo(info);
        // TODO: error checking?
      }
      info = nullptr;
    }

    operator bool() { return info; }
  };

  static auto resolve_tcp(const string *host, const string *port) -> Address* {
    auto addr = new Address(host, port, AF_INET, SOCK_STREAM);
    if (!*addr) {
      delete addr;
      return nullptr;
    }
    return addr;
  }

  static auto resolve_udp(const string *host, const string *port) -> Address* {
    auto addr = new Address(host, port, AF_INET, SOCK_DGRAM);
    if (!*addr) {
      delete addr;
      return nullptr;
    }
    return addr;
  }

  struct Socket {
    int fd = -1;

    // already-created socket:
    Socket(int fd) : fd(fd) {
    }

    // create a new socket:
    Socket(int family, int type, int protocol) {
      ref = 1;

      // create the socket:
#if !defined(PLATFORM_WINDOWS)
      fd = ::socket(family, type, protocol); last_error_location = LOCATION " socket";
#else
      fd = ::WSASocket(family, type, protocol, nullptr, 0, 0); last_error_location = LOCATION " WSASocket";
#endif
      last_error = 0;
      if (fd < 0) {
        last_error = sock_capture_error();
        exception_thrown();
        return;
      }

      // enable reuse address:
      int rc = 0;
      int yes = 1;
#if !defined(PLATFORM_WINDOWS)
      rc = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)); last_error_location = LOCATION " setsockopt";
#else
      rc = ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(int)); last_error_location = LOCATION " setsockopt";
#endif
      last_error = 0;
      if (rc < 0) {
        last_error = sock_capture_error();
        close(false);
        exception_thrown();
        return;
      }

      // enable TCP_NODELAY for SOCK_STREAM type sockets:
      if (type == SOCK_STREAM) {
        //if (true) {
#if !defined(PLATFORM_WINDOWS)
        rc = ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)); last_error_location = LOCATION " setsockopt";
#else
        rc = ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&yes, sizeof(int)); last_error_location = LOCATION " setsockopt";
#endif
        last_error = 0;
        if (rc < 0) {
          last_error = sock_capture_error();
          close(false);
          exception_thrown();
          return;
        }
      }

      // set non-blocking:
#if !defined(PLATFORM_WINDOWS)
      rc = ioctl(fd, FIONBIO, &yes); last_error_location = LOCATION " ioctl";
#else
      rc = ioctlsocket(fd, FIONBIO, (u_long *)&yes); last_error_location = LOCATION " ioctlsocket";
#endif
      last_error = 0;
      if (rc < 0) {
        last_error = sock_capture_error();
        close(false);
        exception_thrown();
        return;
      }
    }

    ~Socket() {
      if (operator bool()) {
        close();
      }

      fd = -1;
    }

    int ref;
    void addRef() {
      ref++;
    }
    void release() {
      if (--ref == 0)
        delete this;
    }

    auto close(bool set_last_error = true) -> void {
      int rc;
#if !defined(PLATFORM_WINDOWS)
      rc = ::close(fd); set_last_error && (last_error_location = LOCATION " close");
#else
      rc = ::closesocket(fd); set_last_error && (last_error_location = LOCATION " closesocket");
#endif
      if (set_last_error) {
        last_error = 0;
        if (rc < 0) {
          last_error = sock_capture_error();
          exception_thrown();
        }
      }

      fd = -1;
    }

    operator bool() { return fd >= 0; }

    // indicates data is ready to be read:
    bool ready_in = false;
    auto set_ready_in(bool value) -> void {
      ready_in = value;
    }

    // indicates data is ready to be written:
    bool ready_out = false;
    auto set_ready_out(bool value) -> void {
      ready_out = value;
    }

    // bind to an address:
    auto bind(const Address *addr) -> int {
      int rc = ::bind(fd, addr->info->ai_addr, addr->info->ai_addrlen); last_error_location = LOCATION " bind";
      last_error = 0;
      if (rc < 0) {
        last_error = sock_capture_error();
        exception_thrown();
      }
      return rc;
    }

    // connect to an address:
    auto connect(const Address *addr) -> int {
      int rc = ::connect(fd, addr->info->ai_addr, addr->info->ai_addrlen); last_error_location = LOCATION " connect";
      last_error = 0;
      if (rc < 0) {
        last_error = sock_capture_error();
        exception_thrown();
      }
      return rc;
    }

    // start listening for connections:
    auto listen(int backlog = 32) -> int {
      int rc = ::listen(fd, backlog); last_error_location = LOCATION " listen";
      last_error = 0;
      if (rc < 0) {
        last_error = sock_capture_error();
        exception_thrown();
      }
      return rc;
    }

    // accept a connection:
    auto accept() -> Socket* {
      // non-blocking sockets don't require a poll() because accept() handles non-blocking scenario itself.

      // accept incoming connection, discard client address:
      int afd = ::accept(fd, nullptr, nullptr); last_error_location = LOCATION " accept";
      last_error = 0;
      if (afd < 0) {
        last_error = sock_capture_error();
        exception_thrown();
        return nullptr;
      }

      auto conn = new Socket(afd);
      if (!*conn) {
        delete conn;
        return nullptr;
      }

      return conn;
    }

    // attempt to receive data:
    auto recv(int offs, int size, CScriptArray* buffer) -> int {
#if !defined(PLATFORM_WINDOWS)
      int rc = ::recv(fd, buffer->At(offs), size, 0); last_error_location = LOCATION " recv";
#else
      int rc = ::recv(fd, (char *)buffer->At(offs), size, 0); last_error_location = LOCATION " recv";
#endif
      last_error = 0;
      if (rc == -1) {
        last_error = sock_capture_error();
        exception_thrown();
      }
      return rc;
    }

    // attempt to send data:
    auto send(int offs, int size, CScriptArray* buffer) -> int {
#if !defined(PLATFORM_WINDOWS)
      int rc = ::send(fd, buffer->At(offs), size, 0); last_error_location = LOCATION " send";
#else
      int rc = ::send(fd, (const char*)buffer->At(offs), size, 0); last_error_location = LOCATION " send";
#endif
      last_error = 0;
      if (rc == -1) {
        last_error = sock_capture_error();
        exception_thrown();
      }
      return rc;
    }

    // TODO: expose this to scripts somehow.
    struct sockaddr_storage recvaddr;
    socklen_t recvaddrlen;

    // attempt to receive data and record address of sender (e.g. for UDP):
    auto recvfrom(int offs, int size, CScriptArray* buffer) -> int {
#if !defined(PLATFORM_WINDOWS)
      int rc = ::recvfrom(fd, buffer->At(offs), size, 0, (struct sockaddr *)&recvaddr, &recvaddrlen); last_error_location = LOCATION " recvfrom";
#else
      int rc = ::recvfrom(fd, (char *)buffer->At(offs), size, 0, (struct sockaddr *)&recvaddr, &recvaddrlen); last_error_location = LOCATION " recvfrom";
#endif
      last_error = 0;
      if (rc == -1) {
        last_error = sock_capture_error();
        exception_thrown();
      }
      return rc;
    }

    // attempt to send data to specific address (e.g. for UDP):
    auto sendto(int offs, int size, CScriptArray* buffer, const Address* addr) -> int {
#if !defined(PLATFORM_WINDOWS)
      int rc = ::sendto(fd, buffer->At(offs), size, 0, addr->info->ai_addr, addr->info->ai_addrlen); last_error_location = LOCATION " sendto";
#else
      int rc = ::sendto(fd, (const char *)buffer->At(offs), size, 0, addr->info->ai_addr, addr->info->ai_addrlen); last_error_location = LOCATION " sendto";
#endif
      last_error = 0;
      if (rc == -1) {
        last_error = sock_capture_error();
        exception_thrown();
      }
      return rc;
    }

    auto recv_append(string &s) -> uint64_t {
      uint8_t rawbuf[4096];
      uint64_t total = 0;

      for (;;) {
#if !defined(PLATFORM_WINDOWS)
        int rc = ::recv(fd, rawbuf, 4096, 0); last_error_location = LOCATION " recv";
#else
        int rc = ::recv(fd, (char *)rawbuf, 4096, 0); last_error_location = LOCATION " recv";
#endif
        last_error = 0;
        if (rc < 0) {
          last_error = sock_capture_error();
          exception_thrown();
          return total;
        }
        if (rc == 0) {
          // remote peer closed the connection
          close();
          return total;
        }

        // append to string:
        uint64_t to = s.size();
        s.resize(s.size() + rc);
        memory::copy(s.get() + to, rawbuf, rc);
        total += rc;
      }
    }

    auto recv_buffer(vector<uint8_t>& buffer) -> uint64_t {
      uint8_t rawbuf[4096];
      uint64_t total = 0;

      for (;;) {
#if !defined(PLATFORM_WINDOWS)
        int rc = ::recv(fd, rawbuf, 4096, 0); last_error_location = LOCATION " recv";
#else
        int rc = ::recv(fd, (char *)rawbuf, 4096, 0); last_error_location = LOCATION " recv";
#endif
        last_error = 0;
        if (rc < 0) {
          last_error = sock_capture_error();
          exception_thrown();
          return total;
        }
        if (rc == 0) {
          close();
          return total;
        }

        // append to buffer:
        buffer.appends({rawbuf, (uint)rc});
        total += rc;
      }
    }

    auto send_buffer(array_view<uint8_t> buffer) -> int {
#if !defined(PLATFORM_WINDOWS)
      int rc = ::send(fd, buffer.data(), buffer.size(), 0); last_error_location = LOCATION " send";
#else
      int rc = ::send(fd, (const char *)buffer.data(), buffer.size(), 0); last_error_location = LOCATION " send";
#endif
      last_error = 0;
      if (rc == 0) {
        close();
        return rc;
      }
      if (rc < 0) {
        last_error = sock_capture_error();
        exception_thrown();
      }
      return rc;
    }
  };

  static auto create_socket(Address *addr) -> Socket* {
    auto socket = new Socket(addr->info->ai_family, addr->info->ai_socktype, addr->info->ai_protocol);
    if (!*socket) {
      delete socket;
      return nullptr;
    }
    return socket;
  }

  struct WebSocketMessage {
    uint8           opcode;
    vector<uint8_t> bytes;

    WebSocketMessage(uint8 opcode) : opcode(opcode)
    {
      ref = 1;
    }
    ~WebSocketMessage() = default;

    int ref;
    void addRef() {
      ref++;
    }
    void release() {
      if (--ref == 0)
        delete this;
    }

    auto get_opcode() -> uint8 { return opcode; }
    auto set_opcode(uint8 value) -> void { opcode = value; }

    auto as_string() -> string* {
      auto s = new string();
      s->resize(bytes.size());
      memory::copy(s->get(), bytes.data(), bytes.size());
      return s;
    }

    auto as_array() -> CScriptArray* {
      asIScriptContext *ctx = asGetActiveContext();
      if (!ctx) return nullptr;

      asIScriptEngine* engine = ctx->GetEngine();
      asITypeInfo* t = engine->GetTypeInfoByDecl("array<uint8>");

      uint64_t size = bytes.size();
      auto a = CScriptArray::Create(t, size);
      if (size > 0) {
        memory::copy(a->At(0), bytes.data(), size);
      }
      return a;
    }

    auto set_payload_as_string(string *s) -> void {
      bytes.resize(s->size());
      memory::copy(bytes.data(), s->data(), s->size());
    }

    auto set_payload_as_array(CScriptArray *a) -> void {
      bytes.resize(a->GetSize());
      memory::copy(bytes.data(), a->At(0), a->GetSize());
    }
  };

  auto create_web_socket_message(uint8 opcode) -> WebSocketMessage* {
    return new WebSocketMessage(opcode);
  }

  struct WebSocket {
    Socket* socket;

    WebSocket(Socket* socket) : socket(socket) {
      socket->addRef();
      ref = 1;
    }
    ~WebSocket() {
      socket->release();
    }

    int ref;
    void addRef() {
      ref++;
    }
    void release() {
      if (--ref == 0)
        delete this;
    }

    operator bool() { return *socket; }

    vector<uint8_t> frame;
    WebSocketMessage *message = nullptr;

    // attempt to receive data:
    auto process() -> WebSocketMessage* {
      if (!*socket) {
        // TODO: throw exception if socket closed?
        printf("[%d] invalid socket!\n", socket->fd);
        return nullptr;
      }

      // Receive data and append to current frame:
      auto rc = socket->recv_buffer(frame);
      // No data ready to recv:
      if (rc == 0) {
        printf("[%d] no data\n", socket->fd);
        return nullptr;
      }

      // check start of frame:
      uint minsize = 2;
      if (frame.size() < minsize) {
        printf("[%d] frame too small (%d) to read opcode and payload length bytes (%d)\n", socket->fd, frame.size(), minsize);
        return nullptr;
      }

      uint i = 0;

      bool fin = frame[i] & 0x80;
      // 3 other reserved bits here
      uint8_t opcode = frame[i] & 0x0F;
      i++;

      bool mask = frame[i] & 0x80;
      uint64_t len = frame[i] & 0x7F;
      i++;

      // determine minimum size of frame to parse:
      if (len == 126) minsize += 2;
      else if (len == 127) minsize += 8;

      // need more data?
      if (frame.size() < minsize) {
        printf("[%d] frame too small (%d) to read additional payload length bytes (%d)\n", socket->fd, frame.size(), minsize);
        return nullptr;
      }

      if (len == 126) {
        // len is 16-bit
        len = 0;
        for (int j = 0; j < 2; j++) {
          len <<= 8u;
          len |= frame[i++];
        }
      } else if (len == 127) {
        // len is 64-bit
        len = 0;
        for (int j = 0; j < 8; j++) {
          len <<= 8u;
          len |= frame[i++];
        }
      }

      uint8_t mask_key[4] = {0};

      if (mask) {
        minsize += 4;
        if (frame.size() < minsize) {
          printf("[%d] frame too small (%d) to read mask key bytes (%d)\n", socket->fd, frame.size(), minsize);
          return nullptr;
        }

        // read mask key:
        for (unsigned char &mask_byte : mask_key) {
          mask_byte = frame[i++];
        }
        //printf("mask_key is %02x%02x%02x%02x\n", mask_key[0], mask_key[1], mask_key[2], mask_key[3]);
      } else {
        //printf("frame is not masked!\n");
        asGetActiveContext()->SetException("WebSocket frame received from client must be masked", true);
        return nullptr;
      }

      // not enough data in frame yet?
      if (frame.size() - i < len) {
        printf("[%d] frame too small (%d) to read full payload (%d)\n", socket->fd, frame.size(), i + len);
        return nullptr;
      }

      // For any opcode but 0 (continuation), start a new message:
      if (opcode != 0) {
        //printf("new message with opcode = %d\n", opcode);
        message = new WebSocketMessage(opcode);
      }

      // append payload data to message buffer:
      auto payload = frame.view(i, len);
      if (mask) {
        // Unmask the payload with the mask_key and append to message:
        //printf("append masked payload of size %d\n", payload.size());
        uint k = 0;
        for (uint8_t byte : payload) {
          message->bytes.append(byte ^ mask_key[k]);
          k = (k + 1) & 3;
        }
      } else {
        // Append unmasked payload:
        //printf("append unmasked payload of size %d\n", payload.size());
        message->bytes.appends(payload);
      }

      // clear out already-read data:
      frame.removeLeft(i + len);

      // if no FIN flag set, wait for more frames:
      if (!fin) {
        printf("[%d] FIN not set\n", socket->fd);
        return nullptr;
      }

      // return final message:
      printf("[%d] FIN set; returning message\n", socket->fd);
      auto tmp = message;
      message = nullptr;
      return tmp;
    }

    // send a message:
    auto send(WebSocketMessage* msg) -> void {
      vector<uint8_t> outframe;

      uint64_t len = msg->bytes.size();
      bool fin = true;

      outframe.append((fin << 7) | (msg->opcode & 0x0F));

      bool masked = false;
      if (len > 65536) {
        outframe.append((masked << 7) | 127);
        // send big-endian 64-bit payload length:
        for (int j = 0; j < 8; j++) {
          outframe.append((len >> (7-j)*8) & 0xFF);
        }
      } else if (len > 125) {
        outframe.append((masked << 7) | 126);
        // send big-endian 16-bit payload length:
        for (int j = 0; j < 2; j++) {
          outframe.append((len >> (1-j)*8) & 0xFF);
        }
      } else {
        // 7-bit (ish) length:
        outframe.append((masked << 7) | (len & 0x7F));
      }

      uint8_t mask_key[4] = {0};
      if (masked) {
        // TODO: build and send mask_key
      }

      // append payload to frame:
      outframe.appends(msg->bytes);

      // try to send frame:
      int rc = socket->send_buffer(outframe);
      if (rc == 0) {
        // remote peer closed the connection:
        return;
      }
      if (rc < 0) {
        // TODO: maintain sending state machine
        printf("[%d] send() error!\n", socket->fd);
        return;
      }
      if (rc < outframe.size()) {
        printf("[%d] send() failed to send entire frame!\n", socket->fd);
        return;
      }
    }
  };

  // accepts incoming GET requests with websocket upgrade headers and completes the handshake, finally
  // passing the socket over to WebSocket which handles regular websocket data framing protocol.
  struct WebSocketHandshaker {
    Socket* socket = nullptr;

    string request;
    vector<string> request_lines;

    string ws_key;

    enum {
      EXPECT_GET_REQUEST = 0,
      PARSE_GET_REQUEST,
      SEND_HANDSHAKE,
      OPEN,
      CLOSED
    } state;

    WebSocketHandshaker(Socket* socket) : socket(socket) {
      state = EXPECT_GET_REQUEST;
      ref = 1;
      socket->addRef();
    }
    ~WebSocketHandshaker() {
      reset();
      socket->release();
    }

    int ref;
    void addRef() {
      ref++;
    }
    void release() {
      if (--ref == 0)
        delete this;
    }

    operator bool() { return *socket; }

    auto get_socket() -> Socket* {
      return socket;
    }

    auto reset() -> void {
      request.reset();
      request_lines.reset();
      ws_key.reset();
    }

    auto base64_decode(const string& text) -> vector<uint8_t> {
      static const unsigned char pr2six[256] =
        {
          /* ASCII table */
          64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
          64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
          64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
          52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
          64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
          15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
          64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
          41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
          64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
          64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
          64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
          64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
          64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
          64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
          64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
          64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
        };

      vector<uint8_t> out;

      int nbytesdecoded;
      const unsigned char *bufin;
      int nprbytes;

      bufin = (const unsigned char *) text.data();
      while (pr2six[*(bufin++)] <= 63);
      nprbytes = (bufin - (const unsigned char *) text.data()) - 1;
      nbytesdecoded = ((nprbytes + 3) / 4) * 3;

      bufin = (const unsigned char *) text.data();

      while (nprbytes > 4) {
        out.append((unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4));
        out.append((unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2));
        out.append((unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]));
        bufin += 4;
        nprbytes -= 4;
      }

      /* Note: (nprbytes == 1) would be an error, so just ingore that case */
      if (nprbytes > 1) {
        out.append((unsigned char) (pr2six[*bufin] << 2 | pr2six[bufin[1]] >> 4));
      }
      if (nprbytes > 2) {
        out.append((unsigned char) (pr2six[bufin[1]] << 4 | pr2six[bufin[2]] >> 2));
      }
      if (nprbytes > 3) {
        out.append((unsigned char) (pr2six[bufin[2]] << 6 | pr2six[bufin[3]]));
      }

      nbytesdecoded -= (4 - nprbytes) & 3;
      out.resize(nbytesdecoded);

      return out;
    }

    auto handshake() -> WebSocket* {
      if (state == CLOSED) {
        return nullptr;
      }

      if (state == EXPECT_GET_REQUEST) {
        // build up GET request from client:
        if (socket->recv_append(request) == 0) {
          //printf("socket closed!\n");
          reset();
          state = CLOSED;
          socket->close(false);
          return nullptr;
        }

        auto s = request.size();
        if (s >= 65536) {
          // too big, toss out.
          //printf("request too big!\n");
          reset();
          state = EXPECT_GET_REQUEST;
          return nullptr;
        }

        // Find the end of the request headers:
        auto found = request.find("\r\n\r\n");
        if (found) {
          auto length = found.get();
          request_lines = slice(request, 0, length).split("\r\n");
          state = PARSE_GET_REQUEST;
        }
      }

      if (state == PARSE_GET_REQUEST) {
        string response = "HTTP/1.1 400 Bad Request\r\n\r\n";

        // parse HTTP request:
        vector<string> headers;
        vector<string> line;
        int len;

        bool req_host = false;
        bool req_connection = false;
        bool req_upgrade = false;
        bool req_ws_key = false;
        bool req_ws_version = false;

        // if no lines in request, bail:
        if (request_lines.size() == 0) {
          response = "HTTP/1.1 400 Bad Request\r\n\r\nMissing HTTP request line";
          goto bad_request;
        }

        // check first line is like `GET ... HTTP/1.1`:
        line = request_lines[0].split(" ");
        if (line.size() != 3) {
          response = "HTTP/1.1 400 Bad Request\r\n\r\nInvalid HTTP request line; must be `[method] [resource] HTTP/1.1`";
          goto bad_request;
        }
        // really want to check if version >= 1.1
        if (line[2] != "HTTP/1.1") {
          response = "HTTP/1.1 400 Bad Request\r\n\r\nInvalid HTTP request line; must be HTTP/1.1 version";
          goto bad_request;
        }
        if (line[0] != "GET") {
          response = "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET\r\n\r\n";
          goto bad_request;
        }

        // check all headers for websocket upgrade requirements:
        len = request_lines.size();
        for (int i = 1; i < len; i++) {
          auto split = request_lines[i].split(":", 1);
          auto header = split[0].downcase().strip();
          auto value = split[1].strip();

          if (header == "host") {
            req_host = true;
          } else if (header == "upgrade") {
            if (!value.contains("websocket")) {
              response = "HTTP/1.1 400 Bad Request\r\n\r\nUpgrade header must be 'websocket'";
              goto bad_request;
            }
            req_upgrade = true;
          } else if (header == "connection") {
            if (!value.contains("Upgrade")) {
              response = "HTTP/1.1 400 Bad Request\r\n\r\nConnection header must be 'Upgrade'";
              goto bad_request;
            }
            req_connection = true;
          } else if (header == "sec-websocket-key") {
            ws_key = value;
            // We don't _need_ to do this but it's nice to check:
            auto decoded = base64_decode(ws_key);
            if (decoded.size() != 16) {
              response = "HTTP/1.1 400 Bad Request\r\n\r\nSec-WebSocket-Key header must base64 decode to 16 bytes";
              goto bad_request;
            }
            req_ws_key = true;
          } else if (header == "sec-websocket-version") {
            if (value != "13") {
              response = "HTTP/1.1 426 Upgrade Required\r\nSec-WebSocket-Version: 13\r\n\r\nSec-WebSocket-Version header must be '13'";
              goto response_sent;
            }
            req_ws_version = true;
          }
        }

        // make sure we have the minimal set of HTTP headers for upgrading to websocket:
        if (!req_host || !req_upgrade || !req_connection || !req_ws_key || !req_ws_version) {
          vector<string> missing;
          if (!req_host) missing.append("Host");
          if (!req_upgrade) missing.append("Upgrade");
          if (!req_connection) missing.append("Connection");
          if (!req_ws_key) missing.append("Sec-WebSocket-Key");
          if (!req_ws_version) missing.append("Sec-WebSocket-Version");
          response = string{
            string("HTTP/1.1 426 Upgrade Required\r\n"
                   "Sec-WebSocket-Version: 13\r\n\r\n"
                   "Missing one or more required HTTP headers for WebSocket upgrade: "),
            missing.merge(", ")
          };
          goto bad_request;
        }

        // we have all the required headers and they are valid:
        goto next_state;

        bad_request:
        socket->send_buffer(response);
        response_sent:
        reset();
        state = EXPECT_GET_REQUEST;
        return nullptr;

        next_state:
        state = SEND_HANDSHAKE;
      }

      if (state == SEND_HANDSHAKE) {
        auto concat = string{ws_key, string("258EAFA5-E914-47DA-95CA-C5AB0DC85B11")};
        auto sha1 = nall::Hash::SHA1(concat).output();
        auto enc = nall::Encode::Base64(sha1);
        auto buf = string{
          string(
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Connection: Upgrade\r\n"
            "Upgrade: websocket\r\n"
            "Sec-WebSocket-Accept: "
          ),
          // base64 encoded sha1 hash here
          enc,
          string("\r\n\r\n")
        };
        socket->send_buffer(buf);

        state = OPEN;
        return new WebSocket(socket);
      }

      return nullptr;
    }
  };

  static WebSocketHandshaker *create_web_socket_handshaker(Socket* socket) {
    return new WebSocketHandshaker(socket);
  }

  struct WebSocketServer {
    Socket* socket = nullptr;
    string host;
    string port;
    string resource;

    bool valid;

    WebSocketServer(string uri) {
      ref = 1;
      valid = false;

      // make sure we have a script context:
      asIScriptContext *ctx = asGetActiveContext();
      if (!ctx) {
        printf("WebSocketServer requires a current script context\n");
        return;
      }
      asIScriptEngine* engine = ctx->GetEngine();
      if (!engine) {
        printf("WebSocketServer requires a current script engine instance\n");
        return;
      }
      asITypeInfo* typeInfo = engine->GetTypeInfoByDecl("array<net::WebSocket@>");
      if (!typeInfo) {
        printf("WebSocketServer could not find script TypeInfo for `array<net::WebSocket@>`\n");
        return;
      }

      // use a default uri:
      if (!uri) uri = "ws://localhost:8080";

      // Split the absolute URI `ws://host:port/path.../path...` into `ws:` and `host:port/path.../path...`
      auto scheme_rest = uri.split("//", 1);
      if (scheme_rest.size() == 0) {
        asGetActiveContext()->SetException("uri must be an absolute URI");
        return;
      }
      string scheme = scheme_rest[0];
      if (scheme != "ws:") {
        asGetActiveContext()->SetException("uri scheme must be `ws:`");
        return;
      }

      // Split the remaining `host:port/path.../path...` by the first '/':
      auto rest = scheme_rest[1];
      auto host_port_path = rest.split("/", 1);
      if (host_port_path.size() == 0) host_port_path.append(rest);

      // TODO: accommodate IPv6 addresses (e.g. `[::1]`)
      host = host_port_path[0];
      auto host_port = host_port_path[0].split(":");
      if (host_port.size() == 2) {
        host = host_port[0];
        port = host_port[1];
      } else {
        port = "80";
      }

      resource = "/";
      if (host_port_path.size() == 2) {
        resource = string{"/", host_port_path[1]};
      }

      // resolve listening address:
      auto addr = resolve_tcp(&host, &port);
      if (!*addr) {
        delete addr;
        return;
      }

      // create listening socket to accept TCP connections on:
      socket = create_socket(addr);
      if (!*socket) return;

      // start listening for connections:
      if (socket->bind(addr) < 0) return;
      if (socket->listen(32) < 0) return;

      // create `array<WebSocket@>` for clients:
      clients = CScriptArray::Create(typeInfo);
      valid = true;
    }
    ~WebSocketServer() {
      if (socket) {
        delete socket;
        socket = nullptr;
      }
      valid = false;
    }

    int ref;
    void addRef() {
      ref++;
    }
    void release() {
      if (--ref == 0)
        delete this;
    }

    operator bool() { return valid; }

    vector<WebSocketHandshaker*> handshakers;
    CScriptArray* clients;

    auto process() -> int {
      int count = 0;

      // accept new connections:
      Socket* accepted = nullptr;
      while ((accepted = socket->accept()) != nullptr) {
        handshakers.append(new WebSocketHandshaker(accepted));
      }

      // handle requests for each open connection:
      for (auto it = handshakers.rbegin(); it != handshakers.rend(); ++it) {
        // if socket closed, remove it:
        if (!*(*it)) {
          handshakers.removeByIndex(it.offset());
          continue;
        }

        // advance the websocket handshake / upgrade process from HTTP requests to WebSockets:
        WebSocket* ws = nullptr;
        // TODO: accept only resource path
        // TODO: if allowing multiple resources, need a shared instance per host:port
        if ((ws = (*it)->handshake()) != nullptr) {
          clients->InsertLast(&ws);
          handshakers.removeByIndex(it.offset());
          count++;
        }
      }

      return count;
    }

    auto get_clients() -> CScriptArray* { return clients; }
  };

  auto create_web_socket_server(string *uri) -> WebSocketServer* {
    auto server = new WebSocketServer(*uri);
    if (!*server) {
      delete server;
      return nullptr;
    }
    return server;
  }
}

auto RegisterNet(asIScriptEngine *e) -> void {
  int r;

  r = e->SetDefaultNamespace("net"); assert(r >= 0);

  // error reporting functions:
  r = e->RegisterGlobalFunction("string get_is_error() property", asFUNCTION(Net::get_is_error), asCALL_CDECL); assert( r >= 0 );
  r = e->RegisterGlobalFunction("string get_error_code() property", asFUNCTION(Net::get_error_code), asCALL_CDECL); assert( r >= 0 );
  r = e->RegisterGlobalFunction("string get_error_text() property", asFUNCTION(Net::get_error_text), asCALL_CDECL); assert( r >= 0 );
  r = e->RegisterGlobalFunction("bool throw_if_error()", asFUNCTION(Net::exception_thrown), asCALL_CDECL); assert(r >= 0 );

  // Address type:
  r = e->RegisterObjectType("Address", 0, asOBJ_REF | asOBJ_NOCOUNT); assert(r >= 0);
  r = e->RegisterGlobalFunction("Address@ resolve_tcp(const string &in host, const string &in port)", asFUNCTION(Net::resolve_tcp), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterGlobalFunction("Address@ resolve_udp(const string &in host, const string &in port)", asFUNCTION(Net::resolve_udp), asCALL_CDECL); assert(r >= 0);
  // TODO: try opImplCast
  r = e->RegisterObjectMethod("Address", "bool get_is_valid() property", asMETHOD(Net::Address, operator bool), asCALL_THISCALL); assert( r >= 0 );

  r = e->RegisterObjectType("Socket", 0, asOBJ_REF); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Socket", asBEHAVE_FACTORY, "Socket@ f(Address @addr)", asFUNCTION(Net::create_socket), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("Socket", asBEHAVE_ADDREF, "void f()", asMETHOD(Net::Socket, addRef), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("Socket", asBEHAVE_RELEASE, "void f()", asMETHOD(Net::Socket, release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Socket", "bool get_is_valid() property", asMETHOD(Net::Socket, operator bool), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Socket", "void connect(Address@ addr)", asMETHOD(Net::Socket, connect), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Socket", "void bind(Address@ addr)", asMETHOD(Net::Socket, bind), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Socket", "void listen(int backlog)", asMETHOD(Net::Socket, listen), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Socket", "Socket@ accept()", asMETHOD(Net::Socket, accept), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Socket", "int recv(int offs, int size, array<uint8> &inout buffer)", asMETHOD(Net::Socket, recv), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Socket", "int send(int offs, int size, array<uint8> &inout buffer)", asMETHOD(Net::Socket, send), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Socket", "int recvfrom(int offs, int size, array<uint8> &inout buffer)", asMETHOD(Net::Socket, recvfrom), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("Socket", "int sendto(int offs, int size, array<uint8> &inout buffer, const Address@ addr)", asMETHOD(Net::Socket, sendto), asCALL_THISCALL); assert( r >= 0 );

  r = e->RegisterObjectType("WebSocketMessage", 0, asOBJ_REF); assert(r >= 0);
  r = e->RegisterObjectBehaviour("WebSocketMessage", asBEHAVE_FACTORY, "WebSocketMessage@ f(uint8 opcode)", asFUNCTION(Net::create_web_socket_message), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("WebSocketMessage", asBEHAVE_ADDREF, "void f()", asMETHOD(Net::WebSocketMessage, addRef), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("WebSocketMessage", asBEHAVE_RELEASE, "void f()", asMETHOD(Net::WebSocketMessage, release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocketMessage", "uint8 get_opcode() property", asMETHOD(Net::WebSocketMessage, get_opcode), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocketMessage", "void set_opcode(uint8 value) property", asMETHOD(Net::WebSocketMessage, set_opcode), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocketMessage", "string &as_string()", asMETHOD(Net::WebSocketMessage, as_string), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocketMessage", "array<uint8> &as_array()", asMETHOD(Net::WebSocketMessage, as_array), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocketMessage", "void set_payload_as_string(string &in value) property", asMETHOD(Net::WebSocketMessage, set_payload_as_string), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocketMessage", "void set_payload_as_array(array<uint8> &in value) property", asMETHOD(Net::WebSocketMessage, set_payload_as_array), asCALL_THISCALL); assert( r >= 0 );

  r = e->RegisterObjectType("WebSocket", 0, asOBJ_REF); assert(r >= 0);
  r = e->RegisterObjectBehaviour("WebSocket", asBEHAVE_ADDREF, "void f()", asMETHOD(Net::WebSocket, addRef), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("WebSocket", asBEHAVE_RELEASE, "void f()", asMETHOD(Net::WebSocket, release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocket", "WebSocketMessage@ process()", asMETHOD(Net::WebSocket, process), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocket", "void send(WebSocketMessage@ msg)", asMETHOD(Net::WebSocket, send), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocket", "bool get_is_valid() property", asMETHOD(Net::WebSocket, operator bool), asCALL_THISCALL); assert( r >= 0 );

  r = e->RegisterObjectType("WebSocketHandshaker", 0, asOBJ_REF); assert(r >= 0);
  r = e->RegisterObjectBehaviour("WebSocketHandshaker", asBEHAVE_FACTORY, "WebSocketHandshaker@ f(Socket@ socket)", asFUNCTION(Net::create_web_socket_handshaker), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("WebSocketHandshaker", asBEHAVE_ADDREF, "void f()", asMETHOD(Net::WebSocketHandshaker, addRef), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("WebSocketHandshaker", asBEHAVE_RELEASE, "void f()", asMETHOD(Net::WebSocketHandshaker, release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocketHandshaker", "Socket@ get_socket() property", asMETHOD(Net::WebSocketHandshaker, get_socket), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocketHandshaker", "WebSocket@ handshake()", asMETHOD(Net::WebSocketHandshaker, handshake), asCALL_THISCALL); assert( r >= 0 );    r = e->RegisterObjectType("WebSocketHandshaker", 0, asOBJ_REF); assert(r >= 0);

  r = e->RegisterObjectType("WebSocketServer", 0, asOBJ_REF); assert(r >= 0);
  r = e->RegisterObjectBehaviour("WebSocketServer", asBEHAVE_FACTORY, "WebSocketServer@ f(string &in uri)", asFUNCTION(Net::create_web_socket_server), asCALL_CDECL); assert(r >= 0);
  r = e->RegisterObjectBehaviour("WebSocketServer", asBEHAVE_ADDREF, "void f()", asMETHOD(Net::WebSocketServer, addRef), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectBehaviour("WebSocketServer", asBEHAVE_RELEASE, "void f()", asMETHOD(Net::WebSocketServer, release), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocketServer", "array<WebSocket@> &get_clients() property", asMETHOD(Net::WebSocketServer, get_clients), asCALL_THISCALL); assert( r >= 0 );
  r = e->RegisterObjectMethod("WebSocketServer", "int process()", asMETHOD(Net::WebSocketServer, process), asCALL_THISCALL); assert( r >= 0 );
}
