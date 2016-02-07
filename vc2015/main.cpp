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

    time_t raw_time;
    time(&raw_time);
    struct tm* time_info = gmtime(&raw_time);

    VBytes header;
    header.format(
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: %d\r\n"
      "Content-Type: %s\r\n"
      "Date: %s GMT\r\n"
      "Last - Modified: Wed, 22 Jul 2009 19 : 15 : 56 GMT\r\n"
      "\r\n"
      , (int)ans->size()
      , content_type
      , asctime(time_info)
      );
    header.send(client);
    ans->send(client);
    return false;
  }

};

int main()
{
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    return -1;

  CMyServer server;
  if (!server.open(8080))
    return -1;
  server.runForEver();

  return 0;
}


