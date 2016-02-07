#ifndef INC_HTTP_SERVER_H_
#define INC_HTTP_SERVER_H_

#if defined( _WIN32 )
#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
typedef SOCKET TSocket;
#else
typedef int    TSocket;
#endif

#include <vector>
#include <sys/types.h> 
#include <ctime>

namespace HTTP {

// -------------------------------------------------------
struct VBytes : public std::vector<char> {
  char* base();
  bool send(TSocket fd);
  bool recv(TSocket fd);
  void format(const char* fmt, ...);
  bool read(const char* file);
};

class CBaseServer {
  
  class VSockets : public std::vector<TSocket> {
  public:
    void remove(TSocket s);
  };

public:

  // -------------------------------------------------------
  struct TRequest {
    enum eMethod { GET, UNSUPPORTED };
    eMethod     method;
    std::string url;
    bool parse(VBytes& buf);
  };

private:

  // -------------------------------------------------------
  struct TActivity {
    fd_set   fds;
    VSockets ready_to_read;
    bool wait(VSockets& sockets, unsigned timeout_usecs);
  };
  
  // -------------------------------------------------------
  bool    createServer(int port);
  TSocket acceptNewClient();
  void    prepare();

  // -------------------------
  TSocket   server;
  VSockets  active_sockets;
  VBytes    inbuf;
  TActivity activity;

public:

  virtual bool onClientRequest(const TRequest& r, TSocket client) = 0;
  virtual ~CBaseServer();

  bool open(int port);
  void close();

  // This will block for timeout_usecs at most. 0 just to poll
  bool tick(unsigned timeout_usecs);
  void runForEver();
};

}

#endif
