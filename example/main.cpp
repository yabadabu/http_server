#include "../http_server.h"

#pragma comment(lib,"ws2_32.lib") //Winsock Library

// -------------------------------------------------------------------
using namespace HTTP;

// -------------------------------------------------------------------
class CMyServer : public CBaseServer {
  VBytes star;
  VBytes index;
  VBytes gidx;
public:
  CMyServer() {
    star.read("star.png");
    index.read("index.html");
  }
  bool onClientRequest(const TRequest& r) override {

    const char* content_type = "text/html";
    const char* content_encoding = nullptr;
    VBytes* ans;
    VBytes zans;
    if (r.url == "/") {
      ans = &index;
      content_type = "text/html";
      // Let http compress our answer 
      compressAndSendAnswer( r, *ans, content_type );
      return false;
    }
    else if (r.url == "/gidx") {
      // gzip compression (static using gzip cmd line)
      ans = &gidx;
      content_type = "text/html";
      content_encoding = "gzip";
    }
    else {
      // No compression...
      ans = &star;
      content_type = "image/png";
    }

    sendAnswer( r, *ans, content_type, content_encoding );

    return false;
  }

};

int main()
{

#ifdef WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    return -1;
#endif

  int port = 8080;
  CMyServer server;
  server.trace = true;
  if (!server.open(port)) {
    printf( "Can't start server at port %d\n", port);
    return -1;
  }
  server.runForEver();

  return 0;
}


