#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cassert>
#include <algorithm>
#include <cstring>
#include "http_server.h"

// -------------------------------------------------------------------
// Enable compression by embedding the miniz.c source code here
// This reduces the file size by 100Kb.
// Define DISABLE_MINIZ_SUPPORT to fully discard it
#if DISABLE_MINIZ_SUPPORT

bool compress( const HTTP::VBytes &src, HTTP::VBytes &dst ) {
  return false;
}

#else

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomma"

#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_STDIO
#include "miniz.h"
#include "miniz.c"

#pragma clang diagnostic pop

bool compress( const HTTP::VBytes &src, HTTP::VBytes &dst ) {
  auto dst_sz = ::compressBound((mz_ulong)src.size());
  dst.resize( dst_sz );
  auto cmp_status = ::compress((unsigned char*) dst.data(), &dst_sz, (unsigned char*) src.data(),  (mz_ulong) src.size());
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
  bool CBaseServer::TRequest::headerContains(const char* title, const char* text_in_header) const {
    auto h = getHeader(title);
    if (!h) return false;
    auto p = strstr(h, text_in_header);
    return p != nullptr;
  }

  std::string CBaseServer::TRequest::getURIParam(const char* title) const {
    std::string::size_type start = url.find(title);
    if( start == std::string::npos )
      return std::string{};
    std::string::size_type equal = url.find('=', start);
    if( equal == std::string::npos )
      return std::string{};
    equal++;
    std::string::size_type end = url.find('&', equal);
    if( end == std::string::npos )
      end = url.size();
    return url.substr(equal, end - equal);
  } 

  //   /path/to/doc?id=23&q=str. => ath/to/doc
  std::string CBaseServer::TRequest::getURLPath() const {
    std::string::size_type c = url.find('?');
    if( c == std::string::npos )
      c = url.size();
    return url.substr( 0, c );
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
    if (server < 0) {
      printf( "createServer.socket failed\n");
      return false;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(server, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
      printf( "createServer.bind failed\n");
      return false;
    }

    if (listen(server, 5) < 0) {
      printf( "createServer.listen failed\n");
      return false;
    }

    return true;
  }

  // -------------------------------------------------------
  bool CBaseServer::TActivity::wait(VSockets& sockets, unsigned timeout_usecs) {

    if( sockets.empty() )
      return false;
    
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
          r.client = s;
          if (r.parse(inbuf, trace)) {
            if (!onClientRequest(r))
              active_sockets.remove(s);
          }
        }
      }
    }

    return true;
  }

  // -------------------------------------------------------
  void CBaseServer::sendAnswer( 
    const TRequest& r,
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
    header.send(r.client);
    answer_data.send(r.client);

  }

  // -------------------------------------------------------
  void CBaseServer::compressAndSendAnswer( 
    const TRequest& r,
    const VBytes& answer_data, 
    const char* content_type
  ) {
    VBytes zans;
    if( r.headerContains("Accept-Encoding", "deflate") && compress( answer_data, zans ) ) {
      if( trace ) printf( "Compressing answer from %d to %d bytes\n", (int)answer_data.size(), (int)zans.size());
      sendAnswer(r, zans, content_type, "deflate");
    }
    else
      sendAnswer(r, answer_data, content_type );
  }

  // -------------------------------------------------------
  void CBaseServer::runForEver() {
    while (true) {
      if (!tick(1000000))
        if( trace ) printf(".\n");
    };
  }

}   // End of namespace
