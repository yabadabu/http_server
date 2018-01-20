#include "../http_server.h"

#pragma comment(lib,"ws2_32.lib") //Winsock Library

// -------------------------------------------------------------------
using namespace HTTP;

class CMyServer : public CBaseServer {
  VBytes star;
  VBytes index;
public:
  CMyServer() {
    star.read("star.png");
    index.read("index.html");
  }
  bool onClientRequest(const TRequest& r, TSocket client) override {

    const char* content_type = "text/html";
    VBytes* ans;
    if (r.url == "/") {
      ans = &index;
      content_type = "text/html";
    }
    else {
      ans = &star;
      content_type = "image/png";
    }

    sendAnswer( client, *ans, content_type );
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

  CMyServer server;
  if (!server.open(8080))
    return -1;
  server.runForEver();

  return 0;
}


