#include <cstdio>
#include <cassert>
#include <algorithm>
#include <cstring>
#include "http_server.h"

// -------------------------------------------------------------------
// Enable compression by embeeding the miniz.c source code here
// Define DISABLE_MINIZ_SUPPORT to fully discard it
#if DISABLE_MINIZ_SUPPORT

bool compress( const HTTP::VBytes &src, HTTP::VBytes &dst ) {
  return false;
}

#else

#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_STDIO
#include "../miniz.h"
#include "../miniz.c"
bool compress( const HTTP::VBytes &src, HTTP::VBytes &dst ) {
  auto dst_sz = ::compressBound(src.size());
  dst.resize( dst_sz );
  auto cmp_status = ::compress((unsigned char*) dst.data(), &dst_sz, (unsigned char*) src.data(), src.size());
  if (cmp_status != Z_OK) 
    return false;
  dst.resize( dst_sz );
  return true;
}

#endif


namespace HTTP {

  // -------------------------------------------------------
  bool VBytes::send(TSocket fd) const {
    auto nbytes_sent = ::send(fd, data(), (int)size(), 0);
    return nbytes_sent == size();
  }

  bool VBytes::recv(TSocket fd) {
    resize(capacity());
    auto nbytes_read = ::recv(fd, data(), (int)size(), 0);
    if (nbytes_read > 0)
      resize(nbytes_read);
    return nbytes_read > 0;
  }

  void VBytes::format(const char* fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    resize(512);
    while (true) {
      int n = vsnprintf(data(), size(), fmt, argp);   // Copies N characters // vsnprintf_s could be used, has the same behavior and interrupts the program
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
  const char* CBaseServer::TRequest::getHeader( const char* title ) const {
    for( int i=0; i<nlines; ++i ) {
      if( strcmp( title, lines[i].title ) == 0 )
        return lines[i].value;
    }
    return nullptr;
  }

  // -------------------------------------------------------
  bool CBaseServer::TRequest::parse(VBytes& buf, bool trace) {

    method = UNSUPPORTED;

    char* bol = buf.data();               // begin of line
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

        if( trace ) printf("request.get: %s\n", url.c_str());
      }
      else {

        // Split header in title and value
        auto title = bol;
        auto value = bol;
        while( *value != ':' && *value != 0x00 ) 
          ++value;
        if( *value == ':' ) {
          *value++ = 0x00;
          if( *value == ' ' )
            value++;
        }
  
        if( nlines < max_header_lines ) {
          auto& h = lines[ nlines ];
          h.title = title;
          h.value = value;
          nlines++;
        }

        // Other headers
        if( trace ) printf("request.header: '%s' => '%s'\n", title, value);
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
    socklen_t addr_len = sizeof(client_addr);
    auto client = ::accept(server, (struct sockaddr *)&client_addr, &addr_len);
    if( trace ) printf("http_server.New client at socket %d\n", (int)client);
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
          if (r.parse(inbuf, trace)) {
            if (!onClientRequest(r, s))
              active_sockets.remove(s);
          }
        }
      }
    }

    return true;
  }

  // -------------------------------------------------------
  void CBaseServer::sendAnswer( 
    TSocket client, 
    const VBytes& answer_data, 
    const char* content_type, 
    const char* content_encoding 
  ) {

    assert( content_type );

    time_t raw_time;
    time(&raw_time);
    struct tm* time_info = gmtime(&raw_time);

    // If the user specifies an encoding type, added the corresponding header answer
    char extra_header[64];
    if( content_encoding ) 
      sprintf( extra_header, "Content-Encoding: %s\r\n", content_encoding );

    // Format the full header
    VBytes header;
    header.format(
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: %d\r\n"
      "Content-Type: %s\r\n"
      "Date: %s GMT\r\n"
      "%s"
      "\r\n"
      , (int)answer_data.size()
      , content_type
      , asctime(time_info)
      , content_encoding ? extra_header : ""
      );
    header.send(client);
    answer_data.send(client);

  }

  // -------------------------------------------------------
  void CBaseServer::compressAndSendAnswer( 
    TSocket client, 
    const VBytes& answer_data, 
    const char* content_type
  ) {
    VBytes zans;
    if( compress( answer_data, zans ) ) {
      if( trace ) printf( "Compressing answer from %ld to %ld bytes\n", answer_data.size(), zans.size());
      sendAnswer( client, zans, content_type, "deflate");
    }
    else
      sendAnswer( client, answer_data, content_type );
  }

  // -------------------------------------------------------
  void CBaseServer::runForEver() {
    while (true) {
      if (!tick(1000000))
        if( trace ) printf(".\n");
    };
  }

}   // End of namespace
