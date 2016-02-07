#include "http_server.h"
#include <cstdio>
#include <cassert>
#include <algorithm>
#include <cstring>

namespace HTTP {

  // -------------------------------------------------------
  char* VBytes::base() {
    return &(*this->begin());
  }

  bool VBytes::send(TSocket fd) {
    auto nbytes_sent = ::send(fd, base(), (int)size(), 0);
    return nbytes_sent == size();
  }

  bool VBytes::recv(TSocket fd) {
    resize(capacity());
    auto nbytes_read = ::recv(fd, base(), (int)size(), 0);
    if (nbytes_read > 0)
      resize(nbytes_read);
    return nbytes_read > 0;
  }

  void VBytes::format(const char* fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    resize(512);
    while (true) {
      int n = vsnprintf(base(), size(), fmt, argp);   // Copies N characters // vsnprintf_s could be used, has the same behavior and interrupts the program
      if (n >= size()) {
        resize(n + 1);
      }
      else {
        resize(n);
        break;
      }
    }
    va_end(argp);
  }

  bool VBytes::read(const char* file) {
    FILE *f = fopen(file, "rb");
    if (!f)
      return false;
    fseek(f, 0, SEEK_END);
    auto sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    resize(sz);
    fread(&(*this->begin()), 1, sz, f);
    fclose(f);
    return true;
  }

  // -------------------------------------------------------
  void CBaseServer::VSockets::remove(TSocket s) {
    ::closesocket(s);
    auto it = std::find(begin(), end(), s);
    assert(it != end());
    erase(it);
  } 

  // -------------------------------------------------------
  bool CBaseServer::TRequest::parse(VBytes& buf) {

    method = UNSUPPORTED;

    char* bol = buf.base();               // begin of line
    const char* eob = bol + buf.size();   // end of buffer
    while (true) {
      auto eol = bol;                     // end of line
      while (!(eol[0] == '\r' && eol[1] == '\n') && (eol < eob))
        eol++;
      if (eol == bol || eol >= eob)      // An empty line of end of buffer found
        break;
      *eol = 0x00;                        // To make easier to parse using str* funcs

      if (strncmp(bol, "GET ", 4) == 0) {
        method = GET;
        url = std::string(bol + 4);
        // Drop the HTTP/1.1
        auto idx = url.find_last_of(' ');
        if (idx != std::string::npos)
          url.resize(idx);
        printf(">> GET %s\n", url.c_str());
      }
      else {
        // Other headers
        //printf("Line:%s\n", bol);
      }

      bol = eol + 2;                      // Skip \r and \n
    }
    return !url.empty();
  }

  // -------------------------------------------------------
  bool CBaseServer::createServer(int port) {
    server = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0)
      return false;

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(server, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
      return false;

    if (listen(server, 5) < 0)
      return false;

    return true;
  }

  // -------------------------------------------------------
  bool CBaseServer::TActivity::wait(VSockets& sockets, unsigned timeout_usecs) {

    FD_ZERO(&fds);
    auto max_fd = sockets[0];
    for (auto s : sockets) {
      if (s > max_fd)
        max_fd = s;
      FD_SET(s, &fds);
    }

    struct timeval timeout;
    timeout.tv_sec = timeout_usecs / 1000000;
    timeout.tv_usec = timeout_usecs % 1000000;
    auto nready = select((int)max_fd + 1, &fds, NULL, NULL, &timeout);
    if (nready <= 0)
      return false;

    ready_to_read.clear();
    for (auto s : sockets) {
      if (FD_ISSET(s, &fds)) {
        ready_to_read.push_back(s);
        nready--;
        if (!nready)
          break;
      }
    }
    return true;
  }

  // -------------------------------------------------------
  TSocket CBaseServer::acceptNewClient() {
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    auto client = accept(server, (struct sockaddr *)&client_addr, &addr_len);
    printf("New client at socket %d\n", (int)client);
    return client;
  }

  // -------------------------------------------------------
  void CBaseServer::prepare() {
    inbuf.reserve(2048);
    active_sockets.reserve(8);
    active_sockets.push_back(server);
    activity.ready_to_read.reserve(8);
  }

  // -------------------------------------------------------
  CBaseServer::~CBaseServer() {
    close();
  }

  // -------------------------------------------------------
  bool CBaseServer::open(int port) {
    if (!createServer(port))
      return false;
    prepare();
    return true;
  }

  // -------------------------------------------------------
  // Close all pending connections
  void CBaseServer::close() {
    while (!active_sockets.empty())
      active_sockets.remove(active_sockets[0]);
  }

  // -------------------------------------------------------
  // This will block for timeout_usecs at most. 0 just to poll
  bool CBaseServer::tick(unsigned timeout_usecs) {
    if (!activity.wait(active_sockets, timeout_usecs))
      return false;

    for (auto s : activity.ready_to_read) {
      if (s == server) {
        auto client = acceptNewClient();
        active_sockets.emplace_back(client);
      }
      else {
        if (!inbuf.recv(s)) {
          active_sockets.remove(s);
        }
        else {
          TRequest r;
          if (r.parse(inbuf)) {
            if (!onClientRequest(r, s))
              active_sockets.remove(s);
          }
        }
      }
    }

    return true;
  }

  // -------------------------------------------------------
  void CBaseServer::runForEver() {
    while (true) {
      if (!tick(1000000))
        printf(".\n");
    };
  }

}   // End of namespace
