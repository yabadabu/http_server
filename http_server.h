#ifndef INC_HTTP_SERVER_H_
#define INC_HTTP_SERVER_H_

#if defined( _WIN32 )

#define _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
typedef SOCKET TSocket;
#else

#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
typedef int    TSocket;
#define closesocket  close

#endif

#include <vector>
#include <sys/types.h> 
#include <ctime>
#include <string>

namespace HTTP {

// -------------------------------------------------------
struct VBytes : public std::vector<char> {
  bool send(TSocket fd) const;
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

    struct THeaderLine {
      const char* title;
      const char* value;
    };

    enum eMethod { GET, UNSUPPORTED };
    eMethod     method;
    std::string url;
    
    // Save header lines
    static const int max_header_lines = 16;
    THeaderLine lines[max_header_lines];
    int         nlines = 0;
    const char* getHeader( const char* title ) const;

    bool parse(VBytes& buf, bool trace);
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

protected:
  
  void sendAnswer( 
      TSocket client
    , const VBytes& answer_data
    , const char* content_type
    , const char* content_encoding = nullptr    // gzip, deflate, br or null
    );

  void compressAndSendAnswer( 
      TSocket client
    , const VBytes& answer_data
    , const char* content_type
    );

public:

  virtual bool onClientRequest(const TRequest& r, TSocket client) = 0;
  virtual ~CBaseServer();

  bool open(int port);
  void close();

  // This will block for timeout_usecs at most. 0 just to poll
  bool tick(unsigned timeout_usecs);
  void runForEver();
  bool trace = false;
};

}

#endif
